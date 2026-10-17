#ifndef SECRET_STORE_H
#define SECRET_STORE_H

#include <memory>

#include <QString>

// Platform secret storage (OS keychain / credential vault) for the API key,
// replacing plaintext storage in config.json.
//
// One backend is compiled per platform (exactly one source provides
// createPlatformSecretStore):
//   Linux   — libsecret (GNOME Keyring / KWallet via the Secret Service API),
//             only when pkg-config finds libsecret-1 (TRANSAI_HAVE_LIBSECRET);
//   macOS   — Keychain Services (Security framework);
//   Windows — DPAPI (CryptProtectData), ciphertext blobs in secrets.dat
//             beside config.json;
//   otherwise — nullptr, and Setting falls back to the legacy plaintext key.
//
// All methods are synchronous (Setting reads/writes config on the main
// thread during load/save; the secrets involved are tiny).
class SecretStore
{
public:
    virtual ~SecretStore() = default;

    // Whether the backend can actually be used right now (e.g. no Secret
    // Service on a headless Linux session → false). Checked per call site;
    // a false result always means "fall back to config.json".
    virtual bool isAvailable() = 0;

    // Empty string when the key is absent or the read failed.
    virtual QString read(const QString& key) = 0;

    // Success must be verifiable: implementations return false on any error,
    // and callers additionally read back after writing before dropping the
    // plaintext copy.
    virtual bool write(const QString& key, const QString& value) = 0;

    // True when the key is gone (or never existed).
    virtual bool remove(const QString& key) = 0;
};

// Returns nullptr when no backend is compiled for this platform.
std::unique_ptr<SecretStore> createPlatformSecretStore();

#endif // SECRET_STORE_H
