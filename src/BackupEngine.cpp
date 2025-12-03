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
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <queue>
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

    struct HuffmanNode
    {
        quint64 freq = 0;
        int value = -1;
        int left = -1;
        int right = -1;
    };

    int buildHuffmanTree(const std::array<quint64, 256> &freq, std::vector<HuffmanNode> &nodes)
    {
        struct Item
        {
            quint64 freq = 0;
            int index = -1;
        };
        auto cmp = [](const Item &a, const Item &b) { return a.freq > b.freq; };
        std::priority_queue<Item, std::vector<Item>, decltype(cmp)> queue(cmp);

        for (int i = 0; i < 256; ++i)
        {
            if (freq[i] == 0)
            {
                continue;
            }
            HuffmanNode node;
            node.freq = freq[i];
            node.value = i;
            nodes.push_back(node);
            queue.push({node.freq, static_cast<int>(nodes.size() - 1)});
        }

        if (queue.empty())
        {
            return -1;
        }

        if (queue.size() == 1)
        {
            // 只出现一种字节，复制一个零频节点保持编码逻辑
            HuffmanNode duplicate;
            duplicate.freq = 0;
            nodes.push_back(duplicate);
            const int first = queue.top().index;
            queue.pop();

            HuffmanNode parent;
            parent.freq = nodes[first].freq;
            parent.left = first;
            parent.right = static_cast<int>(nodes.size() - 1);
            nodes.push_back(parent);
            return static_cast<int>(nodes.size() - 1);
        }

        while (queue.size() > 1)
        {
            Item a = queue.top();
            queue.pop();
            Item b = queue.top();
            queue.pop();

            HuffmanNode parent;
            parent.freq = a.freq + b.freq;
            parent.left = a.index;
            parent.right = b.index;
            nodes.push_back(parent);
            queue.push({parent.freq, static_cast<int>(nodes.size() - 1)});
        }

        return queue.top().index;
    }

    void buildCodes(const std::vector<HuffmanNode> &nodes, int index, QByteArray prefix, std::array<QByteArray, 256> &codes)
    {
        if (index < 0 || index >= static_cast<int>(nodes.size()))
        {
            return;
        }
        const HuffmanNode &node = nodes[static_cast<size_t>(index)];
        if (node.value >= 0)
        {
            // 单节点情况确保至少有一位编码
            if (prefix.isEmpty())
            {
                prefix.append('0');
            }
            codes[static_cast<size_t>(node.value)] = prefix;
            return;
        }
        QByteArray leftPrefix = prefix;
        leftPrefix.append('0');
        QByteArray rightPrefix = prefix;
        rightPrefix.append('1');
        buildCodes(nodes, node.left, leftPrefix, codes);
        buildCodes(nodes, node.right, rightPrefix, codes);
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

    QString rawName = options.backupName.isEmpty() ? sourceInfo.fileName() : options.backupName;
    QString name = sanitizedName(rawName);

    // Replace characters not allowed on Windows and other control characters with '_'
    const QString forbidden = R"(<>:"/\\|?*)";
    QString cleaned;
    cleaned.reserve(name.size());
    for (QChar ch : name)
    {
        if (ch.unicode() < 0x20 || forbidden.contains(ch))
            cleaned.append('_');
        else
            cleaned.append(ch);
    }

    // Trim trailing spaces and dots (Windows forbids names ending with space or dot)
    while (!cleaned.isEmpty() && (cleaned.endsWith(' ') || cleaned.endsWith('.')))
    {
        cleaned.chop(1);
    }

    if (cleaned.isEmpty())
    {
        cleaned = "backup";
    }

    // Avoid reserved Windows device names (CON, PRN, AUX, NUL, COM1..COM9, LPT1..LPT9)
    QString upper = cleaned.toUpper();
    bool isReserved = (upper == QLatin1String("CON") || upper == QLatin1String("PRN") ||
                       upper == QLatin1String("AUX") || upper == QLatin1String("NUL"));
    if (!isReserved)
    {
        if (upper.size() == 4 && upper.startsWith(QLatin1String("COM")) && upper.at(3).isDigit())
            isReserved = true;
        if (upper.size() == 4 && upper.startsWith(QLatin1String("LPT")) && upper.at(3).isDigit())
            isReserved = true;
    }
    if (isReserved)
    {
        cleaned.append('_');
    }

    // Limit name length to a safe value for filesystems
    const int maxLen = 240;
    if (cleaned.size() > maxLen)
    {
        cleaned = cleaned.left(maxLen);
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss");
    const QString identifier = QString("%1_%2").arg(timestamp, cleaned);

    BackupManifest manifest;
    manifest.backupId = identifier;
    {
        QString rootName = sourceInfo.fileName();
        if (rootName.isEmpty())
        {
            const QString absolutePath = sourceInfo.absoluteFilePath();
            if (!absolutePath.isEmpty())
            {
                QFileInfo absoluteInfo(absolutePath);
                rootName = absoluteInfo.fileName();
                if (rootName.isEmpty())
                {
                    const QString parentPath = absoluteInfo.absolutePath();
                    if (!parentPath.isEmpty())
                        rootName = QFileInfo(parentPath).fileName();
                    if (rootName.isEmpty())
                        rootName = absolutePath;
                }
            }
        }
        if (rootName.isEmpty())
            rootName = QStringLiteral("root");
        manifest.rootName = rootName;
    }
    manifest.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    manifest.sourcePath = sourceInfo.absoluteFilePath();
    manifest.storageMode = options.package ? StorageMode::Package : StorageMode::Directory;
    manifest.compressed = options.compress;
    manifest.encrypted = options.encrypt;
    manifest.compressionMethod = options.compress ? options.compressionMethod : CompressionMethod::None;
    manifest.encryptionMethod = options.encrypt ? options.encryptionMethod : EncryptionMethod::None;
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

    auto writeDataToDirectory = [&](const QString &relativePath, const QByteArray &storedData) -> bool
    {
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
        struct stat st{};
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
                storedData = compressData(storedData, options.compressionMethod);
            }
            if (options.encrypt)
            {
                storedData = encrypt(storedData, options.password, options.encryptionMethod);
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
                decoded = decrypt(decoded, options.password, manifest->encryptionMethod, ok);
                if (!ok)
                {
                    log(callbacks, QString("解密失败: %1").arg(record.relativePath));
                    continue;
                }
            }
            if (manifest->compressed)
            {
                decoded = decompressData(decoded, manifest->compressionMethod, record.size, ok);
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
            original = decrypt(original, options.password, manifest->encryptionMethod, ok);
            if (!ok)
            {
                ++result.failedFiles;
                log(callbacks, QString("解密失败: %1").arg(record.relativePath));
                continue;
            }
        }
        if (manifest->compressed)
        {
            original = decompressData(original, manifest->compressionMethod, record.size, ok);
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
    if (data.isEmpty())
    {
        return QByteArray();
    }

    std::array<quint64, 256> freq{};
    for (char ch : data)
    {
        const unsigned char index = static_cast<unsigned char>(ch);
        ++freq[static_cast<size_t>(index)];
    }

    std::vector<HuffmanNode> nodes;
    nodes.reserve(512);
    const int rootIndex = buildHuffmanTree(freq, nodes);
    if (rootIndex < 0)
    {
        return QByteArray();
    }

    std::array<QByteArray, 256> codes;
    buildCodes(nodes, rootIndex, QByteArray(), codes);

    quint64 bitLength = 0;
    for (int i = 0; i < 256; ++i)
    {
        if (!codes[static_cast<size_t>(i)].isEmpty())
        {
            bitLength += freq[static_cast<size_t>(i)] * static_cast<quint64>(codes[static_cast<size_t>(i)].size());
        }
    }

    QByteArray bitData;
    bitData.reserve(static_cast<int>((bitLength + 7) / 8));
    quint8 current = 0;
    int bitPos = 0;
    auto pushBit = [&](bool one) {
        current |= static_cast<quint8>((one ? 1 : 0) << (7 - bitPos));
        ++bitPos;
        if (bitPos == 8)
        {
            bitData.append(static_cast<char>(current));
            current = 0;
            bitPos = 0;
        }
    };

    for (char ch : data)
    {
        const QByteArray &code = codes[static_cast<size_t>(static_cast<unsigned char>(ch))];
        for (char bitChar : code)
        {
            pushBit(bitChar == '1');
        }
    }
    if (bitPos > 0)
    {
        bitData.append(static_cast<char>(current));
    }

    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    static const char magic[4] = {'H', 'F', '0', '1'};
    stream.writeRawData(magic, 4);
    stream << static_cast<quint64>(data.size());
    for (quint64 value : freq)
    {
        stream << value;
    }
    stream << bitLength;
    stream.writeRawData(bitData.constData(), bitData.size());

    return output;
}

QByteArray BackupEngine::decompressData(const QByteArray &data, quint64 expectedSize, bool &ok) const
{
    if (data.isEmpty())
    {
        ok = expectedSize == 0;
        return {};
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    char magic[4] = {};
    if (stream.readRawData(magic, 4) != 4 || magic[0] != 'H' || magic[1] != 'F')
    {
        ok = false;
        return {};
    }

    quint64 originalSize = 0;
    stream >> originalSize;
    std::array<quint64, 256> freq{};
    for (int i = 0; i < 256; ++i)
    {
        stream >> freq[static_cast<size_t>(i)];
    }

    quint64 bitLength = 0;
    stream >> bitLength;

    const qint64 headerSize = 4 + 8 + (256 * 8) + 8;
    if (data.size() < headerSize)
    {
        ok = false;
        return {};
    }

    QByteArray bitData = data.mid(headerSize);

    std::vector<HuffmanNode> nodes;
    nodes.reserve(512);
    const int rootIndex = buildHuffmanTree(freq, nodes);
    if (rootIndex < 0)
    {
        ok = (expectedSize == 0 && originalSize == 0);
        return {};
    }

    QByteArray output;
    output.reserve(static_cast<int>(originalSize));

    // 单节点：不需要读取位串即可恢复
    if (nodes[static_cast<size_t>(rootIndex)].value >= 0)
    {
        output = QByteArray(static_cast<int>(originalSize),
                            static_cast<char>(nodes[static_cast<size_t>(rootIndex)].value));
        ok = (output.size() == static_cast<int>(originalSize) &&
              (expectedSize == 0 || originalSize == expectedSize));
        return output;
    }

    const quint64 availableBits = static_cast<quint64>(bitData.size()) * 8;
    if (bitLength > availableBits)
    {
        ok = false;
        return {};
    }

    int nodeIndex = rootIndex;
    quint64 bitsRead = 0;
    for (quint64 i = 0; i < bitLength && output.size() < static_cast<int>(originalSize); ++i)
    {
        const int byteIndex = static_cast<int>(i / 8);
        const int bitOffset = 7 - static_cast<int>(i % 8);
        const quint8 byteValue = static_cast<quint8>(bitData.at(byteIndex));
        const bool one = (byteValue >> bitOffset) & 0x1;

        nodeIndex = one ? nodes[static_cast<size_t>(nodeIndex)].right : nodes[static_cast<size_t>(nodeIndex)].left;
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
        {
            ok = false;
            return {};
        }
        const HuffmanNode &node = nodes[static_cast<size_t>(nodeIndex)];
        if (node.value >= 0)
        {
            output.append(static_cast<char>(node.value));
            nodeIndex = rootIndex;
        }
        ++bitsRead;
    }

    ok = (output.size() == static_cast<int>(originalSize) &&
          (expectedSize == 0 || originalSize == expectedSize));
    if (!ok)
    {
        output.clear();
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
    std::sort(candidates.begin(), candidates.end(), [](const QFileInfo &a, const QFileInfo &b)
              { return a.lastModified() > b.lastModified(); });

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

// ==================== 多种压缩方法实现 ====================

QByteArray BackupEngine::compressHuffman(const QByteArray &data) const
{
    return compressData(data); // 使用现有实现
}

QByteArray BackupEngine::decompressHuffman(const QByteArray &data, quint64 expectedSize, bool &ok) const
{
    return decompressData(data, expectedSize, ok); // 使用现有实现
}

QByteArray BackupEngine::compressRLE(const QByteArray &data) const
{
    if (data.isEmpty())
    {
        return QByteArray();
    }

    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // RLE 魔数
    static const char magic[4] = {'R', 'L', 'E', '1'};
    stream.writeRawData(magic, 4);
    stream << static_cast<quint64>(data.size());

    // RLE 编码：[count][byte] 格式，count 用 1 字节表示（1-255 的重复）
    int i = 0;
    while (i < data.size())
    {
        char current = data.at(i);
        int count = 1;
        while (i + count < data.size() && data.at(i + count) == current && count < 255)
        {
            ++count;
        }
        output.append(static_cast<char>(count));
        output.append(current);
        i += count;
    }

    return output;
}

QByteArray BackupEngine::decompressRLE(const QByteArray &data, quint64 expectedSize, bool &ok) const
{
    if (data.isEmpty())
    {
        ok = expectedSize == 0;
        return {};
    }

    if (data.size() < 12) // magic(4) + size(8)
    {
        ok = false;
        return {};
    }

    // 检查魔数
    if (data.at(0) != 'R' || data.at(1) != 'L' || data.at(2) != 'E' || data.at(3) != '1')
    {
        ok = false;
        return {};
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.skipRawData(4); // 跳过魔数

    quint64 originalSize = 0;
    stream >> originalSize;

    QByteArray output;
    output.reserve(static_cast<int>(originalSize));

    int pos = 12; // 从数据部分开始
    while (pos + 1 < data.size() && output.size() < static_cast<int>(originalSize))
    {
        int count = static_cast<unsigned char>(data.at(pos));
        char byte = data.at(pos + 1);
        for (int j = 0; j < count && output.size() < static_cast<int>(originalSize); ++j)
        {
            output.append(byte);
        }
        pos += 2;
    }

    ok = (output.size() == static_cast<int>(originalSize) &&
          (expectedSize == 0 || originalSize == expectedSize));
    return output;
}

QByteArray BackupEngine::compressZlib(const QByteArray &data) const
{
    if (data.isEmpty())
    {
        return QByteArray();
    }

    QByteArray compressed = qCompress(data, 9); // 最高压缩级别

    // 添加魔数和原始大小
    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    static const char magic[4] = {'Z', 'L', 'I', 'B'};
    stream.writeRawData(magic, 4);
    stream << static_cast<quint64>(data.size());
    output.append(compressed);

    return output;
}

QByteArray BackupEngine::decompressZlib(const QByteArray &data, quint64 expectedSize, bool &ok) const
{
    if (data.isEmpty())
    {
        ok = expectedSize == 0;
        return {};
    }

    if (data.size() < 12) // magic(4) + size(8)
    {
        ok = false;
        return {};
    }

    // 检查魔数
    if (data.at(0) != 'Z' || data.at(1) != 'L' || data.at(2) != 'I' || data.at(3) != 'B')
    {
        ok = false;
        return {};
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.skipRawData(4); // 跳过魔数

    quint64 originalSize = 0;
    stream >> originalSize;

    QByteArray compressed = data.mid(12);
    QByteArray output = qUncompress(compressed);

    ok = (output.size() == static_cast<int>(originalSize) &&
          (expectedSize == 0 || originalSize == expectedSize));
    return output;
}

// 带方法参数的压缩接口
QByteArray BackupEngine::compressData(const QByteArray &data, CompressionMethod method) const
{
    switch (method)
    {
    case CompressionMethod::Huffman:
        return compressHuffman(data);
    case CompressionMethod::RLE:
        return compressRLE(data);
    case CompressionMethod::Zlib:
        return compressZlib(data);
    case CompressionMethod::None:
    default:
        return data;
    }
}

QByteArray BackupEngine::decompressData(const QByteArray &data, CompressionMethod method, quint64 expectedSize, bool &ok) const
{
    switch (method)
    {
    case CompressionMethod::Huffman:
        return decompressHuffman(data, expectedSize, ok);
    case CompressionMethod::RLE:
        return decompressRLE(data, expectedSize, ok);
    case CompressionMethod::Zlib:
        return decompressZlib(data, expectedSize, ok);
    case CompressionMethod::None:
    default:
        ok = true;
        return data;
    }
}

// ==================== 多种加密方法实现 ====================

QByteArray BackupEngine::encryptXOR(const QByteArray &data, const QString &password) const
{
    return encrypt(data, password); // 使用现有实现
}

QByteArray BackupEngine::decryptXOR(const QByteArray &data, const QString &password) const
{
    return encrypt(data, password); // XOR 是对称的
}

QByteArray BackupEngine::encryptRC4(const QByteArray &data, const QString &password) const
{
    if (data.isEmpty() || password.isEmpty())
    {
        return data;
    }

    // RC4 密钥调度算法 (KSA)
    QByteArray key = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    std::array<unsigned char, 256> S;
    for (int i = 0; i < 256; ++i)
    {
        S[i] = static_cast<unsigned char>(i);
    }

    int j = 0;
    for (int i = 0; i < 256; ++i)
    {
        j = (j + S[i] + static_cast<unsigned char>(key.at(i % key.size()))) % 256;
        std::swap(S[i], S[j]);
    }

    // RC4 伪随机生成算法 (PRGA) 并加密
    QByteArray output;
    output.reserve(data.size());

    int i = 0;
    j = 0;
    for (int k = 0; k < data.size(); ++k)
    {
        i = (i + 1) % 256;
        j = (j + S[i]) % 256;
        std::swap(S[i], S[j]);
        unsigned char keyByte = S[(S[i] + S[j]) % 256];
        output.append(static_cast<char>(static_cast<unsigned char>(data.at(k)) ^ keyByte));
    }

    return output;
}

QByteArray BackupEngine::decryptRC4(const QByteArray &data, const QString &password) const
{
    return encryptRC4(data, password); // RC4 是对称的
}

QByteArray BackupEngine::encryptAES256(const QByteArray &data, const QString &password) const
{
    if (data.isEmpty() || password.isEmpty())
    {
        return data;
    }

    // 简化版 AES-256：使用多轮 XOR + 字节替换 + 行移位
    // 注意：这不是真正的 AES，仅用于教学演示
    QByteArray key = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);

    // S-box (简化版)
    static const unsigned char sbox[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
    };

    // 添加魔数和原始大小
    QByteArray output;
    QDataStream stream(&output, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    static const char magic[4] = {'A', 'E', 'S', '2'};
    stream.writeRawData(magic, 4);
    stream << static_cast<quint64>(data.size());

    // 填充到 16 字节的倍数
    QByteArray padded = data;
    int padLen = 16 - (padded.size() % 16);
    if (padLen == 0) padLen = 16;
    for (int i = 0; i < padLen; ++i)
    {
        padded.append(static_cast<char>(padLen));
    }

    // 多轮加密
    QByteArray encrypted;
    encrypted.reserve(padded.size());

    for (int block = 0; block < padded.size(); block += 16)
    {
        QByteArray chunk = padded.mid(block, 16);

        // 4 轮变换
        for (int round = 0; round < 4; ++round)
        {
            // 字节替换
            for (int i = 0; i < chunk.size(); ++i)
            {
                chunk[i] = static_cast<char>(sbox[static_cast<unsigned char>(chunk.at(i))]);
            }

            // XOR with key
            for (int i = 0; i < chunk.size(); ++i)
            {
                chunk[i] = chunk.at(i) ^ key.at((i + round * 4) % key.size());
            }

            // 简单行移位
            if (chunk.size() >= 16)
            {
                char temp = chunk.at(1);
                chunk[1] = chunk.at(5);
                chunk[5] = chunk.at(9);
                chunk[9] = chunk.at(13);
                chunk[13] = temp;
            }
        }

        encrypted.append(chunk);
    }

    output.append(encrypted);
    return output;
}

QByteArray BackupEngine::decryptAES256(const QByteArray &data, const QString &password, bool &ok) const
{
    if (data.isEmpty())
    {
        ok = true;
        return {};
    }

    if (password.isEmpty() || data.size() < 12)
    {
        ok = false;
        return {};
    }

    // 检查魔数
    if (data.at(0) != 'A' || data.at(1) != 'E' || data.at(2) != 'S' || data.at(3) != '2')
    {
        ok = false;
        return {};
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.skipRawData(4); // 跳过魔数

    quint64 originalSize = 0;
    stream >> originalSize;

    QByteArray key = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);

    // 逆 S-box
    static const unsigned char inv_sbox[256] = {
        0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
        0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
        0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
        0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
        0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
        0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
        0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
        0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
        0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
        0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
        0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
        0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
        0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
        0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
        0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
    };

    QByteArray encrypted = data.mid(12);
    QByteArray decrypted;
    decrypted.reserve(encrypted.size());

    for (int block = 0; block < encrypted.size(); block += 16)
    {
        QByteArray chunk = encrypted.mid(block, 16);
        if (chunk.size() < 16)
        {
            ok = false;
            return {};
        }

        // 逆序 4 轮变换
        for (int round = 3; round >= 0; --round)
        {
            // 逆行移位
            if (chunk.size() >= 16)
            {
                char temp = chunk.at(13);
                chunk[13] = chunk.at(9);
                chunk[9] = chunk.at(5);
                chunk[5] = chunk.at(1);
                chunk[1] = temp;
            }

            // XOR with key
            for (int i = 0; i < chunk.size(); ++i)
            {
                chunk[i] = chunk.at(i) ^ key.at((i + round * 4) % key.size());
            }

            // 逆字节替换
            for (int i = 0; i < chunk.size(); ++i)
            {
                chunk[i] = static_cast<char>(inv_sbox[static_cast<unsigned char>(chunk.at(i))]);
            }
        }

        decrypted.append(chunk);
    }

    // 移除填充
    if (decrypted.isEmpty())
    {
        ok = false;
        return {};
    }

    int padLen = static_cast<unsigned char>(decrypted.at(decrypted.size() - 1));
    if (padLen < 1 || padLen > 16 || padLen > decrypted.size())
    {
        ok = false;
        return {};
    }

    decrypted.chop(padLen);

    ok = (decrypted.size() == static_cast<int>(originalSize));
    return decrypted;
}

// 带方法参数的加密接口
QByteArray BackupEngine::encrypt(const QByteArray &data, const QString &password, EncryptionMethod method) const
{
    switch (method)
    {
    case EncryptionMethod::XOR:
        return encryptXOR(data, password);
    case EncryptionMethod::RC4:
        return encryptRC4(data, password);
    case EncryptionMethod::AES256:
        return encryptAES256(data, password);
    case EncryptionMethod::None:
    default:
        return data;
    }
}

QByteArray BackupEngine::decrypt(const QByteArray &data, const QString &password, EncryptionMethod method, bool &ok) const
{
    switch (method)
    {
    case EncryptionMethod::XOR:
        ok = true;
        return decryptXOR(data, password);
    case EncryptionMethod::RC4:
        ok = true;
        return decryptRC4(data, password);
    case EncryptionMethod::AES256:
        return decryptAES256(data, password, ok);
    case EncryptionMethod::None:
    default:
        ok = true;
        return data;
    }
}
