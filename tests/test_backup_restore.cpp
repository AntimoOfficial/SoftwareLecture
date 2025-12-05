#include "test_utils.h"
#include "BackupEngine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <gtest/gtest.h>

// ==================== Basic Backup/Restore Tests ====================

class BackupRestoreBasicTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreBasicTest, BackupSingleFile)
{
    m_sourceDir->createFile("test.txt", "Hello World");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.backupName = "single_file_backup";

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_FALSE(result.location.isEmpty());
    EXPECT_GE(result.processedFiles, 1);
}

TEST_F(BackupRestoreBasicTest, BackupMultipleFiles)
{
    createTestFiles();

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.backupName = "multi_file_backup";

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_GE(result.processedFiles, 4);
}

TEST_F(BackupRestoreBasicTest, BackupNestedDirectories)
{
    m_sourceDir->createFile("level1/level2/level3/deep.txt", "Deep file");
    m_sourceDir->createFile("level1/shallow.txt", "Shallow file");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreBasicTest, BackupEmptyDirectory)
{
    m_sourceDir->createDirectory("empty_dir");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreBasicTest, RestoreBasic)
{
    m_sourceDir->createFile("restore_test.txt", "Content to restore");

    BackupEngine engine;
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    RestoreOptions restoreOptions;
    restoreOptions.backupPath = backupResult.location;
    restoreOptions.restoreDestination = restoreDir();

    RestoreResult restoreResult = engine.restore(restoreOptions);

    EXPECT_TRUE(restoreResult.success) << restoreResult.error.toStdString();
    EXPECT_GE(restoreResult.restoredFiles, 1);
}

TEST_F(BackupRestoreBasicTest, VerifyBackup)
{
    m_sourceDir->createFile("verify_test.txt", "Content to verify");

    BackupEngine engine;
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.verify = false; // Disable auto-verify

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    VerifyOptions verifyOptions;
    verifyOptions.backupPath = backupResult.location;

    VerifyResult verifyResult = engine.verify(verifyOptions);

    EXPECT_TRUE(verifyResult.success) << verifyResult.error.toStdString();
    EXPECT_EQ(verifyResult.failedFiles, 0);
}

// ==================== Compression Integration Tests ====================

class BackupRestoreCompressionTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreCompressionTest, BackupWithHuffmanCompression)
{
    m_sourceDir->createFile("text.txt", TestData::textData(100));

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.compress = true;
    options.compressionMethod = CompressionMethod::Huffman;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreCompressionTest, BackupWithRLECompression)
{
    // RLE works best with repeated data
    m_sourceDir->createFile("repeated.dat", QByteArray(1000, 'A'));

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.compress = true;
    options.compressionMethod = CompressionMethod::RLE;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreCompressionTest, BackupWithZlibCompression)
{
    m_sourceDir->createFile("data.bin", TestData::binaryData(5000));

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.compress = true;
    options.compressionMethod = CompressionMethod::Zlib;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreCompressionTest, RestoreCompressedBackup)
{
    QByteArray originalContent = TestData::textData(50);
    m_sourceDir->createFile("compressed.txt", originalContent);

    BackupEngine engine;

    // Backup with compression
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.compress = true;
    backupOptions.compressionMethod = CompressionMethod::Zlib;

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    // Restore
    RestoreOptions restoreOptions;
    restoreOptions.backupPath = backupResult.location;
    restoreOptions.restoreDestination = restoreDir();

    RestoreResult restoreResult = engine.restore(restoreOptions);
    EXPECT_TRUE(restoreResult.success) << restoreResult.error.toStdString();
}

// ==================== Encryption Integration Tests ====================

class BackupRestoreEncryptionTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreEncryptionTest, BackupWithXOREncryption)
{
    m_sourceDir->createFile("secret.txt", "Secret content");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;
    options.password = "xorpassword";

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreEncryptionTest, BackupWithRC4Encryption)
{
    m_sourceDir->createFile("secret.txt", "Secret content");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::RC4;
    options.password = "rc4password";

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreEncryptionTest, BackupWithAES256Encryption)
{
    m_sourceDir->createFile("topsecret.txt", "Top secret content");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::AES256;
    options.password = "aes256strongpassword";

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreEncryptionTest, RestoreEncryptedBackup)
{
    QString password = "decryptme";
    m_sourceDir->createFile("encrypted.txt", "Encrypted content");

    BackupEngine engine;

    // Backup with encryption
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.encrypt = true;
    backupOptions.encryptionMethod = EncryptionMethod::AES256;
    backupOptions.password = password;

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    // Restore with correct password
    RestoreOptions restoreOptions;
    restoreOptions.backupPath = backupResult.location;
    restoreOptions.restoreDestination = restoreDir();
    restoreOptions.password = password;

    RestoreResult restoreResult = engine.restore(restoreOptions);
    EXPECT_TRUE(restoreResult.success) << restoreResult.error.toStdString();
}

TEST_F(BackupRestoreEncryptionTest, RestoreWithWrongPassword)
{
    m_sourceDir->createFile("encrypted.txt", "Encrypted content");

    BackupEngine engine;

    // Backup with encryption
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.encrypt = true;
    backupOptions.encryptionMethod = EncryptionMethod::AES256;
    backupOptions.password = "correctpassword";
    backupOptions.verify = false;

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    // Attempt restore with wrong password
    RestoreOptions restoreOptions;
    restoreOptions.backupPath = backupResult.location;
    restoreOptions.restoreDestination = restoreDir();
    restoreOptions.password = "wrongpassword";

    // This may succeed but data will be corrupted
    RestoreResult restoreResult = engine.restore(restoreOptions);
    // The restore may "succeed" but verification should fail
}

TEST_F(BackupRestoreEncryptionTest, VerifyEncryptedBackup)
{
    QString password = "verifyme";
    m_sourceDir->createFile("verify.txt", "Content to verify");

    BackupEngine engine;

    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.encrypt = true;
    backupOptions.encryptionMethod = EncryptionMethod::RC4;
    backupOptions.password = password;
    backupOptions.verify = false;

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    VerifyOptions verifyOptions;
    verifyOptions.backupPath = backupResult.location;
    verifyOptions.password = password;

    VerifyResult verifyResult = engine.verify(verifyOptions);
    EXPECT_TRUE(verifyResult.success) << verifyResult.error.toStdString();
}

// ==================== Package Mode Tests ====================

class BackupRestorePackageTest : public BackupTestFixture
{
};

TEST_F(BackupRestorePackageTest, BackupAsPackage)
{
    createTestFiles();

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.package = true;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_TRUE(result.location.endsWith(".fbk"));
    EXPECT_TRUE(QFileInfo::exists(result.location));
}

TEST_F(BackupRestorePackageTest, RestoreFromPackage)
{
    QByteArray content = "Package content";
    m_sourceDir->createFile("packaged.txt", content);

    BackupEngine engine;

    // Create package backup
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.package = true;

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    // Restore from package
    RestoreOptions restoreOptions;
    restoreOptions.backupPath = backupResult.location;
    restoreOptions.restoreDestination = restoreDir();

    RestoreResult restoreResult = engine.restore(restoreOptions);
    EXPECT_TRUE(restoreResult.success) << restoreResult.error.toStdString();
}

TEST_F(BackupRestorePackageTest, PackageWithCompression)
{
    m_sourceDir->createFile("data.txt", TestData::textData(100));

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.package = true;
    options.compress = true;
    options.compressionMethod = CompressionMethod::Zlib;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestorePackageTest, PackageWithEncryption)
{
    m_sourceDir->createFile("secret.txt", "Secret in package");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.package = true;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::AES256;
    options.password = "packagepassword";

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestorePackageTest, PackageWithCompressionAndEncryption)
{
    m_sourceDir->createFile("combined.txt", TestData::textData(50));

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.package = true;
    options.compress = true;
    options.compressionMethod = CompressionMethod::Zlib;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::AES256;
    options.password = "combinedpassword";

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

// ==================== Error Handling Tests ====================

class BackupRestoreErrorTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreErrorTest, BackupNonExistentSource)
{
    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = "/nonexistent/path/to/backup";
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.isEmpty());
}

