// Fallback factory for platforms without a compiled SecretStore backend
// (e.g. Linux built without libsecret): no secure storage, Setting keeps
// the API key in config.json (chmod 600), exactly as before.

#include "settings/SecretStore.h"

std::unique_ptr<SecretStore> createPlatformSecretStore()
{
    return nullptr;
}
