#ifndef HASH_H
#define HASH_H

#include <string>
#include <vector>
#include <cstdint>

// SHA-256 hash function
std::string sha256(const std::string& input);

// Simple hash for passwords
std::string hash_password(const std::string& password);

// Base64 encoding/decoding
std::string encode_base64(const std::vector<uint8_t>& data);
std::vector<uint8_t> decode_base64(const std::string& encoded_string);

#endif // HASH_H