#include "BackupEngine.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#ifdef Q_OS_UNIX
#include <grp.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
using Path = std::filesystem::path;
namespace fs = std::filesystem;

struct Header
{
    quint64 manifestOffset = 0;
    quint64 manifestLength = 0;
};

const QByteArray kPackageMagic = "FBS1";
constexpr quint32 kPackageVersion = 1;

QString normalizeRelative(const Path &root, const Path &current)
{
    std::error_code ec;
    Path relative = fs::relative(current, root, ec);
    if (ec)
    {
        relative = current.filename();
    }
    QString text = QString::fromStdString(relative.generic_string());
    if (text == ".")
    {
        text.clear();
    }
    return text;
}

FileKind detectKind(const fs::file_status &status)
{
    switch (status.type())
    {
    case fs::file_type::regular:
        return FileKind::Regular;
    case fs::file_type::directory:
        return FileKind::Directory;
    case fs::file_type::symlink:
        return FileKind::Symlink;
    case fs::file_type::block:
        return FileKind::Block;
    case fs::file_type::character:
        return FileKind::Character;
    case fs::file_type::fifo:
        return FileKind::Fifo;
    case fs::file_type::socket:
        return FileKind::Socket;
    default:
        return FileKind::Unknown;
    }
}

bool kindHasData(FileKind kind)
{
    return kind == FileKind::Regular;
}

QString sanitizedName(const QString &text)
{
    QString name = text;
    if (name.isEmpty())
    {
        name = "backup";
    }
    for (int i = 0; i < name.size(); ++i)
    {
        const QChar ch = name.at(i);
        if (!(ch.isLetterOrNumber() || ch == '_' || ch == '-' || ch == '.'))
        {
            name[i] = '_';
        }
    }
    return name;
}

#ifdef Q_OS_UNIX
bool readStat(const QString &path, struct stat &buffer, bool followSymlink)
{
    QByteArray pathBytes = QFile::encodeName(path);
    if (followSymlink)
    {
        return ::stat(pathBytes.constData(), &buffer) == 0;
    }
    return ::lstat(pathBytes.constData(), &buffer) == 0;
}

QString ownerName(uid_t id)
{
    if (struct passwd *pwd = ::getpwuid(id))
    {
        return QString::fromLocal8Bit(pwd->pw_name);
    }
    return {};
}

QString groupName(gid_t id)
{
    if (struct group *grp = ::getgrgid(id))
    {
        return QString::fromLocal8Bit(grp->gr_name);
    }
    return {};
}
#endif

QByteArray sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

void applyPermissions(const QString &path, QFile::Permissions permissions)
{
    QFile file(path);
    file.setPermissions(permissions);
}

#ifdef Q_OS_UNIX
void applyOwnership(const QString &path, quint32 ownerId, quint32 groupId, bool followSymlink)
{
    QByteArray encoded = QFile::encodeName(path);
    if (followSymlink)
    {
        ::chown(encoded.constData(), static_cast<uid_t>(ownerId), static_cast<gid_t>(groupId));
    }
    else
    {
        ::lchown(encoded.constData(), static_cast<uid_t>(ownerId), static_cast<gid_t>(groupId));
    }
}

void applyTimestamps(const QString &path, const FileRecord &record, bool followSymlink)
{
    struct timespec times[2];
    times[0].tv_sec = record.accessedAt;
    times[0].tv_nsec = 0;
    times[1].tv_sec = record.modifiedAt;
    times[1].tv_nsec = 0;
    QByteArray encoded = QFile::encodeName(path);
#if defined(__APPLE__)
    ::utimensat(AT_FDCWD, encoded.constData(), times, followSymlink ? 0 : AT_SYMLINK_NOFOLLOW);
#else
    ::utimensat(AT_FDCWD, encoded.constData(), times, followSymlink ? 0 : AT_SYMLINK_NOFOLLOW);
#endif
}
#endif

bool ensureParentDir(const QString &filePath)
{
    QFileInfo info(filePath);
    QDir dir;
    return dir.mkpath(info.path());
}

QString joinPath(const QString &base, const QString &relative)
{
    QDir dir(base);
    return dir.filePath(relative);
}

} // namespace

void BackupEngine::log(const BackupEngineCallbacks &callbacks, const QString &message) const
{
    if (callbacks.log)
    {
        callbacks.log(message);
    }
}

