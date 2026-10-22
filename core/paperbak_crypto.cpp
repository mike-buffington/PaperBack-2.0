#include "paperbak_crypto.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include <Security/SecRandom.h>
#include <stdexcept>
#include <cstring>
namespace paperbak {
void randomSaltAndIV(std::uint8_t output[32]) {
    if (SecRandomCopyBytes(kSecRandomDefault, 32, output) != errSecSuccess)
        throw std::runtime_error("Unable to generate cryptographic salt and IV");
}
std::vector<std::uint8_t> cryptBackup(const std::vector<std::uint8_t>& input,
    const std::string& password, const std::uint8_t saltAndIV[32], bool encrypt) {
    if (password.empty()) throw std::runtime_error("Enter a password");
    if (input.size()%16) throw std::runtime_error("Encrypted data is not AES block-aligned");
    std::uint8_t key[24] = {};
    int result = CCKeyDerivationPBKDF(kCCPBKDF2, password.data(), password.size(),
        saltAndIV, 16, kCCPRFHmacAlgSHA256, 524288, key, sizeof(key));
    if (result != kCCSuccess) throw std::runtime_error("Password key derivation failed");
    std::vector<std::uint8_t> output(input.size());
    size_t used = 0;
    CCCryptorStatus status = CCCrypt(encrypt?kCCEncrypt:kCCDecrypt, kCCAlgorithmAES,
        0, key, sizeof(key), saltAndIV+16, input.data(), input.size(),
        output.data(), output.size(), &used);
    std::memset(key,0,sizeof(key));
    if (status != kCCSuccess || used != input.size())
        throw std::runtime_error("AES-CBC operation failed");
    return output;
}
}
