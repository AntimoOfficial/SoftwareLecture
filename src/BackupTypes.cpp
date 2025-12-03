#include "BackupTypes.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace
{
QString numberToString(quint64 value)
{
    return QString::number(static_cast<qulonglong>(value));
}

quint64 stringToUint64(const QJsonValue &value)
{
    if (value.isDouble())
    {
        return static_cast<quint64>(value.toDouble());
    }
    if (value.isString())
    {
        bool ok = false;
        return value.toString().toULongLong(&ok);
    }
    return 0;
}

quint32 stringToUint32(const QJsonValue &value)
{
    if (value.isDouble())
    {
        return static_cast<quint32>(value.toDouble());
    }
    if (value.isString())
    {
        bool ok = false;
        return value.toString().toUInt(&ok);
    }
    return 0;
}

QJsonArray serializeFiles(const QVector<FileRecord> &records)
{
    QJsonArray array;
    for (const auto &record : records)
    {
        array.push_back(fileRecordToJson(record));
    }
    return array;
}

QVector<FileRecord> deserializeFiles(const QJsonArray &array)
{
    QVector<FileRecord> records;
    records.reserve(array.size());
    for (const auto &value : array)
    {
        if (value.isObject())
        {
            records.push_back(fileRecordFromJson(value.toObject()));
        }
    }
    return records;
}
} // namespace

QString fileKindToString(FileKind kind)
{
    switch (kind)
    {
    case FileKind::Regular:
        return "regular";
    case FileKind::Directory:
        return "directory";
    case FileKind::Symlink:
        return "symlink";
    case FileKind::Fifo:
        return "fifo";
    case FileKind::Block:
        return "block";
    case FileKind::Character:
        return "character";
    case FileKind::Socket:
        return "socket";
    default:
        return "unknown";
    }
}

FileKind fileKindFromString(const QString &value)
{
    const QString normalized = value.toLower();
    if (normalized == "regular")
    {
        return FileKind::Regular;
    }
    if (normalized == "directory")
    {
        return FileKind::Directory;
    }
    if (normalized == "symlink")
    {
        return FileKind::Symlink;
    }
    if (normalized == "fifo")
    {
        return FileKind::Fifo;
    }
    if (normalized == "block")
    {
        return FileKind::Block;
    }
    if (normalized == "character")
    {
        return FileKind::Character;
    }
    if (normalized == "socket")
    {
        return FileKind::Socket;
    }
    return FileKind::Unknown;
}

QString compressionMethodToString(CompressionMethod method)
{
    switch (method)
    {
    case CompressionMethod::None:
        return "none";
    case CompressionMethod::Huffman:
        return "huffman";
    case CompressionMethod::RLE:
        return "rle";
    case CompressionMethod::Zlib:
        return "zlib";
    default:
        return "none";
    }
}

CompressionMethod compressionMethodFromString(const QString &value)
{
    const QString normalized = value.toLower();
    if (normalized == "huffman")
    {
        return CompressionMethod::Huffman;
    }
    if (normalized == "rle")
    {
        return CompressionMethod::RLE;
    }
    if (normalized == "zlib")
    {
        return CompressionMethod::Zlib;
    }
    return CompressionMethod::None;
}

QString encryptionMethodToString(EncryptionMethod method)
{
    switch (method)
    {
    case EncryptionMethod::None:
        return "none";
    case EncryptionMethod::XOR:
        return "xor";
    case EncryptionMethod::RC4:
        return "rc4";
    case EncryptionMethod::AES256:
        return "aes256";
    default:
        return "none";
    }
}

EncryptionMethod encryptionMethodFromString(const QString &value)
{
    const QString normalized = value.toLower();
    if (normalized == "xor")
    {
        return EncryptionMethod::XOR;
    }
    if (normalized == "rc4")
    {
        return EncryptionMethod::RC4;
    }
    if (normalized == "aes256")
    {
        return EncryptionMethod::AES256;
    }
    return EncryptionMethod::None;
}

QJsonObject fileRecordToJson(const FileRecord &record)
{
    QJsonObject object;
    object["path"] = record.relativePath;
    object["type"] = fileKindToString(record.kind);
    object["size"] = numberToString(record.size);
    object["storedSize"] = numberToString(record.storedSize);
    object["hash"] = record.hash;
    object["storedHash"] = record.storedHash;
    object["permissions"] = numberToString(record.permissions);
    object["ownerId"] = numberToString(record.ownerId);
    object["groupId"] = numberToString(record.groupId);
    object["owner"] = record.ownerName;
    object["group"] = record.groupName;
    object["created"] = numberToString(static_cast<quint64>(record.createdAt));
    object["modified"] = numberToString(static_cast<quint64>(record.modifiedAt));
    object["accessed"] = numberToString(static_cast<quint64>(record.accessedAt));
    object["symlinkTarget"] = record.symlinkTarget;
    object["specialDevice"] = numberToString(record.specialDevice);
    object["hasData"] = record.hasData;
    object["metadataOnly"] = record.metadataOnly;
    object["dataOffset"] = numberToString(record.dataOffset);
    object["storedPath"] = record.storedRelativePath;
    return object;
}

