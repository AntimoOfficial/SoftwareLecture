#pragma once

#include <QFile>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <cstdint>
#include <functional>

enum class FileKind
{
    Regular,
    Directory,
    Symlink,
    Fifo,
    Block,
    Character,
    Socket,
    Unknown
};

enum class StorageMode
{
    Directory,
    Package
};

// 压缩方法枚举
enum class CompressionMethod
{
    None,       // 不压缩
    Huffman,    // 哈夫曼编码
    RLE,        // 游程编码 (Run-Length Encoding)
    Zlib        // zlib 压缩 (使用 Qt 内置)
};

// 加密方法枚举
enum class EncryptionMethod
{
    None,       // 不加密
    XOR,        // 简单异或加密
    RC4,        // RC4 流加密
    AES256      // AES-256 加密 (简化实现)
};

struct FileRecord
{
    QString relativePath;
    FileKind kind = FileKind::Unknown;
    quint64 size = 0;
    quint64 storedSize = 0;
    QString hash;
    QString storedHash;
    quint32 permissions = 0;
    quint32 ownerId = 0;
    quint32 groupId = 0;
    QString ownerName;
    QString groupName;
    qint64 createdAt = 0;
    qint64 modifiedAt = 0;
    qint64 accessedAt = 0;
    QString symlinkTarget;
    quint64 specialDevice = 0;
    bool hasData = false;
    bool metadataOnly = false;
    quint64 dataOffset = 0;
    QString storedRelativePath;
};

struct BackupManifest
{
    QString manifestVersion = "1.0";
    QString backupId;
    QString rootName;
    QString createdAt;
    QString sourcePath;
    StorageMode storageMode = StorageMode::Directory;
    bool compressed = false;
    bool encrypted = false;
    CompressionMethod compressionMethod = CompressionMethod::None;
    EncryptionMethod encryptionMethod = EncryptionMethod::None;
    bool preserveMetadata = true;
    bool includeSpecialFiles = true;
    bool verificationEnabled = true;
    qint64 totalFiles = 0;
    qint64 totalBytes = 0;
    QString dataRelativePath = QStringLiteral("data");
    QVector<FileRecord> files;

    // runtime info (not serialized)
    QString manifestPath;
    QString basePath;
    QString dataDirectory;
    QString packagePath;
};

struct BackupOptions
{
    QString sourcePath;
    QString destinationPath;
    QString backupName;
    bool compress = false;
    bool encrypt = false;
    CompressionMethod compressionMethod = CompressionMethod::Huffman;
    EncryptionMethod encryptionMethod = EncryptionMethod::XOR;
    bool package = false;
    bool preserveMetadata = true;
    bool includeSpecialFiles = true;
    bool verify = true;
    QString password;
    int retentionCount = 0;
};

struct RestoreOptions
{
    QString backupPath;
    QString restoreDestination;
    QString password;
};

struct VerifyOptions
{
    QString backupPath;
    QString password;
};

struct BackupResult
{
    bool success = false;
    QString location;
    QString error;
    int processedFiles = 0;
    qint64 totalBytes = 0;
};

struct RestoreResult
{
    bool success = false;
    QString targetPath;
    QString error;
    int restoredFiles = 0;
};

struct VerifyResult
{
    bool success = false;
    QString error;
    int checkedFiles = 0;
    int failedFiles = 0;
};

struct BackupEngineCallbacks
{
    std::function<void(int current, int total)> progress;
    std::function<void(const QString &message)> log;
};

QString fileKindToString(FileKind kind);
FileKind fileKindFromString(const QString &value);

QString compressionMethodToString(CompressionMethod method);
CompressionMethod compressionMethodFromString(const QString &value);

QString encryptionMethodToString(EncryptionMethod method);
EncryptionMethod encryptionMethodFromString(const QString &value);

QJsonObject fileRecordToJson(const FileRecord &record);
FileRecord fileRecordFromJson(const QJsonObject &object);

QJsonObject manifestToJson(const BackupManifest &manifest);
BackupManifest manifestFromJson(const QJsonObject &object);
