#include "test_utils.h"
#include "BackupTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

class BackupTypesTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ==================== FileKind Tests ====================

TEST_F(BackupTypesTest, FileKindToString)
{
    EXPECT_EQ(fileKindToString(FileKind::Regular), "regular");
    EXPECT_EQ(fileKindToString(FileKind::Directory), "directory");
    EXPECT_EQ(fileKindToString(FileKind::Symlink), "symlink");
    EXPECT_EQ(fileKindToString(FileKind::Fifo), "fifo");
    EXPECT_EQ(fileKindToString(FileKind::Block), "block");
    EXPECT_EQ(fileKindToString(FileKind::Character), "character");
    EXPECT_EQ(fileKindToString(FileKind::Socket), "socket");
    EXPECT_EQ(fileKindToString(FileKind::Unknown), "unknown");
}

TEST_F(BackupTypesTest, FileKindFromString)
{
    EXPECT_EQ(fileKindFromString("regular"), FileKind::Regular);
    EXPECT_EQ(fileKindFromString("directory"), FileKind::Directory);
    EXPECT_EQ(fileKindFromString("symlink"), FileKind::Symlink);
    EXPECT_EQ(fileKindFromString("fifo"), FileKind::Fifo);
    EXPECT_EQ(fileKindFromString("block"), FileKind::Block);
    EXPECT_EQ(fileKindFromString("character"), FileKind::Character);
    EXPECT_EQ(fileKindFromString("socket"), FileKind::Socket);
    EXPECT_EQ(fileKindFromString("unknown"), FileKind::Unknown);
    EXPECT_EQ(fileKindFromString("invalid"), FileKind::Unknown);
    EXPECT_EQ(fileKindFromString(""), FileKind::Unknown);
}

TEST_F(BackupTypesTest, FileKindRoundTrip)
{
    for (auto kind : {FileKind::Regular, FileKind::Directory, FileKind::Symlink,
                      FileKind::Fifo, FileKind::Block, FileKind::Character,
                      FileKind::Socket, FileKind::Unknown})
    {
        QString str = fileKindToString(kind);
        EXPECT_EQ(fileKindFromString(str), kind);
    }
}

// ==================== CompressionMethod Tests ====================

TEST_F(BackupTypesTest, CompressionMethodToString)
{
    EXPECT_EQ(compressionMethodToString(CompressionMethod::None), "none");
    EXPECT_EQ(compressionMethodToString(CompressionMethod::Huffman), "huffman");
    EXPECT_EQ(compressionMethodToString(CompressionMethod::RLE), "rle");
    EXPECT_EQ(compressionMethodToString(CompressionMethod::Zlib), "zlib");
}

TEST_F(BackupTypesTest, CompressionMethodFromString)
{
    EXPECT_EQ(compressionMethodFromString("none"), CompressionMethod::None);
    EXPECT_EQ(compressionMethodFromString("huffman"), CompressionMethod::Huffman);
    EXPECT_EQ(compressionMethodFromString("rle"), CompressionMethod::RLE);
    EXPECT_EQ(compressionMethodFromString("zlib"), CompressionMethod::Zlib);
    EXPECT_EQ(compressionMethodFromString("invalid"), CompressionMethod::None);
    EXPECT_EQ(compressionMethodFromString(""), CompressionMethod::None);
}

TEST_F(BackupTypesTest, CompressionMethodRoundTrip)
{
    for (auto method : {CompressionMethod::None, CompressionMethod::Huffman,
                        CompressionMethod::RLE, CompressionMethod::Zlib})
    {
        QString str = compressionMethodToString(method);
        EXPECT_EQ(compressionMethodFromString(str), method);
    }
}

// ==================== EncryptionMethod Tests ====================

TEST_F(BackupTypesTest, EncryptionMethodToString)
{
    EXPECT_EQ(encryptionMethodToString(EncryptionMethod::None), "none");
    EXPECT_EQ(encryptionMethodToString(EncryptionMethod::XOR), "xor");
    EXPECT_EQ(encryptionMethodToString(EncryptionMethod::RC4), "rc4");
    EXPECT_EQ(encryptionMethodToString(EncryptionMethod::AES256), "aes256");
}