FileRecord fileRecordFromJson(const QJsonObject &object)
{
    FileRecord record;
    record.relativePath = object.value("path").toString();
    record.kind = fileKindFromString(object.value("type").toString());
    record.size = stringToUint64(object.value("size"));
    record.storedSize = stringToUint64(object.value("storedSize"));
    record.hash = object.value("hash").toString();
    record.storedHash = object.value("storedHash").toString();
    record.permissions = stringToUint32(object.value("permissions"));
    record.ownerId = stringToUint32(object.value("ownerId"));
    record.groupId = stringToUint32(object.value("groupId"));
    record.ownerName = object.value("owner").toString();
    record.groupName = object.value("group").toString();
    record.createdAt = static_cast<qint64>(stringToUint64(object.value("created")));
    record.modifiedAt = static_cast<qint64>(stringToUint64(object.value("modified")));
    record.accessedAt = static_cast<qint64>(stringToUint64(object.value("accessed")));
    record.symlinkTarget = object.value("symlinkTarget").toString();
    record.specialDevice = stringToUint64(object.value("specialDevice"));
    record.hasData = object.value("hasData").toBool();
    record.metadataOnly = object.value("metadataOnly").toBool();
    record.dataOffset = stringToUint64(object.value("dataOffset"));
    record.storedRelativePath = object.value("storedPath").toString();
    return record;
}

QJsonObject manifestToJson(const BackupManifest &manifest)
{
    QJsonObject object;
    object["version"] = manifest.manifestVersion;
    object["backupId"] = manifest.backupId;
    object["rootName"] = manifest.rootName;
    object["createdAt"] = manifest.createdAt;
    object["sourcePath"] = manifest.sourcePath;
    object["storageMode"] = manifest.storageMode == StorageMode::Package ? "package" : "directory";
    object["compressed"] = manifest.compressed;
    object["encrypted"] = manifest.encrypted;
    object["compressionMethod"] = compressionMethodToString(manifest.compressionMethod);
    object["encryptionMethod"] = encryptionMethodToString(manifest.encryptionMethod);
    object["preserveMetadata"] = manifest.preserveMetadata;
    object["includeSpecial"] = manifest.includeSpecialFiles;
    object["verify"] = manifest.verificationEnabled;
    object["totalFiles"] = numberToString(static_cast<quint64>(manifest.totalFiles));
    object["totalBytes"] = numberToString(static_cast<quint64>(manifest.totalBytes));
    object["dataDir"] = manifest.dataRelativePath;
    object["files"] = serializeFiles(manifest.files);
    return object;
}

BackupManifest manifestFromJson(const QJsonObject &object)
{
    BackupManifest manifest;
    manifest.manifestVersion = object.value("version").toString();
    manifest.backupId = object.value("backupId").toString();
    manifest.rootName = object.value("rootName").toString();
    manifest.createdAt = object.value("createdAt").toString();
    manifest.sourcePath = object.value("sourcePath").toString();
    manifest.storageMode = object.value("storageMode").toString() == "package" ? StorageMode::Package : StorageMode::Directory;
    manifest.compressed = object.value("compressed").toBool();
    manifest.encrypted = object.value("encrypted").toBool();
    manifest.compressionMethod = compressionMethodFromString(object.value("compressionMethod").toString());
    manifest.encryptionMethod = encryptionMethodFromString(object.value("encryptionMethod").toString());
    // 兼容旧版本：如果没有方法字段但标记为压缩/加密，默认使用旧方法
    if (manifest.compressed && manifest.compressionMethod == CompressionMethod::None)
    {
        manifest.compressionMethod = CompressionMethod::Huffman;
    }
    if (manifest.encrypted && manifest.encryptionMethod == EncryptionMethod::None)
    {
        manifest.encryptionMethod = EncryptionMethod::XOR;
    }
    manifest.preserveMetadata = object.value("preserveMetadata").toBool(true);
    manifest.includeSpecialFiles = object.value("includeSpecial").toBool(true);
    manifest.verificationEnabled = object.value("verify").toBool(true);
    manifest.totalFiles = static_cast<qint64>(stringToUint64(object.value("totalFiles")));
    manifest.totalBytes = static_cast<qint64>(stringToUint64(object.value("totalBytes")));
    manifest.dataRelativePath = object.value("dataDir").toString("data");
    manifest.files = deserializeFiles(object.value("files").toArray());
    return manifest;
}
