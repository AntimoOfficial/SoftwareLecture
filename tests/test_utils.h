#pragma once

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <memory>

class TempDirectory
{
public:
    TempDirectory();
    ~TempDirectory() = default;

    bool isValid() const;
    QString path() const;
    QString filePath(const QString &relativePath) const;

    bool createFile(const QString &relativePath, const QByteArray &content);
    bool createDirectory(const QString &relativePath);
    bool createSymlink(const QString &relativePath, const QString &target);

    QByteArray readFile(const QString &relativePath) const;
    bool fileExists(const QString &relativePath) const;
    bool directoryExists(const QString &relativePath) const;

private:
    std::unique_ptr<QTemporaryDir> m_tempDir;
};

class BackupTestFixture : public ::testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    QString sourceDir() const;
    QString destDir() const;
    QString restoreDir() const;

    void createTestFiles();
    void createTestFilesWithContent(const QByteArray &content, int count = 5);

    std::unique_ptr<TempDirectory> m_sourceDir;
    std::unique_ptr<TempDirectory> m_destDir;
    std::unique_ptr<TempDirectory> m_restoreDir;
};

namespace TestData
{
    QByteArray randomData(int size);
    QByteArray repeatPattern(const QByteArray &pattern, int count);
    QByteArray textData(int lines);
    QByteArray binaryData(int size);
    QByteArray emptyData();
    QByteArray largeData(int sizeMB);
}