TEST_F(BackupTypesTest, EncryptionMethodFromString)
{
    EXPECT_EQ(encryptionMethodFromString("none"), EncryptionMethod::None);
    EXPECT_EQ(encryptionMethodFromString("xor"), EncryptionMethod::XOR);
    EXPECT_EQ(encryptionMethodFromString("rc4"), EncryptionMethod::RC4);
    EXPECT_EQ(encryptionMethodFromString("aes256"), EncryptionMethod::AES256);
    EXPECT_EQ(encryptionMethodFromString("invalid"), EncryptionMethod::None);
    EXPECT_EQ(encryptionMethodFromString(""), EncryptionMethod::None);
}

TEST_F(BackupTypesTest, EncryptionMethodRoundTrip)
{
    for (auto method : {EncryptionMethod::None, EncryptionMethod::XOR,
                        EncryptionMethod::RC4, EncryptionMethod::AES256})
    {
        QString str = encryptionMethodToString(method);
        EXPECT_EQ(encryptionMethodFromString(str), method);
    }
}

// ==================== FileRecord Tests ====================

TEST_F(BackupTypesTest, FileRecordDefaultValues)
{
    FileRecord record;
    EXPECT_TRUE(record.relativePath.isEmpty());
    EXPECT_EQ(record.kind, FileKind::Unknown);
    EXPECT_EQ(record.size, 0u);
    EXPECT_EQ(record.storedSize, 0u);
    EXPECT_TRUE(record.hash.isEmpty());
    EXPECT_EQ(record.permissions, 0u);
    EXPECT_FALSE(record.hasData);
    EXPECT_FALSE(record.metadataOnly);
}

TEST_F(BackupTypesTest, FileRecordToJson)
{
    FileRecord record;
    record.relativePath = "test/file.txt";
    record.kind = FileKind::Regular;
    record.size = 1024;
    record.storedSize = 512;
    record.hash = "abc123";
    record.storedHash = "def456";
    record.permissions = 0644;
    record.ownerId = 1000;
    record.groupId = 1000;
    record.ownerName = "testuser";
    record.groupName = "testgroup";
    record.createdAt = 1000000;
    record.modifiedAt = 2000000;
    record.accessedAt = 3000000;
    record.hasData = true;
    record.dataOffset = 100;

    QJsonObject json = fileRecordToJson(record);

    // Actual JSON field names from BackupTypes.cpp
    EXPECT_EQ(json["path"].toString(), "test/file.txt");
    EXPECT_EQ(json["type"].toString(), "regular");
    EXPECT_EQ(json["size"].toString(), "1024");
    EXPECT_EQ(json["storedSize"].toString(), "512");
    EXPECT_EQ(json["hash"].toString(), "abc123");
    EXPECT_EQ(json["storedHash"].toString(), "def456");
    EXPECT_EQ(json["permissions"].toString(), "420"); // 0644 octal = 420 decimal
    EXPECT_EQ(json["ownerId"].toString(), "1000");
    EXPECT_EQ(json["groupId"].toString(), "1000");
    EXPECT_EQ(json["owner"].toString(), "testuser");
    EXPECT_EQ(json["group"].toString(), "testgroup");
    EXPECT_EQ(json["hasData"].toBool(), true);
}

TEST_F(BackupTypesTest, FileRecordFromJson)
{
    QJsonObject json;
    // Use actual field names from BackupTypes.cpp
    json["path"] = "test/file.txt";
    json["type"] = "regular";
    json["size"] = "1024";
    json["storedSize"] = "512";
    json["hash"] = "abc123";
    json["storedHash"] = "def456";
    json["permissions"] = "420"; // 0644 octal
    json["ownerId"] = "1000";
    json["groupId"] = "1000";
    json["owner"] = "testuser";
    json["group"] = "testgroup";
    json["created"] = "1000000";
    json["modified"] = "2000000";
    json["accessed"] = "3000000";
    json["hasData"] = true;
    json["dataOffset"] = "100";

    FileRecord record = fileRecordFromJson(json);

    EXPECT_EQ(record.relativePath, "test/file.txt");
    EXPECT_EQ(record.kind, FileKind::Regular);
    EXPECT_EQ(record.size, 1024u);
    EXPECT_EQ(record.storedSize, 512u);
    EXPECT_EQ(record.hash, "abc123");
    EXPECT_EQ(record.storedHash, "def456");
    EXPECT_EQ(record.permissions, 420u); // 0644 octal = 420 decimal
    EXPECT_EQ(record.ownerId, 1000u);
    EXPECT_EQ(record.groupId, 1000u);
    EXPECT_EQ(record.ownerName, "testuser");
    EXPECT_EQ(record.groupName, "testgroup");
    EXPECT_TRUE(record.hasData);
}

