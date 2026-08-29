#include "SecureUtils.hpp"
#include <iomanip>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <sys/stat.h>

#ifdef HAVE_OPENSSL
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#endif

namespace folio {
namespace security {

std::string encryptAes256Gcm(const std::string& plaintext,
                             const std::string& key) {
#ifndef HAVE_OPENSSL
    return "";
#else
    if (key.size() != 32) {
        return "";
    }

    unsigned char iv[12];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        return "";
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";

    std::vector<unsigned char> ciphertext(plaintext.size() + 16);
    int len = 0;
    int ciphertext_len = 0;

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                            reinterpret_cast<const unsigned char*>(key.data()),
                            iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    if (!EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          static_cast<int>(plaintext.size()))) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    ciphertext_len = len;

    if (!EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    ciphertext_len += len;

    unsigned char tag[16];
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    EVP_CIPHER_CTX_free(ctx);

    std::stringstream ss;
    for (unsigned char c : iv) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    for (int i = 0; i < ciphertext_len; ++i) {
        const unsigned char c = ciphertext[i];
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    for (unsigned char c : tag) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }

    return ss.str();
#endif
}

std::string decryptAes256Gcm(const std::string& ciphertextHex,
                             const std::string& key) {
#ifndef HAVE_OPENSSL
    return "";
#else
    if (key.size() != 32 || ciphertextHex.length() < 48) {
        return "";
    }

    std::stringstream hexStream(ciphertextHex.substr(0, 24));
    unsigned char iv[12];
    for (int i = 0; i < 12; ++i) {
        std::string byteStr;
        hexStream >> std::setw(2) >> byteStr;
        if (byteStr.size() != 2) return "";
        iv[i] = static_cast<unsigned char>(std::stoi(byteStr, nullptr, 16));
    }

    std::string ciphertextTagHex = ciphertextHex.substr(24);
    std::vector<unsigned char> ciphertextTag;
    for (size_t i = 0; i + 1 < ciphertextTagHex.length(); i += 2) {
        ciphertextTag.push_back(
            static_cast<unsigned char>(std::stoi(ciphertextTagHex.substr(i, 2), nullptr, 16))
        );
    }

    if (ciphertextTag.size() < 16) {
        return "";
    }

    std::vector<unsigned char> ciphertext(ciphertextTag.begin(), ciphertextTag.end() - 16);
    unsigned char tag[16];
    std::copy(ciphertextTag.end() - 16, ciphertextTag.end(), tag);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";

    std::vector<unsigned char> plaintext(ciphertext.size());
    int len = 0;
    int plaintext_len = 0;

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                            reinterpret_cast<const unsigned char*>(key.data()),
                            iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    if (!EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                          ciphertext.data(), static_cast<int>(ciphertext.size()))) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    plaintext_len = len;

    if (!EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    return std::string(plaintext.begin(), plaintext.begin() + plaintext_len);
#endif
}

std::string generateRandomKey() {
#ifndef HAVE_OPENSSL
    return "";
#else
    unsigned char key[32];
    if (RAND_bytes(key, sizeof(key)) != 1) {
        return "";
    }

    std::stringstream ss;
    for (unsigned char c : key) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
#endif
}

bool validateSymbol(const std::string& symbol) {
    // NSE_EQ|ISIN format: e.g., "NSE_EQ|INE009A01021"
    static const std::regex symbolPattern("^[A-Z]+_[A-Z]+\\|[A-Z0-9]{12}$");
    return std::regex_match(symbol, symbolPattern);
}

bool validateQuantity(long quantity) {
    return quantity >= 1 && quantity <= 1000000;
}

bool validatePrice(double price) {
    return price >= 0.01 && price <= 100000.0;
}

bool validateSide(const std::string& side) {
    return side == "BUY" || side == "SELL";
}

std::string computeFileSha256(const std::string& filePath) {
#ifndef HAVE_OPENSSL
    (void)filePath;
    return "";
#else
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }

    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (unsigned char c : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }

    return ss.str();
#endif
}

SecureString getEnvSecure(const std::string& varName) {
    const char* value = std::getenv(varName.c_str());
    if (!value) {
        return SecureString("");
    }

    SecureString result(value);

    // Attempt to clear environment variable (platform-specific)
#ifdef __linux__
    unsetenv(varName.c_str());
#elif _WIN32
    _putenv_s(varName.c_str(), "");
#endif

    return result;
}

bool checkFilePermissions(const std::string& filePath,
                         unsigned int expectedMode) {
#ifdef __linux__
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0) {
        return false;  // File doesn't exist or can't be accessed
    }

    // Check if permissions match expected mode (mask with 0777)
    unsigned int actualMode = fileStat.st_mode & 0777;
    return actualMode == expectedMode;
#else
    // On Windows or other platforms, consider this check passed
    // (Windows doesn't use Unix-style file permissions)
    return true;
#endif
}

std::string generateRandomString(size_t length) {
#ifndef HAVE_OPENSSL
    return "";
#else
    if (length == 0) {
        return "";
    }

    std::vector<unsigned char> buffer(length);
    if (RAND_bytes(buffer.data(), static_cast<int>(length)) != 1) {
        return "";
    }

    std::stringstream ss;
    for (unsigned char c : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return ss.str().substr(0, length * 2);
#endif
}

}  // namespace security
}  // namespace folio