void BackupEngine::reportProgress(const BackupEngineCallbacks &callbacks, int current, int total) const
{
    if (callbacks.progress)
    {
        callbacks.progress(current, total);
    }
}

BackupResult BackupEngine::performBackup(const BackupOptions &options, const BackupEngineCallbacks &callbacks) const
{
    BackupResult result;
    QFileInfo sourceInfo(options.sourcePath);
    if (!sourceInfo.exists())
    {
        result.error = QString("源路径不存在: %1").arg(options.sourcePath);
        return result;
    }
    if (options.destinationPath.isEmpty())
    {
        result.error = "请提供备份目标路径";
        return result;
    }
    if (options.encrypt && options.password.isEmpty())
    {
        result.error = "启用加密时必须提供密码";
        return result;
    }

    QDir destinationDir(options.destinationPath);
    if (!destinationDir.exists() && !destinationDir.mkpath("."))
    {
        result.error = QString("无法创建目标目录: %1").arg(options.destinationPath);
        return result;
    }

    const QString name = sanitizedName(options.backupName.isEmpty() ? sourceInfo.fileName() : options.backupName);
    const QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss");
    const QString identifier = QString("%1_%2").arg(timestamp, name);

    BackupManifest manifest;
    manifest.backupId = identifier;
    manifest.rootName = sourceInfo.isDir() ? sourceInfo.fileName() : sourceInfo.fileName();
    if (manifest.rootName.isEmpty())
    {
        manifest.rootName = "root";
    }
    manifest.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    manifest.sourcePath = sourceInfo.absoluteFilePath();
    manifest.storageMode = options.package ? StorageMode::Package : StorageMode::Directory;
    manifest.compressed = options.compress;
    manifest.encrypted = options.encrypt;
    manifest.preserveMetadata = options.preserveMetadata;
    manifest.includeSpecialFiles = options.includeSpecialFiles;
    manifest.verificationEnabled = options.verify;

    QString backupLocation;
    QString dataDirectory;
    if (options.package)
    {
        backupLocation = destinationDir.filePath(identifier + ".fbk");
        manifest.packagePath = backupLocation;
    }
    else
    {
        backupLocation = destinationDir.filePath(identifier);
        dataDirectory = QDir(backupLocation).filePath(manifest.dataRelativePath);
        manifest.basePath = backupLocation;
        manifest.dataDirectory = dataDirectory;
        if (!ensureDirectory(backupLocation, result.error))
        {
            result.error = QString("无法创建备份目录: %1").arg(backupLocation);
            return result;
        }
        if (!ensureDirectory(dataDirectory, result.error))
        {
            result.error = QString("无法创建数据目录: %1").arg(dataDirectory);
            return result;
        }
    }

    Path rootPath = Path(options.sourcePath.toStdString());
    std::error_code ec;
    if (!fs::exists(rootPath))
    {
        result.error = "源路径不存在";
        return result;
    }

    std::vector<Path> paths;
    if (fs::is_directory(rootPath))
    {
        fs::directory_options dirOptions = fs::directory_options::skip_permission_denied;
        try
        {
            for (fs::recursive_directory_iterator it(rootPath, dirOptions), end; it != end; ++it)
            {
                paths.push_back(it->path());
            }
        }
        catch (const std::exception &e)
        {
            result.error = QString("遍历源目录失败: %1").arg(e.what());
            return result;
        }
    }
    else
    {
        paths.push_back(rootPath);
    }

    const int totalItems = static_cast<int>(paths.size());
    int currentItem = 0;

    QFile packageFile;
    if (options.package)
    {
        packageFile.setFileName(backupLocation);
        if (!packageFile.open(QIODevice::WriteOnly))
        {
            result.error = QString("无法创建备份文件: %1").arg(backupLocation);
            return result;
        }
        QDataStream stream(&packageFile);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData(kPackageMagic.constData(), kPackageMagic.size());
        stream << kPackageVersion;
        stream << quint64(0);
        stream << quint64(0);
    }

    auto writeDataToDirectory = [&](const QString &relativePath, const QByteArray &storedData) -> bool {
        if (relativePath.isEmpty())
        {
            return false;
        }
        const QString targetPath = joinPath(dataDirectory, relativePath);
        if (!ensureParentDir(targetPath))
        {
            return false;
        }
        QSaveFile file(targetPath);
        if (!file.open(QIODevice::WriteOnly))
        {
            return false;
        }
        if (file.write(storedData) != storedData.size())
        {
            return false;
        }
        return file.commit();
    };

    for (const auto &path : paths)
    {
        ++currentItem;
        reportProgress(callbacks, currentItem, std::max(totalItems, 1));

        fs::file_status status = fs::symlink_status(path, ec);
        if (ec)
        {
            log(callbacks, QString("跳过无法访问的条目: %1").arg(QString::fromStdString(path.string())));
            continue;
        }

        FileKind kind = detectKind(status);
        if (!options.includeSpecialFiles &&
            (kind == FileKind::Block || kind == FileKind::Character || kind == FileKind::Fifo || kind == FileKind::Socket))
        {
            continue;
        }

        FileRecord record;
        record.relativePath = normalizeRelative(rootPath, path);
        if (record.relativePath.isEmpty())
        {
            if (fs::is_directory(path))
            {
                continue;
            }
            record.relativePath = QString::fromStdString(path.filename().string());
            if (record.relativePath.isEmpty())
            {
                record.relativePath = sourceInfo.fileName();
            }
        }
        record.kind = kind;
        record.hasData = kindHasData(kind);
        record.size = 0;
        ec.clear();
        if (fs::is_regular_file(path, ec))
        {
            ec.clear();
            std::uintmax_t fileSize = fs::file_size(path, ec);
            if (!ec)
            {
                record.size = static_cast<quint64>(fileSize);
            }
        }

#ifdef Q_OS_UNIX
        struct stat st
        {
        };
        if (readStat(QString::fromStdString(path.string()), st, kind != FileKind::Symlink))
        {
#if defined(__APPLE__)
            record.createdAt = st.st_birthtime ? st.st_birthtime : st.st_ctime;
#else
            record.createdAt = st.st_ctime;
#endif
            record.modifiedAt = st.st_mtime;
            record.accessedAt = st.st_atime;
            record.permissions = static_cast<quint32>(st.st_mode & 07777);
            record.ownerId = static_cast<quint32>(st.st_uid);
            record.groupId = static_cast<quint32>(st.st_gid);
            record.ownerName = ownerName(st.st_uid);
            record.groupName = groupName(st.st_gid);
            record.specialDevice = static_cast<quint64>(st.st_rdev);
            if (!record.hasData)
            {
                record.size = static_cast<quint64>(st.st_size);
            }
        }
#else
        record.permissions = static_cast<quint32>(QFile(QString::fromStdString(path.string())).permissions());
#endif

        if (kind == FileKind::Symlink)
        {
            record.symlinkTarget = QFileInfo(QString::fromStdString(path.string())).symLinkTarget();
        }

        if (record.hasData)
        {
            QFile file(QString::fromStdString(path.string()));
            if (!file.open(QIODevice::ReadOnly))
            {
                log(callbacks, QString("无法读取文件: %1").arg(record.relativePath));
                continue;
            }
            QByteArray data = file.readAll();
            record.size = static_cast<quint64>(data.size());
            record.hash = QString::fromLatin1(sha256(data));
            result.totalBytes += record.size;
            QByteArray storedData = data;
            if (options.compress)
            {
                storedData = compressData(storedData);
            }
            if (options.encrypt)
            {
                storedData = encrypt(storedData, options.password);
            }
            record.storedSize = static_cast<quint64>(storedData.size());
            record.storedHash = QString::fromLatin1(sha256(storedData));

            if (options.package)
            {
                record.dataOffset = static_cast<quint64>(packageFile.pos());
                if (packageFile.write(storedData) != storedData.size())
                {
                    result.error = "写入备份包失败";
                    return result;
                }
            }
            else
            {
                record.storedRelativePath = record.relativePath;
                if (!writeDataToDirectory(record.relativePath, storedData))
                {
                    result.error = QString("写入备份文件失败: %1").arg(record.relativePath);
                    return result;
                }
            }
        }

        manifest.files.push_back(record);
        ++result.processedFiles;
    }

    manifest.totalFiles = manifest.files.size();
    manifest.totalBytes = result.totalBytes;

    QString error;
    if (options.package)
    {
        if (!finalizePackage(packageFile, manifest, error))
        {
            result.error = error;
            return result;
        }
    }
    else
    {
        if (!writeManifestToDirectory(backupLocation, manifest, error))
        {
            result.error = error;
            return result;
        }
    }

    result.success = true;
    result.location = backupLocation;
    manifest.manifestPath = options.package ? backupLocation : QDir(backupLocation).filePath("manifest.json");
    manifest.packagePath = options.package ? backupLocation : QString();
    manifest.basePath = options.package ? QFileInfo(backupLocation).absolutePath() : backupLocation;
    manifest.dataDirectory = options.package ? QString() : dataDirectory;

    if (options.verify)
    {
        VerifyOptions verifyOptions;
        verifyOptions.backupPath = backupLocation;
        verifyOptions.password = options.password;
        VerifyResult verifyResult = verify(verifyOptions, callbacks);
        if (!verifyResult.success)
        {
            result.error = QString("备份完成但验证失败: %1").arg(verifyResult.error);
            result.success = false;
            return result;
        }
    }

    if (options.retentionCount > 0)
    {
        enforceRetention(options, backupLocation, callbacks);
    }

    return result;
}

