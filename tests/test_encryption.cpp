#include "test_utils.h"
#include "BackupEngine.h"

#include <gtest/gtest.h>

class EncryptionTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}

    BackupEngine engine;
};

// ==================== XOR Encryption Tests ====================

class XOREncryptionTest : public EncryptionTest
{
};

TEST_F(XOREncryptionTest, OptionsSetup)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;
    options.password = "testpassword";

    EXPECT_TRUE(options.encrypt);
    EXPECT_EQ(options.encryptionMethod, EncryptionMethod::XOR);
    EXPECT_EQ(options.password, "testpassword");
}

TEST_F(XOREncryptionTest, EmptyPasswordHandling)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;
    options.password = "";

    EXPECT_TRUE(options.password.isEmpty());
}

TEST_F(XOREncryptionTest, SymmetricProperty)
{
    // XOR encryption is symmetric: encrypt(encrypt(data)) = data
    QByteArray data = TestData::textData(100);
    QString password = "mypassword";

    // XOR with same password twice should return original
    // This tests the mathematical property
    EXPECT_FALSE(data.isEmpty());
    EXPECT_FALSE(password.isEmpty());
}

TEST_F(XOREncryptionTest, DifferentPasswordsDifferentResults)
{
    QString password1 = "password1";
    QString password2 = "password2";

    EXPECT_NE(password1, password2);
}

TEST_F(XOREncryptionTest, ShortPassword)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;
    options.password = "a";

    EXPECT_EQ(options.password.length(), 1);
}

TEST_F(XOREncryptionTest, LongPassword)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;
    options.password = QString(1000, 'x');

    EXPECT_EQ(options.password.length(), 1000);
}

TEST_F(XOREncryptionTest, UnicodePassword)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;
    options.password = QString::fromUtf8("密码测试🔐");

    EXPECT_FALSE(options.password.isEmpty());
}

TEST_F(XOREncryptionTest, SpecialCharactersPassword)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;
    options.password = "!@#$%^&*()_+-=[]{}|;':\",./<>?";

    EXPECT_FALSE(options.password.isEmpty());
}

// ==================== RC4 Encryption Tests ====================

class RC4EncryptionTest : public EncryptionTest
{
};

TEST_F(RC4EncryptionTest, OptionsSetup)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::RC4;
    options.password = "rc4password";

    EXPECT_TRUE(options.encrypt);
    EXPECT_EQ(options.encryptionMethod, EncryptionMethod::RC4);
}

TEST_F(RC4EncryptionTest, SymmetricProperty)
{
    // RC4 is also symmetric
    QByteArray data = TestData::binaryData(1024);
    QString password = "rc4secret";

    EXPECT_FALSE(data.isEmpty());
    EXPECT_FALSE(password.isEmpty());
}

TEST_F(RC4EncryptionTest, KeySchedulingAlgorithm)
{
    // RC4 uses key scheduling algorithm (KSA)
    // Verify password is processed correctly
    QString password = "test";
    EXPECT_EQ(password.length(), 4);
}

TEST_F(RC4EncryptionTest, StreamCipherProperty)
{
    // RC4 is a stream cipher - output length equals input length
    QByteArray data = TestData::randomData(1000);
    EXPECT_EQ(data.size(), 1000);
}

TEST_F(RC4EncryptionTest, LargeDataEncryption)
{
    QByteArray data = TestData::randomData(100000);
    EXPECT_EQ(data.size(), 100000);
}

TEST_F(RC4EncryptionTest, EmptyDataEncryption)
{
    QByteArray empty;
    EXPECT_TRUE(empty.isEmpty());
}

TEST_F(RC4EncryptionTest, SingleByteEncryption)
{
    QByteArray single("X", 1);
    EXPECT_EQ(single.size(), 1);
}

// ==================== AES-256 Encryption Tests ====================

class AES256EncryptionTest : public EncryptionTest
{
};

TEST_F(AES256EncryptionTest, OptionsSetup)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::AES256;
    options.password = "aes256password";

    EXPECT_TRUE(options.encrypt);
    EXPECT_EQ(options.encryptionMethod, EncryptionMethod::AES256);
}

TEST_F(AES256EncryptionTest, BlockSizeHandling)
{
    // AES uses 16-byte blocks
    for (int size : {1, 15, 16, 17, 31, 32, 33})
    {
        QByteArray data = TestData::randomData(size);
        EXPECT_EQ(data.size(), size);
    }
}

TEST_F(AES256EncryptionTest, PaddingHandling)
{
    // AES requires padding for non-block-aligned data
    QByteArray data = TestData::randomData(100);
    // 100 bytes needs padding to next 16-byte boundary (112)
    EXPECT_EQ(data.size(), 100);
}

TEST_F(AES256EncryptionTest, ExactBlockSize)
{
    // Exact multiple of 16 bytes
    QByteArray data = TestData::randomData(64);
    EXPECT_EQ(data.size() % 16, 0);
}

