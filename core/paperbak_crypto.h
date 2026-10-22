#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace paperbak {
// Windows PaperBak 1.10: PBKDF2-HMAC-SHA256, 524288 rounds, AES-192-CBC.
std::vector<std::uint8_t> cryptBackup(const std::vector<std::uint8_t>& input,
    const std::string& password, const std::uint8_t saltAndIV[32], bool encrypt);
void randomSaltAndIV(std::uint8_t output[32]);
}