TEST_F(BackupTypesTest, FileRecordRoundTrip)
{
    FileRecord original;
    original.relativePath = "subdir/test.bin";
    original.kind = FileKind::Regular;
    original.size = 2048;
    original.storedSize = 1024;
    original.hash = "hash123";
    original.storedHash = "storedhash456";
    original.permissions = 0755;
    original.ownerId = 500;
    original.groupId = 500;
    original.ownerName = "user";
    original.groupName = "group";
    original.createdAt = 100;
    original.modifiedAt = 200;
    original.accessedAt = 300;
    original.hasData = true;
    original.dataOffset = 50;

    QJsonObject json = fileRecordToJson(original);
    FileRecord restored = fileRecordFromJson(json);

    EXPECT_EQ(restored.relativePath, original.relativePath);
    EXPECT_EQ(restored.kind, original.kind);
    EXPECT_EQ(restored.size, original.size);
    EXPECT_EQ(restored.storedSize, original.storedSize);
    EXPECT_EQ(restored.hash, original.hash);
    EXPECT_EQ(restored.storedHash, original.storedHash);
    EXPECT_EQ(restored.permissions, original.permissions);
    EXPECT_EQ(restored.ownerId, original.ownerId);
    EXPECT_EQ(restored.groupId, original.groupId);
    EXPECT_EQ(restored.hasData, original.hasData);
}

// ==================== BackupManifest Tests ====================

TEST_F(BackupTypesTest, BackupManifestDefaultValues)
{
    BackupManifest manifest;
    EXPECT_EQ(manifest.manifestVersion, "1.0");
    EXPECT_TRUE(manifest.backupId.isEmpty());
    EXPECT_EQ(manifest.storageMode, StorageMode::Directory);
    EXPECT_FALSE(manifest.compressed);
    EXPECT_FALSE(manifest.encrypted);
    EXPECT_EQ(manifest.compressionMethod, CompressionMethod::None);
    EXPECT_EQ(manifest.encryptionMethod, EncryptionMethod::None);
    EXPECT_TRUE(manifest.preserveMetadata);
    EXPECT_TRUE(manifest.includeSpecialFiles);
    EXPECT_TRUE(manifest.verificationEnabled);
    EXPECT_EQ(manifest.totalFiles, 0);
    EXPECT_EQ(manifest.totalBytes, 0);
}

