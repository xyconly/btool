/*************************************************
File name:  hmac.hpp
Author:
Date:
Description:    基于openssl提供的hmac算法支持进行封装

NOTE: 
*************************************************/
#pragma once
#include <iomanip>
#include <string>
#include <vector>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "encrypt_base.hpp"
#include "../scope_guard.hpp"

namespace BTool {
    class HMAC : public EncryptBase {
    public:
        static std::string SHA256(const char* key, size_t key_len, const char* data, size_t data_len, bool upper = true) {
            if (data == nullptr || data_len == 0) {
                return "";
            }
            char result[SHA256_DIGEST_LENGTH] = {0};
            unsigned int len = 0;
            ::HMAC(EVP_sha256(), key, key_len, (const unsigned char*)data, data_len, (unsigned char*)result, &len);
            std::string signature(len * 2, '\0');
            HexToString(result, len, (unsigned char*)signature.data(), upper);
            return signature;
        }
    };
}