// Windows backend: DPAPI (CryptProtectData). The encrypted blobs are stored
// base64-encoded in secrets.dat beside config.json — the file alone is
// useless without the user's Windows credentials.
// Compiled only on Windows (see CMakeLists.txt).

#include "settings/SecretStore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <windows.h>
#include <dpapi.h>

namespace {

QString blobFilePath()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (baseDir.isEmpty()) {
        return QString();
    }
    QDir dir(baseDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(QStringLiteral("secrets.dat"));
}

QJsonObject readBlobs(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeBlobs(const QString& path, const QJsonObject& blobs)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(blobs).toJson(QJsonDocument::Compact));
    return file.commit();
}

class DpapiSecretStore : public SecretStore
{
public:
    // DPAPI is part of Windows and always usable.
    bool isAvailable() override { return !blobFilePath().isEmpty(); }

    QString read(const QString& key) override
    {
        const QString encoded = readBlobs(blobFilePath()).value(key).toString();
        if (encoded.isEmpty()) {
            return QString();
        }
        DATA_BLOB input{ 0, nullptr };
        const QByteArray cipher = QByteArray::fromBase64(encoded.toLatin1());
        input.cbData = static_cast<DWORD>(cipher.size());
        input.pbData = reinterpret_cast<BYTE*>(cipher.data());
        DATA_BLOB output{ 0, nullptr };
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
            qWarning() << "SecretStore: CryptUnprotectData failed, error" << GetLastError();
            return QString();
        }
        const QString result = QString::fromUtf8(reinterpret_cast<char*>(output.pbData),
                                                 static_cast<int>(output.cbData));
        LocalFree(output.pbData);
        return result;
    }

    bool write(const QString& key, const QString& value) override
    {
        const QByteArray plain = value.toUtf8();
        DATA_BLOB input{ static_cast<DWORD>(plain.size()),
                         reinterpret_cast<BYTE*>(plain.data()) };
        DATA_BLOB output{ 0, nullptr };
        if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
            qWarning() << "SecretStore: CryptProtectData failed, error" << GetLastError();
            return false;
        }
        const QByteArray encoded = QByteArray(reinterpret_cast<char*>(output.pbData),
                                              static_cast<int>(output.cbData)).toBase64();
        LocalFree(output.pbData);

        QJsonObject blobs = readBlobs(blobFilePath());
        blobs.insert(key, QString::fromLatin1(encoded));
        return writeBlobs(blobFilePath(), blobs);
    }

    bool remove(const QString& key) override
    {
        QJsonObject blobs = readBlobs(blobFilePath());
        if (!blobs.contains(key)) {
            return true;
        }
        blobs.remove(key);
        return writeBlobs(blobFilePath(), blobs);
    }
};

} // namespace

std::unique_ptr<SecretStore> createPlatformSecretStore()
{
    return std::make_unique<DpapiSecretStore>();
}