TEST_F(BackupRestoreErrorTest, BackupEmptySourcePath)
{
    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = "";
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_FALSE(result.success);
}

TEST_F(BackupRestoreErrorTest, BackupEmptyDestinationPath)
{
    m_sourceDir->createFile("test.txt", "content");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = "";

    BackupResult result = engine.performBackup(options);

    EXPECT_FALSE(result.success);
}

TEST_F(BackupRestoreErrorTest, EncryptionWithoutPassword)
{
    m_sourceDir->createFile("test.txt", "content");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::AES256;
    options.password = ""; // Empty password

    BackupResult result = engine.performBackup(options);

    EXPECT_FALSE(result.success);
}

TEST_F(BackupRestoreErrorTest, RestoreNonExistentBackup)
{
    BackupEngine engine;
    RestoreOptions options;
    options.backupPath = "/nonexistent/backup.fbk";
    options.restoreDestination = restoreDir();

    RestoreResult result = engine.restore(options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.isEmpty());
}

TEST_F(BackupRestoreErrorTest, VerifyNonExistentBackup)
{
    BackupEngine engine;
    VerifyOptions options;
    options.backupPath = "/nonexistent/backup";

    VerifyResult result = engine.verify(options);

    EXPECT_FALSE(result.success);
}

// ==================== Metadata Tests ====================

class BackupRestoreMetadataTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreMetadataTest, PreserveMetadataEnabled)
{
    m_sourceDir->createFile("meta.txt", "Content with metadata");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.preserveMetadata = true;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreMetadataTest, PreserveMetadataDisabled)
{
    m_sourceDir->createFile("nometa.txt", "Content without metadata");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.preserveMetadata = false;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

// ==================== Special Files Tests ====================

class BackupRestoreSpecialFilesTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreSpecialFilesTest, IncludeSpecialFiles)
{
    m_sourceDir->createFile("regular.txt", "Regular file");
#ifdef Q_OS_UNIX
    m_sourceDir->createSymlink("link.txt", "regular.txt");
#endif

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.includeSpecialFiles = true;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreSpecialFilesTest, ExcludeSpecialFiles)
{
    m_sourceDir->createFile("regular.txt", "Regular file");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.includeSpecialFiles = false;

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

// ==================== Retention Tests ====================

class BackupRetentionTest : public BackupTestFixture
{
};

TEST_F(BackupRetentionTest, RetentionCountEnforced)
{
    m_sourceDir->createFile("data.txt", "Test data");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();
    options.retentionCount = 3;

    // Create multiple backups
    for (int i = 0; i < 5; ++i)
    {
        options.backupName = QString("backup_%1").arg(i);
        BackupResult result = engine.performBackup(options);
        EXPECT_TRUE(result.success) << result.error.toStdString();
    }

    // Count backups in destination
    QDir dest(destDir());
    QStringList entries = dest.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    // Should have at most retentionCount backups
    EXPECT_LE(entries.size(), options.retentionCount);
}

// ==================== Data Integrity Tests ====================

class BackupRestoreIntegrityTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreIntegrityTest, ContentIntegrityAfterBackupRestore)
{
    QByteArray originalContent = TestData::randomData(1024);
    m_sourceDir->createFile("integrity.bin", originalContent);

    BackupEngine engine;

    // Backup
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    // Restore
    RestoreOptions restoreOptions;
    restoreOptions.backupPath = backupResult.location;
    restoreOptions.restoreDestination = restoreDir();

    RestoreResult restoreResult = engine.restore(restoreOptions);
    ASSERT_TRUE(restoreResult.success) << restoreResult.error.toStdString();

    // Verify content
    // The restored file will be in a subdirectory named after the source
    QDir restoredDir(restoreDir());
    QStringList subdirs = restoredDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (!subdirs.isEmpty())
    {
        QString restoredFilePath = restoredDir.filePath(subdirs.first() + "/integrity.bin");
        if (QFileInfo::exists(restoredFilePath))
        {
            QFile restoredFile(restoredFilePath);
            ASSERT_TRUE(restoredFile.open(QIODevice::ReadOnly));
            QByteArray restoredContent = restoredFile.readAll();
            EXPECT_EQ(restoredContent, originalContent);
        }
    }
}

