// Linux backend: libsecret (Secret Service API → GNOME Keyring / KWallet).
// Compiled only when pkg-config finds libsecret-1 (TRANSAI_HAVE_LIBSECRET);
// otherwise SecretStoreNull.cpp provides the factory instead.

#include "settings/SecretStore.h"

#include <QDebug>

#include <libsecret/secret.h>

namespace {

const SecretSchema* transaiSchema()
{
    static const SecretSchema schema = {
        "me.tinyx.TransAI", SECRET_SCHEMA_NONE,
        {
            { "app", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "key", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING /* unused terminator type */ },
        },
    };
    return &schema;
}

class LibsecretStore : public SecretStore
{
public:
    bool isAvailable() override
    {
        if (m_available.has_value()) {
            return *m_available;
        }
        // Probe with a harmless lookup: when no Secret Service is reachable
        // (headless session, no D-Bus), this errors out immediately.
        GError* error = nullptr;
        secret_password_lookup_sync(transaiSchema(), nullptr, &error,
                                    "app", "TransAI", "key", "__probe__", nullptr);
        m_available = (error == nullptr);
        if (error) {
            qInfo() << "SecretStore: no Secret Service available, API key stays in config.json:"
                    << error->message;
            g_error_free(error);
        }
        return *m_available;
    }

    QString read(const QString& key) override
    {
        GError* error = nullptr;
        gchar* value = secret_password_lookup_sync(transaiSchema(), nullptr, &error,
                                                   "app", "TransAI",
                                                   "key", key.toUtf8().constData(), nullptr);
        if (error) {
            qWarning() << "SecretStore read failed:" << error->message;
            g_error_free(error);
            return QString();
        }
        if (!value) {
            return QString();
        }
        const QString result = QString::fromUtf8(value);
        secret_password_free(value);
        return result;
    }

    bool write(const QString& key, const QString& value) override
    {
        GError* error = nullptr;
        const QByteArray label = QStringLiteral("TransAI API key (%1)").arg(key).toUtf8();
        const gboolean ok = secret_password_store_sync(transaiSchema(), SECRET_COLLECTION_DEFAULT,
                                                       label.constData(), value.toUtf8().constData(),
                                                       nullptr, &error,
                                                       "app", "TransAI",
                                                       "key", key.toUtf8().constData(), nullptr);
        if (error) {
            qWarning() << "SecretStore write failed:" << error->message;
            g_error_free(error);
        }
        return ok == TRUE && !error;
    }

    bool remove(const QString& key) override
    {
        GError* error = nullptr;
        secret_password_clear_sync(transaiSchema(), nullptr, &error,
                                   "app", "TransAI", "key", key.toUtf8().constData(), nullptr);
        if (error) {
            qWarning() << "SecretStore remove failed:" << error->message;
            g_error_free(error);
            return false;
        }
        return true;
    }

private:
    std::optional<bool> m_available;
};

} // namespace

std::unique_ptr<SecretStore> createPlatformSecretStore()
{
    return std::make_unique<LibsecretStore>();
}