RestoreResult BackupEngine::restore(const RestoreOptions &options, const BackupEngineCallbacks &callbacks) const
{
    RestoreResult result;
    QString error;
    auto manifest = loadManifest(options.backupPath, error);
    if (!manifest)
    {
        result.error = error;
        return result;
    }

    QString destinationRoot = QDir(options.restoreDestination).filePath(manifest->rootName);
    if (!ensureDirectory(destinationRoot, error))
    {
        result.error = QString("无法创建恢复目录: %1").arg(destinationRoot);
        return result;
    }

    int current = 0;
    const int total = manifest->files.size();
    for (const auto &record : manifest->files)
    {
        ++current;
        reportProgress(callbacks, current, std::max(total, 1));
        const QString targetPath = joinPath(destinationRoot, record.relativePath);
        QFileInfo info(targetPath);
        if (!ensureParentDir(targetPath))
        {
            log(callbacks, QString("无法创建父目录: %1").arg(info.path()));
            continue;
        }

        switch (record.kind)
        {
        case FileKind::Directory:
            ensureDirectory(targetPath, error);
            break;
        case FileKind::Regular:
        {
            QString dataError;
            QByteArray data = readStoredData(*manifest, record, options.password, dataError);
            if (!dataError.isEmpty())
            {
                log(callbacks, dataError);
                continue;
            }
            QByteArray decoded = data;
            bool ok = true;
            if (manifest->encrypted)
            {
                decoded = decrypt(decoded, options.password, ok);
                if (!ok)
                {
                    log(callbacks, QString("解密失败: %1").arg(record.relativePath));
                    continue;
                }
            }
            if (manifest->compressed)
            {
                decoded = decompressData(decoded, record.size, ok);
                if (!ok)
                {
                    log(callbacks, QString("解压失败: %1").arg(record.relativePath));
                    continue;
                }
            }
            QSaveFile file(targetPath);
            if (!file.open(QIODevice::WriteOnly))
            {
                log(callbacks, QString("无法写入文件: %1").arg(targetPath));
                continue;
            }
            if (file.write(decoded) != decoded.size() || !file.commit())
            {
                log(callbacks, QString("写入文件失败: %1").arg(targetPath));
                continue;
            }
#ifdef Q_OS_UNIX
            if (manifest->preserveMetadata)
            {
                applyOwnership(targetPath, record.ownerId, record.groupId, true);
                applyTimestamps(targetPath, record, true);
            }
#endif
            if (manifest->preserveMetadata)
            {
                applyPermissions(targetPath, toQtPermissions(record.permissions));
            }
            break;
        }
        case FileKind::Symlink:
            QFile::remove(targetPath);
            if (!QFile::link(record.symlinkTarget, targetPath))
            {
                log(callbacks, QString("创建符号链接失败: %1").arg(targetPath));
            }
            break;
        case FileKind::Fifo:
#ifdef Q_OS_UNIX
            QFile::remove(targetPath);
            ::mkfifo(QFile::encodeName(targetPath).constData(), record.permissions);
#endif
            break;
        case FileKind::Block:
        case FileKind::Character:
        {
#ifdef Q_OS_UNIX
            QFile::remove(targetPath);
            mode_t mode = (record.kind == FileKind::Block ? S_IFBLK : S_IFCHR) | record.permissions;
            ::mknod(QFile::encodeName(targetPath).constData(), mode, static_cast<dev_t>(record.specialDevice));
#endif
            break;
        }
        case FileKind::Socket:
            log(callbacks, QString("跳过套接字文件: %1").arg(record.relativePath));
            break;
        default:
            break;
        }

        if (manifest->preserveMetadata && record.kind != FileKind::Symlink && record.kind != FileKind::Socket)
        {
            applyPermissions(targetPath, toQtPermissions(record.permissions));
        }
        ++result.restoredFiles;
    }

    result.success = true;
    result.targetPath = destinationRoot;
    return result;
}

