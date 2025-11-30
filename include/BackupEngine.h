#pragma once

#include "BackupTypes.h"

#include <optional>

class BackupEngine
{
public:
    BackupEngine() = default;

    BackupResult performBackup(const BackupOptions &options,
                               const BackupEngineCallbacks &callbacks = {}) const;

    RestoreResult restore(const RestoreOptions &options,
                          const BackupEngineCallbacks &callbacks = {}) const;

    VerifyResult verify(const VerifyOptions &options,
                        const BackupEngineCallbacks &callbacks = {}) const;

private:
    using ManifestPtr = std::optional<BackupManifest>;

    void log(const BackupEngineCallbacks &callbacks, const QString &message) const;
    void reportProgress(const BackupEngineCallbacks &callbacks, int current, int total) const;

    ManifestPtr loadManifest(const QString &path, QString &error) const;
    ManifestPtr loadManifestFromDirectory(const QString &path, QString &error) const;
    ManifestPtr loadManifestFromPackage(const QString &path, QString &error) const;

    bool writeManifestToDirectory(const QString &path, const BackupManifest &manifest, QString &error) const;
    bool finalizePackage(QFile &file, const BackupManifest &manifest, QString &error) const;

    QByteArray readStoredData(const BackupManifest &manifest, const FileRecord &record, const QString &password,
                              QString &error) const;

    QString dataFilePath(const BackupManifest &manifest, const FileRecord &record) const;

    QByteArray encrypt(const QByteArray &data, const QString &password) const;
    QByteArray decrypt(const QByteArray &data, const QString &password, bool &ok) const;

    QByteArray compressData(const QByteArray &data) const;
    QByteArray decompressData(const QByteArray &data, quint64 expectedSize, bool &ok) const;

    QFile::Permissions toQtPermissions(quint32 permissions) const;
    quint32 toStoredPermissions(QFile::Permissions permissions) const;

    bool ensureDirectory(const QString &path, QString &error) const;
    bool removePath(const QString &path) const;

    StorageMode detectStorageMode(const QString &path) const;

    void enforceRetention(const BackupOptions &options, const QString &latestPath,
                          const BackupEngineCallbacks &callbacks) const;
};
