/*************************************************
File name:  rsa.hpp
Author:
Date:
Description:    基于openssl提供的rsa算法支持进行封装
NOTE: 密码生成
    1 进入对应openssl下运行目录(比如bin)
    2 新建文件夹(比如keygen),在该文件夹下创建密码本(比如passphrase.txt),在密码本中输入原始密码(比如128位时输入:1234567812345678)
    3 openssl运行目录中
            windows下打开cmd,执行openssl enc -rsa-256-ctr -kfile keygen/passphrase.txt -md md5 -P -salt
            输出salt=EA11981133F4D757
                key=824210E90E71933AB0D31F41B4FBF50B57286D155F7A455F4DFD8C4E7CBDEA67
                iv =B70A4392545F7E293685C4B3A54BA58F
            salt每次均为随机生成,key与iv生成规则参考https://superuser.com/questions/455463/openssl-hash-function-for-generating-rsa-key
*************************************************/
#pragma once

#include <string>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "encrypt_base.hpp"
#include "../scope_guard.hpp"

namespace BTool {
    class RSA : public EncryptBase
    {
    public:
        /************************************************************************
         *功能: RSA加密
         *参数: key: 密钥,长度为 128/192/256 bit(部分java不支持256)
                src_buff: 输入待加密参数内容
                src_len:  输入待加密参数长度
                out_buff: 输出加密后数据指针
                out_len:  输出解密后数据长度
                mode:     加密方式, ECB和CBC需要0位补齐至16的整数倍,外部需要确保src_buff的长度最少为 src_len + RSA_BLOCK_SIZE - src_len % RSA_BLOCK_SIZE
                iv:       ECB模式无需此值
          返回是否正确加密
         ************************************************************************/
        static std::string EncryptToHex(const char* key, const char* data, size_t data_len, Padding pad = Padding::PKCS7Padding) {
            // 创建 RSA 结构体
            ::BIO* bio = BIO_new_mem_buf(key, -1);
            ::RSA* rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
            if (!rsa) {
                return "";
            }
            // 根据填充方式选择合适的加密函数
            int padding;
            switch (pad) {
            case Padding::NonePadding:
                padding = RSA_NO_PADDING;
                break;
            default:
                padding = RSA_PKCS1_PADDING;
                break;
            }
            // 计算加密后的数据大小
            int rsa_size = RSA_size(rsa);
            std::string encrypted(rsa_size, '\0');
            // 加密
            int result = RSA_public_encrypt(data_len, (const unsigned char*)data, (unsigned char*)encrypted.data(), rsa, padding);
            if (result == -1) {
                RSA_free(rsa);
                return "";
            }
            RSA_free(rsa);
            return encrypted;
        }

        /************************************************************************
         *功能: RSA加密 + base64加密
         *目的: 为实现字符串传输,对RSA加密后的内存数据进行base64加密,改为可识别字符串进行传输
         *参数: key:  密钥,长度为 128/192/256 bit(部分java不支持256)
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   ECB模式无需此值
          返回加密数据,失败则返回空数据
         ************************************************************************/
        static std::string EncryptToBase64(const char* key, const std::string& data, Padding pad = Padding::PKCS7Padding) {
            return EncryptToBase64(key, data.c_str(), data.length(), pad);
        }
        static std::string EncryptToBase64(const char* key, const char* data, size_t data_len, Padding pad = Padding::PKCS7Padding) {
            std::string hex = EncryptToHex(key, data, data_len, pad);
            if (hex.empty()) {
                return "";
            }
            std::string rslt;
            if (!ToBase64(hex.c_str(), hex.length(), rslt)) {
                return "";
            }
            return rslt;
        }
        /************************************************************************
         *功能: RSA解密
         *参数: key:      密钥,长度为 128/192/256 bit(部分java不支持256)
                src_buff: 输入待解密参数内容
                src_len:  输入待解密参数长度
                out_buff: 输出解密后数据指针
                out_len:  输出解密后数据长度
                mode:     解密方式
                iv:       ECB模式无需此值
          返回是否正确解密
         ************************************************************************/
        static std::string DecryptFromHex(const char* key, const unsigned char* src_buff, size_t src_len, Padding pad = Padding::PKCS7Padding) {
            // 创建 RSA 结构体
            ::BIO* bio = BIO_new_mem_buf(key, -1);
            ::RSA* rsa = PEM_read_bio_RSAPrivateKey(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
            if (!rsa) {
                return "";
            }

            // 根据填充方式选择合适的解密函数
            int padding;
            switch (pad) {
            case Padding::NonePadding:
                padding = RSA_NO_PADDING;
                break;
            default:
                padding = RSA_PKCS1_PADDING;
                break;
            }

            // 计算解密后的数据大小
            int rsa_size = RSA_size(rsa);
            std::string decrypted(rsa_size, '\0');

            // 解密
            int result = RSA_private_decrypt(src_len, src_buff, (unsigned char*)decrypted.data(), rsa, padding);
            if (result == -1) {
                RSA_free(rsa);
                return "";
            }

            RSA_free(rsa);
            return decrypted;
        }
        /************************************************************************
         *功能: base64解密 + RSA解密
         *目的: 先对数据进行base64解密,再采用RSA解密
         *参数: key:  密钥,长度为 128/192/256 bit(部分java不支持256)
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   ECB模式无需此值
          返回解密数据,失败则返回空数据
         ************************************************************************/
        static std::string DecryptFromBase64(const char* key, const std::string& data, Padding pad = Padding::ZeroPadding) {
            size_t src_len(data.length());
            unsigned char* src_buff = new unsigned char[src_len];
            ScopeGuard ext([&] {
                delete[] src_buff;
                });
            if (!FromBase64(data, src_buff, src_len)) {
                return "";
            }
            return DecryptFromHex(key, src_buff, src_len, pad);
        }
    };
};