TEST_F(BackupRestoreIntegrityTest, HashVerification)
{
    QByteArray content = TestData::textData(100);
    m_sourceDir->createFile("hashtest.txt", content);

    BackupEngine engine;

    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.verify = true;

    BackupResult result = engine.performBackup(backupOptions);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreIntegrityTest, CompressedContentIntegrity)
{
    QByteArray originalContent = TestData::textData(200);
    m_sourceDir->createFile("compressed.txt", originalContent);

    BackupEngine engine;

    // Backup with compression
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.compress = true;
    backupOptions.compressionMethod = CompressionMethod::Zlib;

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    // Verify
    VerifyOptions verifyOptions;
    verifyOptions.backupPath = backupResult.location;

    VerifyResult verifyResult = engine.verify(verifyOptions);
    EXPECT_TRUE(verifyResult.success) << verifyResult.error.toStdString();
}

TEST_F(BackupRestoreIntegrityTest, EncryptedContentIntegrity)
{
    QString password = "integritypass";
    QByteArray originalContent = TestData::binaryData(512);
    m_sourceDir->createFile("encrypted.bin", originalContent);

    BackupEngine engine;

    // Backup with encryption
    BackupOptions backupOptions;
    backupOptions.sourcePath = sourceDir();
    backupOptions.destinationPath = destDir();
    backupOptions.encrypt = true;
    backupOptions.encryptionMethod = EncryptionMethod::AES256;
    backupOptions.password = password;

    BackupResult backupResult = engine.performBackup(backupOptions);
    ASSERT_TRUE(backupResult.success) << backupResult.error.toStdString();

    // Verify with correct password
    VerifyOptions verifyOptions;
    verifyOptions.backupPath = backupResult.location;
    verifyOptions.password = password;

    VerifyResult verifyResult = engine.verify(verifyOptions);
    EXPECT_TRUE(verifyResult.success) << verifyResult.error.toStdString();
}

// ==================== Large File Tests ====================

class BackupRestoreLargeFileTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreLargeFileTest, BackupLargeFile)
{
    // Create a 1MB file
    QByteArray largeContent = TestData::randomData(1024 * 1024);
    m_sourceDir->createFile("large.bin", largeContent);

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_GE(result.totalBytes, 1024 * 1024);
}

TEST_F(BackupRestoreLargeFileTest, BackupManySmallFiles)
{
    // Create many small files
    for (int i = 0; i < 100; ++i)
    {
        m_sourceDir->createFile(QString("file_%1.txt").arg(i), QString("Content %1").arg(i).toUtf8());
    }

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
    EXPECT_GE(result.processedFiles, 100);
}

// ==================== Unicode and Special Characters ====================

class BackupRestoreUnicodeTest : public BackupTestFixture
{
};

TEST_F(BackupRestoreUnicodeTest, UnicodeFilenames)
{
    m_sourceDir->createFile(QString::fromUtf8("中文文件.txt"), "Chinese filename");
    m_sourceDir->createFile(QString::fromUtf8("日本語.txt"), "Japanese filename");
    m_sourceDir->createFile(QString::fromUtf8("한국어.txt"), "Korean filename");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreUnicodeTest, UnicodeContent)
{
    QString content = QString::fromUtf8("你好世界！こんにちは！안녕하세요！🎉");
    m_sourceDir->createFile("unicode_content.txt", content.toUtf8());

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}

TEST_F(BackupRestoreUnicodeTest, SpecialCharactersInPath)
{
    m_sourceDir->createFile("path with spaces/file.txt", "Content");
    m_sourceDir->createFile("special-chars_v2.0/data.txt", "More content");

    BackupEngine engine;
    BackupOptions options;
    options.sourcePath = sourceDir();
    options.destinationPath = destDir();

    BackupResult result = engine.performBackup(options);

    EXPECT_TRUE(result.success) << result.error.toStdString();
}
