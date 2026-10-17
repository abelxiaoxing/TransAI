// macOS backend: Keychain Services (Security framework, C API).
// Compiled only on Darwin (see CMakeLists.txt).

#include "settings/SecretStore.h"

#include <QDebug>

#import <Foundation/Foundation.h>
#include <Security/Security.h>

namespace {

NSDictionary* baseQuery(const QString& key)
{
    return @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: @"TransAI",
        (__bridge id)kSecAttrAccount: key.toNSString(),
    };
}

class MacKeychainStore : public SecretStore
{
public:
    // The Keychain is always present on macOS.
    bool isAvailable() override { return true; }

    QString read(const QString& key) override
    {
        NSMutableDictionary* query = [baseQuery(key) mutableCopy];
        query[(__bridge id)kSecReturnData] = @YES;
        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
        if (status == errSecItemNotFound) {
            return QString();
        }
        if (status != errSecSuccess) {
            qWarning() << "SecretStore read failed, OSStatus:" << status;
            return QString();
        }
        NSData* data = (__bridge_transfer NSData*)result;
        return QString::fromUtf8(static_cast<const char*>(data.bytes),
                                 static_cast<int>(data.length));
    }

    bool write(const QString& key, const QString& value) override
    {
        // Replace-or-insert: delete first (updating requires item attributes
        // matching; delete+add is simpler and atomic enough here).
        SecItemDelete((__bridge CFDictionaryRef)baseQuery(key));
        NSMutableDictionary* item = [baseQuery(key) mutableCopy];
        item[(__bridge id)kSecValueData] = [value.toNSString() dataUsingEncoding:NSUTF8StringEncoding];
        const OSStatus status = SecItemAdd((__bridge CFDictionaryRef)item, nullptr);
        if (status != errSecSuccess) {
            qWarning() << "SecretStore write failed, OSStatus:" << status;
        }
        return status == errSecSuccess;
    }

    bool remove(const QString& key) override
    {
        const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)baseQuery(key));
        return status == errSecSuccess || status == errSecItemNotFound;
    }
};

} // namespace

std::unique_ptr<SecretStore> createPlatformSecretStore()
{
    return std::make_unique<MacKeychainStore>();
}
