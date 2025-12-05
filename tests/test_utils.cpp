#include "test_utils.h"

#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>

TempDirectory::TempDirectory()
    : m_tempDir(std::make_unique<QTemporaryDir>())
{
}

bool TempDirectory::isValid() const
{
    return m_tempDir && m_tempDir->isValid();
}

QString TempDirectory::path() const
{
    return m_tempDir ? m_tempDir->path() : QString();
}

QString TempDirectory::filePath(const QString &relativePath) const
{
    return QDir(path()).filePath(relativePath);
}

bool TempDirectory::createFile(const QString &relativePath, const QByteArray &content)
{
    const QString fullPath = filePath(relativePath);
    QFileInfo info(fullPath);
    QDir().mkpath(info.absolutePath());

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        return false;
    }
    return file.write(content) == content.size();
}

bool TempDirectory::createDirectory(const QString &relativePath)
{
    return QDir(path()).mkpath(relativePath);
}

bool TempDirectory::createSymlink(const QString &relativePath, const QString &target)
{
    const QString fullPath = filePath(relativePath);
    QFileInfo info(fullPath);
    QDir().mkpath(info.absolutePath());
    return QFile::link(target, fullPath);
}

QByteArray TempDirectory::readFile(const QString &relativePath) const
{
    QFile file(filePath(relativePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return file.readAll();
}

bool TempDirectory::fileExists(const QString &relativePath) const
{
    return QFileInfo::exists(filePath(relativePath));
}

bool TempDirectory::directoryExists(const QString &relativePath) const
{
    QFileInfo info(filePath(relativePath));
    return info.exists() && info.isDir();
}

void BackupTestFixture::SetUp()
{
    m_sourceDir = std::make_unique<TempDirectory>();
    m_destDir = std::make_unique<TempDirectory>();
    m_restoreDir = std::make_unique<TempDirectory>();

    ASSERT_TRUE(m_sourceDir->isValid());
    ASSERT_TRUE(m_destDir->isValid());
    ASSERT_TRUE(m_restoreDir->isValid());
}

void BackupTestFixture::TearDown()
{
    m_sourceDir.reset();
    m_destDir.reset();
    m_restoreDir.reset();
}

QString BackupTestFixture::sourceDir() const
{
    return m_sourceDir ? m_sourceDir->path() : QString();
}

QString BackupTestFixture::destDir() const
{
    return m_destDir ? m_destDir->path() : QString();
}

QString BackupTestFixture::restoreDir() const
{
    return m_restoreDir ? m_restoreDir->path() : QString();
}

void BackupTestFixture::createTestFiles()
{
    ASSERT_TRUE(m_sourceDir->createFile("file1.txt", "Hello World\n"));
    ASSERT_TRUE(m_sourceDir->createFile("file2.txt", "Another file content\n"));
    ASSERT_TRUE(m_sourceDir->createFile("subdir/file3.txt", "Nested file\n"));
    ASSERT_TRUE(m_sourceDir->createFile("subdir/deep/file4.txt", "Deep nested file\n"));
    ASSERT_TRUE(m_sourceDir->createFile("binary.bin", TestData::binaryData(1024)));
}

void BackupTestFixture::createTestFilesWithContent(const QByteArray &content, int count)
{
    for (int i = 0; i < count; ++i)
    {
        QString filename = QString("testfile_%1.dat").arg(i);
        ASSERT_TRUE(m_sourceDir->createFile(filename, content));
    }
}

namespace TestData
{

QByteArray randomData(int size)
{
    QByteArray data(size, Qt::Uninitialized);
    auto *generator = QRandomGenerator::global();
    for (int i = 0; i < size; ++i)
    {
        data[i] = static_cast<char>(generator->bounded(256));
    }
    return data;
}

QByteArray repeatPattern(const QByteArray &pattern, int count)
{
    QByteArray result;
    result.reserve(pattern.size() * count);
    for (int i = 0; i < count; ++i)
    {
        result.append(pattern);
    }
    return result;
}

QByteArray textData(int lines)
{
    QByteArray result;
    for (int i = 0; i < lines; ++i)
    {
        result.append(QString("This is line %1 of the test file.\n").arg(i + 1).toUtf8());
    }
    return result;
}

QByteArray binaryData(int size)
{
    QByteArray data(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i)
    {
        data[i] = static_cast<char>(i % 256);
    }
    return data;
}

QByteArray emptyData()
{
    return QByteArray();
}

QByteArray largeData(int sizeMB)
{
    const int size = sizeMB * 1024 * 1024;
    return randomData(size);
}

} // namespace TestData