VerifyResult BackupEngine::verify(const VerifyOptions &options, const BackupEngineCallbacks &callbacks) const
{
    VerifyResult result;
    QString error;
    auto manifest = loadManifest(options.backupPath, error);
    if (!manifest)
    {
        result.error = error;
        return result;
    }

    int current = 0;
    const int total = manifest->files.size();
    for (const auto &record : manifest->files)
    {
        if (!record.hasData)
        {
            continue;
        }
        ++current;
        reportProgress(callbacks, current, std::max(total, 1));
        QString dataError;
        QByteArray stored = readStoredData(*manifest, record, options.password, dataError);
        if (!dataError.isEmpty())
        {
            ++result.failedFiles;
            log(callbacks, dataError);
            continue;
        }
        QByteArray original = stored;
        bool ok = true;
        if (manifest->encrypted)
        {
            original = decrypt(original, options.password, ok);
            if (!ok)
            {
                ++result.failedFiles;
                log(callbacks, QString("解密失败: %1").arg(record.relativePath));
                continue;
            }
        }
        if (manifest->compressed)
        {
            original = decompressData(original, record.size, ok);
            if (!ok)
            {
                ++result.failedFiles;
                log(callbacks, QString("解压失败: %1").arg(record.relativePath));
                continue;
            }
        }
        const QByteArray hash = sha256(original);
        if (QString::fromLatin1(hash) != record.hash)
        {
            ++result.failedFiles;
            log(callbacks, QString("校验失败: %1").arg(record.relativePath));
        }
        ++result.checkedFiles;
    }

    result.success = result.failedFiles == 0;
    if (!result.success && result.error.isEmpty())
    {
        result.error = "验证失败，请检查日志";
    }
    return result;
}

