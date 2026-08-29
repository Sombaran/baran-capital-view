#include "SecureUtils.hpp"
#include <iomanip>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <array>
#include <random>
#include <sys/stat.h>

#ifdef HAVE_OPENSSL
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#endif

namespace folio {
namespace security {

namespace {

#ifndef HAVE_OPENSSL
struct Sha256Context {
    std::array<uint32_t, 8> state{};
    std::array<uint32_t, 64> buffer{};
    std::size_t bufferLen = 0;
    std::uint64_t bitLen = 0;
};

static constexpr std::array<uint32_t, 64> kSha256Constants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

static uint32_t rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

static uint32_t choose(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static uint32_t majority(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t sigma0(uint32_t x) {
    return rotr(x, 2U) ^ rotr(x, 13U) ^ rotr(x, 22U);
}

static uint32_t sigma1(uint32_t x) {
    return rotr(x, 6U) ^ rotr(x, 11U) ^ rotr(x, 25U);
}

static uint32_t gamma0(uint32_t x) {
    return rotr(x, 7U) ^ rotr(x, 18U) ^ (x >> 3U);
}

static uint32_t gamma1(uint32_t x) {
    return rotr(x, 17U) ^ rotr(x, 19U) ^ (x >> 10U);
}

static void sha256Transform(Sha256Context& context, const std::array<uint32_t, 64>& block) {
    std::array<uint32_t, 8> working = context.state;
    std::array<uint32_t, 64> message = block;

    for (std::size_t i = 16; i < message.size(); ++i) {
        message[i] = gamma1(message[i - 2]) + message[i - 7] + gamma0(message[i - 15]) + message[i - 16];
    }

    for (std::size_t i = 0; i < message.size(); ++i) {
        const uint32_t temp1 = working[7] + sigma1(working[4]) + choose(working[4], working[5], working[6]) + kSha256Constants[i] + message[i];
        const uint32_t temp2 = sigma0(working[0]) + majority(working[0], working[1], working[2]);
        working[7] = working[6];
        working[6] = working[5];
        working[5] = working[4];
        working[4] = working[3] + temp1;
        working[3] = working[2];
        working[2] = working[1];
        working[1] = working[0];
        working[0] = temp1 + temp2;
    }

    for (std::size_t i = 0; i < context.state.size(); ++i) {
        context.state[i] += working[i];
    }
}

static void sha256Init(Sha256Context& context) {
    context.state = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
    context.bufferLen = 0;
    context.bitLen = 0;
}

static void sha256Update(Sha256Context& context, const unsigned char* data, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        context.buffer[context.bufferLen / 4U] = (context.buffer[context.bufferLen / 4U] << 8) | data[i];
        context.bufferLen++;
        if (context.bufferLen == 64U) {
            std::array<uint32_t, 64> block{};
            for (std::size_t j = 0; j < 16; ++j) {
                block[j] = (static_cast<uint32_t>(context.buffer[j * 4]) << 24) |
                           (static_cast<uint32_t>(context.buffer[j * 4 + 1]) << 16) |
                           (static_cast<uint32_t>(context.buffer[j * 4 + 2]) << 8) |
                           static_cast<uint32_t>(context.buffer[j * 4 + 3]);
            }
            sha256Transform(context, block);
            context.bufferLen = 0;
            context.bitLen += 512U;
        }
    }
}

static std::string sha256Hex(const std::string& input) {
    Sha256Context context;
    sha256Init(context);
    sha256Update(context, reinterpret_cast<const unsigned char*>(input.data()), input.size());

    const std::uint64_t totalBits = (context.bitLen + static_cast<std::uint64_t>(context.bufferLen) * 8U) + 64U;
    const std::size_t padLen = (context.bufferLen < 56U) ? (56U - context.bufferLen) : (120U - context.bufferLen);
    std::array<unsigned char, 64> padding{};
    padding[0] = 0x80;
    for (std::size_t i = 0; i < padLen; ++i) {
        sha256Update(context, padding.data(), 1U);
    }

    std::array<unsigned char, 8> lengthBytes{};
    for (std::size_t i = 0; i < 8; ++i) {
        lengthBytes[7 - i] = static_cast<unsigned char>((totalBits >> (i * 8U)) & 0xFFU);
    }
    sha256Update(context, lengthBytes.data(), 8U);

    std::array<uint32_t, 8> state = context.state;
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (uint32_t value : state) {
        output << std::setw(8) << value;
    }
    return output.str();
}

static std::string fallbackRandomBytes(std::size_t length) {
    std::random_device random;
    std::uniform_int_distribution<int> dist(0, 255);
    std::string bytes(length, '\0');
    for (std::size_t i = 0; i < length; ++i) {
        bytes[i] = static_cast<char>(dist(random));
    }
    return bytes;
}

#endif

} // namespace

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
    const std::string random = fallbackRandomBytes(32);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned char value : random) {
        output << std::setw(2) << static_cast<int>(value);
    }
    return output.str();
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
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return sha256Hex(buffer.str());
#else
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    char streamBuffer[4096];
    while (file.read(streamBuffer, sizeof(streamBuffer))) {
        SHA256_Update(&sha256, streamBuffer, file.gcount());
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
    if (length == 0) {
        return "";
    }

    std::random_device random;
    std::uniform_int_distribution<int> dist(0, 255);
    std::string bytes(length, '\0');
    for (std::size_t i = 0; i < length; ++i) {
        bytes[i] = static_cast<char>(dist(random));
    }

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : bytes) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str().substr(0, length * 2);
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