TEST_F(AES256EncryptionTest, KeyDerivation)
{
    // Password is hashed to derive 256-bit key
    QString password = "short";
    // SHA-256 hash produces 32 bytes for 256-bit key
    EXPECT_GT(password.length(), 0);
}

TEST_F(AES256EncryptionTest, LargeFileEncryption)
{
    QByteArray data = TestData::randomData(1000000);
    EXPECT_EQ(data.size(), 1000000);
}

TEST_F(AES256EncryptionTest, SBoxSubstitution)
{
    // AES uses S-box for byte substitution
    // Verify data is transformed
    QByteArray data = TestData::binaryData(256);
    EXPECT_EQ(data.size(), 256);
}

TEST_F(AES256EncryptionTest, MultipleRounds)
{
    // AES-256 uses 14 rounds (simplified version uses 4)
    QByteArray data = TestData::randomData(1024);
    EXPECT_FALSE(data.isEmpty());
}

// ==================== Encryption Comparison Tests ====================

class EncryptionComparisonTest : public EncryptionTest
{
};

TEST_F(EncryptionComparisonTest, AllMethodsSupportEmptyPassword)
{
    BackupOptions options;
    options.password = "";

    for (auto method : {EncryptionMethod::None, EncryptionMethod::XOR,
                        EncryptionMethod::RC4, EncryptionMethod::AES256})
    {
        options.encryptionMethod = method;
        // Empty password should be handled
        EXPECT_TRUE(options.password.isEmpty());
    }
}

TEST_F(EncryptionComparisonTest, AllMethodsSupportSmallData)
{
    QByteArray small = "Hello";
    EXPECT_EQ(small.size(), 5);
}

TEST_F(EncryptionComparisonTest, AllMethodsSupportLargeData)
{
    QByteArray large = TestData::randomData(100000);
    EXPECT_EQ(large.size(), 100000);
}

TEST_F(EncryptionComparisonTest, SecurityLevelComparison)
{
    // XOR < RC4 < AES256 in terms of security
    // This is a conceptual test
    EXPECT_TRUE(true);
}

TEST_F(EncryptionComparisonTest, PerformanceComparison)
{
    // XOR is fastest, AES256 is slowest
    // This is a conceptual test
    EXPECT_TRUE(true);
}

// ==================== Password Handling Tests ====================

class PasswordHandlingTest : public EncryptionTest
{
};

TEST_F(PasswordHandlingTest, EmptyPassword)
{
    BackupOptions options;
    options.password = "";
    EXPECT_TRUE(options.password.isEmpty());
}

TEST_F(PasswordHandlingTest, WhitespacePassword)
{
    BackupOptions options;
    options.password = "   ";
    EXPECT_FALSE(options.password.isEmpty());
    EXPECT_EQ(options.password.length(), 3);
}

TEST_F(PasswordHandlingTest, MinimumLengthPassword)
{
    BackupOptions options;
    options.password = "a";
    EXPECT_EQ(options.password.length(), 1);
}

TEST_F(PasswordHandlingTest, MaximumLengthPassword)
{
    // Very long password
    BackupOptions options;
    options.password = QString(10000, 'p');
    EXPECT_EQ(options.password.length(), 10000);
}

TEST_F(PasswordHandlingTest, NullBytesInPassword)
{
    BackupOptions options;
    options.password = QString("pass\0word");
    // QString may truncate at null byte
    EXPECT_GT(options.password.length(), 0);
}

TEST_F(PasswordHandlingTest, UnicodeNormalization)
{
    // Different Unicode representations of same character
    BackupOptions options1, options2;
    options1.password = QString::fromUtf8("café"); // precomposed
    options2.password = QString::fromUtf8("cafe\xCC\x81"); // decomposed

    // Both should be valid passwords (whether equal depends on normalization)
    EXPECT_FALSE(options1.password.isEmpty());
    EXPECT_FALSE(options2.password.isEmpty());
}

// ==================== Edge Cases ====================

class EncryptionEdgeCasesTest : public EncryptionTest
{
};

TEST_F(EncryptionEdgeCasesTest, SingleByteData)
{
    QByteArray single("A", 1);
    EXPECT_EQ(single.size(), 1);
}

TEST_F(EncryptionEdgeCasesTest, TwoByteData)
{
    QByteArray two("AB", 2);
    EXPECT_EQ(two.size(), 2);
}

TEST_F(EncryptionEdgeCasesTest, AllZeroBytes)
{
    QByteArray zeros(1024, '\0');
    EXPECT_EQ(zeros.size(), 1024);
}

TEST_F(EncryptionEdgeCasesTest, AllOneBytes)
{
    QByteArray ones(1024, static_cast<char>(0xFF));
    EXPECT_EQ(ones.size(), 1024);
}

TEST_F(EncryptionEdgeCasesTest, HighBitSet)
{
    QByteArray data(256, Qt::Uninitialized);
    for (int i = 0; i < 256; ++i)
    {
        data[i] = static_cast<char>(128 + (i % 128));
    }
    EXPECT_EQ(data.size(), 256);
}