BackupEngine::ManifestPtr BackupEngine::loadManifest(const QString &path, QString &error) const
{
    QFileInfo info(path);
    if (!info.exists())
    {
        error = QString("未找到备份路径: %1").arg(path);
        return std::nullopt;
    }
    if (info.isDir())
    {
        return loadManifestFromDirectory(info.absoluteFilePath(), error);
    }
    if (info.isFile())
    {
        const QString suffix = info.suffix().toLower();
        if (suffix == "fbk")
        {
            return loadManifestFromPackage(info.absoluteFilePath(), error);
        }
        if (info.fileName() == "manifest.json")
        {
            return loadManifestFromDirectory(info.absolutePath(), error);
        }
        QFileInfo manifestCandidate(QDir(info.absolutePath()).filePath("manifest.json"));
        if (manifestCandidate.exists())
        {
            return loadManifestFromDirectory(info.absolutePath(), error);
        }
    }
    QFileInfo manifestInDir(QDir(path).filePath("manifest.json"));
    if (manifestInDir.exists())
    {
        return loadManifestFromDirectory(manifestInDir.absolutePath(), error);
    }
    error = QString("无法加载备份: %1").arg(path);
    return std::nullopt;
}

BackupEngine::ManifestPtr BackupEngine::loadManifestFromDirectory(const QString &path, QString &error) const
{
    QFileInfo info(path);
    QString manifestPath = info.isDir() ? QDir(info.absoluteFilePath()).filePath("manifest.json") : info.absoluteFilePath();
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("无法读取 manifest: %1").arg(manifestPath);
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QByteArray data = file.readAll();
    QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        error = QString("manifest 格式错误: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    BackupManifest manifest = manifestFromJson(document.object());
    manifest.storageMode = StorageMode::Directory;
    manifest.manifestPath = manifestPath;
    manifest.basePath = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    manifest.dataDirectory = QDir(manifest.basePath).filePath(manifest.dataRelativePath);
    return manifest;
}

BackupEngine::ManifestPtr BackupEngine::loadManifestFromPackage(const QString &path, QString &error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("无法打开备份文件: %1").arg(path);
        return std::nullopt;
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    QByteArray magic(4, Qt::Uninitialized);
    if (stream.readRawData(magic.data(), magic.size()) != magic.size() || magic != kPackageMagic)
    {
        error = "备份文件格式错误";
        return std::nullopt;
    }
    quint32 version = 0;
    stream >> version;
    if (version != kPackageVersion)
    {
        error = "备份文件版本不兼容";
        return std::nullopt;
    }
    Header header;
    stream >> header.manifestOffset;
    stream >> header.manifestLength;
    if (header.manifestOffset == 0 || header.manifestLength == 0)
    {
        error = "备份文件不完整";
        return std::nullopt;
    }
    if (!file.seek(static_cast<qint64>(header.manifestOffset)))
    {
        error = "无法定位 manifest 数据";
        return std::nullopt;
    }
    QByteArray manifestBytes = file.read(static_cast<qint64>(header.manifestLength));
    if (manifestBytes.size() != static_cast<qint64>(header.manifestLength))
    {
        error = "读取 manifest 失败";
        return std::nullopt;
    }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        error = QString("manifest 格式错误: %1").arg(parseError.errorString());
        return std::nullopt;
    }
    BackupManifest manifest = manifestFromJson(document.object());
    manifest.storageMode = StorageMode::Package;
    manifest.manifestPath = path;
    manifest.packagePath = path;
    manifest.basePath = QFileInfo(path).absolutePath();
    return manifest;
}

