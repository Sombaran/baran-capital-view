#ifndef FOLIO_SECURE_UTILS_HPP
#define FOLIO_SECURE_UTILS_HPP

#include <string>
#include <vector>
#include <cstring>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace folio {
namespace security {

/**
 * @brief Secure string class that overwrites memory on destruction.
 * Prevents sensitive data (tokens, passwords) from lingering in RAM.
 */
class SecureString {
private:
    std::vector<char> data_;
    size_t length_;

    static void cleanse(std::vector<char>& data) noexcept {
        volatile char* bytes = data.data();
        for (size_t i = 0; i < data.size(); ++i) bytes[i] = 0;
    }

public:
    SecureString() : length_(0) {}
    
    explicit SecureString(const std::string& plaintext) 
        : data_(plaintext.begin(), plaintext.end()), 
          length_(plaintext.length()) {
    }
    
    ~SecureString() {
        if (!data_.empty()) {
            cleanse(data_);
        }
    }
    
    const char* c_str() const {
        return data_.data();
    }
    
    size_t length() const {
        return length_;
    }
    
    std::string to_string() const {
        return std::string(data_.begin(), data_.end());
    }
    
    // Prevent copying
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;
    
    // Allow moving
    SecureString(SecureString&& other) noexcept 
        : data_(std::move(other.data_)), length_(other.length_) {
        other.length_ = 0;
    }
    
    SecureString& operator=(SecureString&& other) noexcept {
        if (this != &other) {
            cleanse(data_);
            data_ = std::move(other.data_);
            length_ = other.length_;
            other.length_ = 0;
        }
        return *this;
    }
};

/**
 * @brief Encrypt a string using AES-256-GCM.
 * @param plaintext Data to encrypt
 * @param key 32-byte encryption key
 * @return Hex-encoded ciphertext with IV and auth tag
 */
std::string encryptAes256Gcm(const std::string& plaintext, 
                             const std::string& key);

/**
 * @brief Decrypt AES-256-GCM encrypted data.
 * @param ciphertextHex Hex-encoded ciphertext from encryptAes256Gcm()
 * @param key 32-byte encryption key
 * @return Decrypted plaintext or empty string on failure
 */
std::string decryptAes256Gcm(const std::string& ciphertextHex,
                             const std::string& key);

/**
 * @brief Generate a random key for AES-256.
 * @return 32 random bytes as hex string
 */
std::string generateRandomKey();

/**
 * @brief Validate symbol format (NSE_EQ|ISIN or similar).
 * @param symbol Symbol to validate
 * @return true if valid, false otherwise
 */
bool validateSymbol(const std::string& symbol);

/**
 * @brief Validate quantity bounds.
 * @param quantity Units to validate
 * @return true if valid (1-1,000,000), false otherwise
 */
bool validateQuantity(long quantity);

/**
 * @brief Validate price bounds.
 * @param price Price to validate
 * @return true if valid (0.01-100,000), false otherwise
 */
bool validatePrice(double price);

/**
 * @brief Validate order side.
 * @param side "BUY" or "SELL"
 * @return true if valid, false otherwise
 */
bool validateSide(const std::string& side);

/**
 * @brief Compute SHA256 hash of a file.
 * @param filePath Path to file
 * @return Hex-encoded SHA256 hash or empty string on error
 */
std::string computeFileSha256(const std::string& filePath);

/**
 * @brief Get environment variable securely.
 * Clears the variable after reading (one-time use).
 * @param varName Variable name
 * @return SecureString containing the value
 */
SecureString getEnvSecure(const std::string& varName);

/**
 * @brief Check file permissions (Unix-like systems).
 * @param filePath File to check
 * @param expectedMode Expected mode (e.g., 0600)
 * @return true if permissions match, false otherwise
 */
bool checkFilePermissions(const std::string& filePath, 
                         unsigned int expectedMode);

/**
 * @brief Generate a cryptographically secure random string.
 * @param length Desired length
 * @return Random hex string
 */
std::string generateRandomString(size_t length);

}  // namespace security
}  // namespace folio

#endif  // FOLIO_SECURE_UTILS_HPP