TEST_F(BackupTypesTest, BackupManifestToJson)
{
    BackupManifest manifest;
    manifest.backupId = "backup-001";
    manifest.rootName = "testroot";
    manifest.createdAt = "2024-01-01T00:00:00Z";
    manifest.sourcePath = "/source/path";
    manifest.storageMode = StorageMode::Package;
    manifest.compressed = true;
    manifest.encrypted = true;
    manifest.compressionMethod = CompressionMethod::Zlib;
    manifest.encryptionMethod = EncryptionMethod::AES256;
    manifest.preserveMetadata = true;
    manifest.includeSpecialFiles = false;
    manifest.verificationEnabled = true;
    manifest.totalFiles = 100;
    manifest.totalBytes = 1024000;

    FileRecord file1;
    file1.relativePath = "file1.txt";
    file1.kind = FileKind::Regular;
    file1.size = 100;
    manifest.files.push_back(file1);

    QJsonObject json = manifestToJson(manifest);

    // Actual JSON field names from BackupTypes.cpp
    EXPECT_EQ(json["backupId"].toString(), "backup-001");
    EXPECT_EQ(json["rootName"].toString(), "testroot");
    EXPECT_EQ(json["createdAt"].toString(), "2024-01-01T00:00:00Z");
    EXPECT_EQ(json["sourcePath"].toString(), "/source/path");
    EXPECT_EQ(json["storageMode"].toString(), "package");
    EXPECT_TRUE(json["compressed"].toBool());
    EXPECT_TRUE(json["encrypted"].toBool());
    EXPECT_EQ(json["compressionMethod"].toString(), "zlib");
    EXPECT_EQ(json["encryptionMethod"].toString(), "aes256");
    EXPECT_TRUE(json["preserveMetadata"].toBool());
    EXPECT_FALSE(json["includeSpecial"].toBool()); // actual field name
    EXPECT_EQ(json["totalFiles"].toString(), "100");
    EXPECT_EQ(json["totalBytes"].toString(), "1024000");
    EXPECT_TRUE(json["files"].isArray());
    EXPECT_EQ(json["files"].toArray().size(), 1);
}

TEST_F(BackupTypesTest, BackupManifestFromJson)
{
    QJsonObject json;
    // Use actual field names from BackupTypes.cpp
    json["version"] = "1.0";
    json["backupId"] = "backup-002";
    json["rootName"] = "root";
    json["createdAt"] = "2024-06-15T12:00:00Z";
    json["sourcePath"] = "/data/source";
    json["storageMode"] = "directory";
    json["compressed"] = true;
    json["encrypted"] = false;
    json["compressionMethod"] = "huffman";
    json["encryptionMethod"] = "none";
    json["preserveMetadata"] = false;
    json["includeSpecial"] = true;
    json["verify"] = true;
    json["totalFiles"] = "50";
    json["totalBytes"] = "512000";
    json["dataDir"] = "data";
    json["files"] = QJsonArray();

    BackupManifest manifest = manifestFromJson(json);

    EXPECT_EQ(manifest.backupId, "backup-002");
    EXPECT_EQ(manifest.rootName, "root");
    EXPECT_EQ(manifest.createdAt, "2024-06-15T12:00:00Z");
    EXPECT_EQ(manifest.sourcePath, "/data/source");
    EXPECT_EQ(manifest.storageMode, StorageMode::Directory);
    EXPECT_TRUE(manifest.compressed);
    EXPECT_FALSE(manifest.encrypted);
    EXPECT_EQ(manifest.compressionMethod, CompressionMethod::Huffman);
    // Note: when encrypted=false but encryptionMethod=none, it stays None
    EXPECT_FALSE(manifest.preserveMetadata);
    EXPECT_TRUE(manifest.includeSpecialFiles);
    EXPECT_EQ(manifest.totalFiles, 50);
    EXPECT_EQ(manifest.totalBytes, 512000);
}

TEST_F(BackupTypesTest, BackupManifestRoundTrip)
{
    BackupManifest original;
    original.manifestVersion = "1.0";
    original.backupId = "test-backup";
    original.rootName = "testdir";
    original.createdAt = "2024-03-15T08:30:00Z";
    original.sourcePath = "/home/user/data";
    original.storageMode = StorageMode::Package;
    original.compressed = true;
    original.encrypted = true;
    original.compressionMethod = CompressionMethod::RLE;
    original.encryptionMethod = EncryptionMethod::RC4;
    original.preserveMetadata = true;
    original.includeSpecialFiles = true;
    original.verificationEnabled = false;
    original.totalFiles = 25;
    original.totalBytes = 256000;
    original.dataRelativePath = "backup_data";

    FileRecord file;
    file.relativePath = "document.pdf";
    file.kind = FileKind::Regular;
    file.size = 10000;
    file.storedSize = 8000;
    file.hash = "filehash";
    original.files.push_back(file);

    QJsonObject json = manifestToJson(original);
    BackupManifest restored = manifestFromJson(json);

    EXPECT_EQ(restored.manifestVersion, original.manifestVersion);
    EXPECT_EQ(restored.backupId, original.backupId);
    EXPECT_EQ(restored.rootName, original.rootName);
    EXPECT_EQ(restored.createdAt, original.createdAt);
    EXPECT_EQ(restored.sourcePath, original.sourcePath);
    EXPECT_EQ(restored.storageMode, original.storageMode);
    EXPECT_EQ(restored.compressed, original.compressed);
    EXPECT_EQ(restored.encrypted, original.encrypted);
    EXPECT_EQ(restored.compressionMethod, original.compressionMethod);
    EXPECT_EQ(restored.encryptionMethod, original.encryptionMethod);
    EXPECT_EQ(restored.preserveMetadata, original.preserveMetadata);
    EXPECT_EQ(restored.includeSpecialFiles, original.includeSpecialFiles);
    EXPECT_EQ(restored.totalFiles, original.totalFiles);
    EXPECT_EQ(restored.totalBytes, original.totalBytes);
    EXPECT_EQ(restored.files.size(), original.files.size());
}