bool BackupEngine::writeManifestToDirectory(const QString &path, const BackupManifest &manifest, QString &error) const
{
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath("."))
    {
        error = QString("无法创建目录: %1").arg(path);
        return false;
    }
    const QString manifestPath = dir.filePath("manifest.json");
    QSaveFile file(manifestPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        error = QString("无法写入 manifest: %1").arg(manifestPath);
        return false;
    }
    QJsonDocument document(manifestToJson(manifest));
    if (file.write(document.toJson(QJsonDocument::Indented)) <= 0)
    {
        error = "写入 manifest 失败";
        return false;
    }
    if (!file.commit())
    {
        error = "提交 manifest 文件失败";
        return false;
    }
    return true;
}

bool BackupEngine::finalizePackage(QFile &file, const BackupManifest &manifest, QString &error) const
{
    if (!file.isOpen())
    {
        error = "备份文件未打开";
        return false;
    }
    QJsonDocument document(manifestToJson(manifest));
    QByteArray manifestBytes = document.toJson(QJsonDocument::Indented);
    const quint64 offset = static_cast<quint64>(file.pos());
    if (file.write(manifestBytes) != manifestBytes.size())
    {
        error = "写入 manifest 失败";
        return false;
    }
    const quint64 length = static_cast<quint64>(manifestBytes.size());
    if (!file.seek(kPackageMagic.size() + sizeof(quint32)))
    {
        error = "无法回写 manifest 位置";
        return false;
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << offset;
    stream << length;
    file.flush();
    return true;
}


QByteArray BackupEngine::readStoredData(const BackupManifest &manifest, const FileRecord &record, const QString &password,
                                        QString &error) const
{
    Q_UNUSED(password);
    QByteArray data;
    if (manifest.storageMode == StorageMode::Package)
    {
        QFile file(manifest.packagePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            error = QString("无法打开备份包: %1").arg(manifest.packagePath);
            return {};
        }
        if (!file.seek(static_cast<qint64>(record.dataOffset)))
        {
            error = "无法定位数据块";
            return {};
        }
        data = file.read(static_cast<qint64>(record.storedSize));
        if (data.size() != static_cast<qint64>(record.storedSize))
        {
            error = QString("读取数据失败: %1").arg(record.relativePath);
            return {};
        }
    }
    else
    {
        const QString storedPath = dataFilePath(manifest, record);
        QFile file(storedPath);
        if (!file.open(QIODevice::ReadOnly))
        {
            error = QString("无法读取备份数据: %1").arg(storedPath);
            return {};
        }
        data = file.readAll();
    }
    return data;
}

QString BackupEngine::dataFilePath(const BackupManifest &manifest, const FileRecord &record) const
{
    if (manifest.storageMode == StorageMode::Package)
    {
        return manifest.packagePath;
    }
    return QDir(manifest.basePath).filePath(QStringLiteral("%1/%2").arg(manifest.dataRelativePath, record.storedRelativePath));
}

QByteArray BackupEngine::encrypt(const QByteArray &data, const QString &password) const
{
    if (password.isEmpty())
    {
        return data;
    }
    QByteArray key = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    QByteArray output = data;
    for (int i = 0; i < output.size(); ++i)
    {
        output[i] = output.at(i) ^ key.at(i % key.size());
    }
    return output;
}

QByteArray BackupEngine::decrypt(const QByteArray &data, const QString &password, bool &ok) const
{
    if (password.isEmpty())
    {
        ok = false;
        return {};
    }
    ok = true;
    return encrypt(data, password);
}

QByteArray BackupEngine::compressData(const QByteArray &data) const
{
    return qCompress(data, 9);
}

QByteArray BackupEngine::decompressData(const QByteArray &data, quint64 expectedSize, bool &ok) const
{
    QByteArray output = qUncompress(data);
    if (output.isEmpty() && expectedSize > 0)
    {
        ok = false;
    }
    else
    {
        ok = true;
    }
    return output;
}

QFile::Permissions BackupEngine::toQtPermissions(quint32 permissions) const
{
    QFile::Permissions perms;
    if (permissions & 0400)
        perms |= QFile::ReadOwner;
    if (permissions & 0200)
        perms |= QFile::WriteOwner;
    if (permissions & 0100)
        perms |= QFile::ExeOwner;
    if (permissions & 0040)
        perms |= QFile::ReadGroup;
    if (permissions & 0020)
        perms |= QFile::WriteGroup;
    if (permissions & 0010)
        perms |= QFile::ExeGroup;
    if (permissions & 0004)
        perms |= QFile::ReadOther;
    if (permissions & 0002)
        perms |= QFile::WriteOther;
    if (permissions & 0001)
        perms |= QFile::ExeOther;
    return perms;
}

quint32 BackupEngine::toStoredPermissions(QFile::Permissions permissions) const
{
    quint32 value = 0;
    if (permissions & QFile::ReadOwner)
        value |= 0400;
    if (permissions & QFile::WriteOwner)
        value |= 0200;
    if (permissions & QFile::ExeOwner)
        value |= 0100;
    if (permissions & QFile::ReadGroup)
        value |= 0040;
    if (permissions & QFile::WriteGroup)
        value |= 0020;
    if (permissions & QFile::ExeGroup)
        value |= 0010;
    if (permissions & QFile::ReadOther)
        value |= 0004;
    if (permissions & QFile::WriteOther)
        value |= 0002;
    if (permissions & QFile::ExeOther)
        value |= 0001;
    return value;
}

bool BackupEngine::ensureDirectory(const QString &path, QString &error) const
{
    QDir dir(path);
    if (dir.exists())
    {
        return true;
    }
    if (!dir.mkpath("."))
    {
        error = QString("无法创建目录: %1").arg(path);
        return false;
    }
    return true;
}

bool BackupEngine::removePath(const QString &path) const
{
    QFileInfo info(path);
    if (!info.exists())
    {
        return true;
    }
    if (info.isDir())
    {
        QDir dir(path);
        return dir.removeRecursively();
    }
    return QFile::remove(path);
}

StorageMode BackupEngine::detectStorageMode(const QString &path) const
{
    QFileInfo info(path);
    if (info.isDir())
    {
        return StorageMode::Directory;
    }
    return StorageMode::Package;
}

void BackupEngine::enforceRetention(const BackupOptions &options, const QString &latestPath,
                                    const BackupEngineCallbacks &callbacks) const
{
    if (options.retentionCount <= 0)
    {
        return;
    }
    QDir dir(options.destinationPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QVector<QFileInfo> candidates;
    for (const QFileInfo &info : entries)
    {
        if (info.isDir())
        {
            if (!QFileInfo(QDir(info.absoluteFilePath()).filePath("manifest.json")).exists())
            {
                continue;
            }
        }
        else if (info.suffix().toLower() != "fbk")
        {
            continue;
        }
        candidates.push_back(info);
    }
    std::sort(candidates.begin(), candidates.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() > b.lastModified();
    });

    while (candidates.size() > options.retentionCount)
    {
        const QFileInfo info = candidates.takeLast();
        if (info.absoluteFilePath() == latestPath)
        {
            continue;
        }
        if (removePath(info.absoluteFilePath()))
        {
            log(callbacks, QString("已清理旧备份: %1").arg(info.fileName()));
        }
    }
}