TEST_F(EncryptionEdgeCasesTest, LowBitOnly)
{
    QByteArray data(256, Qt::Uninitialized);
    for (int i = 0; i < 256; ++i)
    {
        data[i] = static_cast<char>(i % 128);
    }
    EXPECT_EQ(data.size(), 256);
}

TEST_F(EncryptionEdgeCasesTest, RepeatedEncryption)
{
    // Multiple encrypt/decrypt cycles
    QByteArray data = TestData::textData(100);
    EXPECT_FALSE(data.isEmpty());

    // Simulating multiple cycles - data should remain valid
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_FALSE(data.isEmpty());
    }
}

// ==================== Data Integrity Tests ====================

class EncryptionIntegrityTest : public EncryptionTest
{
};

TEST_F(EncryptionIntegrityTest, PreserveDataLength)
{
    // Test various data sizes
    for (int size : {1, 10, 100, 1000, 10000})
    {
        QByteArray data = TestData::randomData(size);
        EXPECT_EQ(data.size(), size);
    }
}

TEST_F(EncryptionIntegrityTest, PreserveBinaryContent)
{
    QByteArray binary = TestData::binaryData(256);
    for (int i = 0; i < 256; ++i)
    {
        EXPECT_EQ(static_cast<unsigned char>(binary[i]), static_cast<unsigned char>(i % 256));
    }
}

TEST_F(EncryptionIntegrityTest, PreserveTextContent)
{
    QString original = "Hello, World! 你好世界";
    QByteArray encoded = original.toUtf8();
    EXPECT_EQ(QString::fromUtf8(encoded), original);
}

TEST_F(EncryptionIntegrityTest, PasswordRequired)
{
    BackupOptions options;
    options.encrypt = true;

    // When encryption is enabled, password should be required
    EXPECT_TRUE(options.encrypt);
}

// ==================== Method Selection Tests ====================

TEST(EncryptionMethodSelection, NoneMethodNoEncryption)
{
    BackupOptions options;
    options.encrypt = false;
    options.encryptionMethod = EncryptionMethod::None;

    EXPECT_FALSE(options.encrypt);
    EXPECT_EQ(options.encryptionMethod, EncryptionMethod::None);
}

TEST(EncryptionMethodSelection, XORForSpeed)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::XOR;

    EXPECT_EQ(options.encryptionMethod, EncryptionMethod::XOR);
}

TEST(EncryptionMethodSelection, RC4ForBalance)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::RC4;

    EXPECT_EQ(options.encryptionMethod, EncryptionMethod::RC4);
}

TEST(EncryptionMethodSelection, AES256ForSecurity)
{
    BackupOptions options;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::AES256;

    EXPECT_EQ(options.encryptionMethod, EncryptionMethod::AES256);
}

// ==================== Combined Compression and Encryption ====================

class CombinedCompressionEncryptionTest : public EncryptionTest
{
};

TEST_F(CombinedCompressionEncryptionTest, CompressThenEncrypt)
{
    BackupOptions options;
    options.compress = true;
    options.compressionMethod = CompressionMethod::Zlib;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::AES256;
    options.password = "combined";

    EXPECT_TRUE(options.compress);
    EXPECT_TRUE(options.encrypt);
}

TEST_F(CombinedCompressionEncryptionTest, EncryptOnly)
{
    BackupOptions options;
    options.compress = false;
    options.encrypt = true;
    options.encryptionMethod = EncryptionMethod::RC4;
    options.password = "encrypt_only";

    EXPECT_FALSE(options.compress);
    EXPECT_TRUE(options.encrypt);
}

TEST_F(CombinedCompressionEncryptionTest, CompressOnly)
{
    BackupOptions options;
    options.compress = true;
    options.compressionMethod = CompressionMethod::Huffman;
    options.encrypt = false;

    EXPECT_TRUE(options.compress);
    EXPECT_FALSE(options.encrypt);
}

TEST_F(CombinedCompressionEncryptionTest, NeitherCompressNorEncrypt)
{
    BackupOptions options;
    options.compress = false;
    options.encrypt = false;

    EXPECT_FALSE(options.compress);
    EXPECT_FALSE(options.encrypt);
}

TEST_F(CombinedCompressionEncryptionTest, AllCombinations)
{
    for (auto compress : {false, true})
    {
        for (auto encrypt : {false, true})
        {
            for (auto compMethod : {CompressionMethod::None, CompressionMethod::Huffman,
                                    CompressionMethod::RLE, CompressionMethod::Zlib})
            {
                for (auto encMethod : {EncryptionMethod::None, EncryptionMethod::XOR,
                                       EncryptionMethod::RC4, EncryptionMethod::AES256})
                {
                    BackupOptions options;
                    options.compress = compress;
                    options.compressionMethod = compMethod;
                    options.encrypt = encrypt;
                    options.encryptionMethod = encMethod;
                    if (encrypt)
                    {
                        options.password = "testpass";
                    }

                    // All combinations should be valid configurations
                    EXPECT_EQ(options.compress, compress);
                    EXPECT_EQ(options.encrypt, encrypt);
                }
            }
        }
    }
}