// ==================== Edge Cases ====================

TEST_F(BackupTypesTest, FileRecordWithSymlink)
{
    FileRecord record;
    record.relativePath = "link";
    record.kind = FileKind::Symlink;
    record.symlinkTarget = "/target/path";
    record.hasData = false;

    QJsonObject json = fileRecordToJson(record);
    FileRecord restored = fileRecordFromJson(json);

    EXPECT_EQ(restored.kind, FileKind::Symlink);
    EXPECT_EQ(restored.symlinkTarget, "/target/path");
    EXPECT_FALSE(restored.hasData);
}

TEST_F(BackupTypesTest, FileRecordWithSpecialDevice)
{
    FileRecord record;
    record.relativePath = "device";
    record.kind = FileKind::Block;
    record.specialDevice = 0x0803; // /dev/sda3
    record.hasData = false;

    QJsonObject json = fileRecordToJson(record);
    FileRecord restored = fileRecordFromJson(json);

    EXPECT_EQ(restored.kind, FileKind::Block);
    EXPECT_EQ(restored.specialDevice, 0x0803u);
}

TEST_F(BackupTypesTest, EmptyManifestFiles)
{
    BackupManifest manifest;
    manifest.backupId = "empty-backup";
    manifest.totalFiles = 0;

    QJsonObject json = manifestToJson(manifest);
    BackupManifest restored = manifestFromJson(json);

    EXPECT_TRUE(restored.files.isEmpty());
    EXPECT_EQ(restored.totalFiles, 0);
}

TEST_F(BackupTypesTest, ManifestWithMultipleFiles)
{
    BackupManifest manifest;
    manifest.backupId = "multi-file-backup";

    for (int i = 0; i < 10; ++i)
    {
        FileRecord file;
        file.relativePath = QString("file%1.txt").arg(i);
        file.kind = FileKind::Regular;
        file.size = i * 100;
        manifest.files.push_back(file);
    }
    manifest.totalFiles = manifest.files.size();

    QJsonObject json = manifestToJson(manifest);
    BackupManifest restored = manifestFromJson(json);

    EXPECT_EQ(restored.files.size(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(restored.files[i].relativePath, QString("file%1.txt").arg(i));
        EXPECT_EQ(restored.files[i].size, static_cast<quint64>(i * 100));
    }
}

TEST_F(BackupTypesTest, UnicodeInPaths)
{
    FileRecord record;
    record.relativePath = QString::fromUtf8("文件夹/测试文件.txt");
    record.kind = FileKind::Regular;

    QJsonObject json = fileRecordToJson(record);
    FileRecord restored = fileRecordFromJson(json);

    EXPECT_EQ(restored.relativePath, QString::fromUtf8("文件夹/测试文件.txt"));
}

TEST_F(BackupTypesTest, SpecialCharactersInPaths)
{
    FileRecord record;
    record.relativePath = "path with spaces/file-name_v2.0.txt";
    record.kind = FileKind::Regular;

    QJsonObject json = fileRecordToJson(record);
    FileRecord restored = fileRecordFromJson(json);

    EXPECT_EQ(restored.relativePath, "path with spaces/file-name_v2.0.txt");
}
