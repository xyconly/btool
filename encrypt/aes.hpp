/*************************************************
File name:  aes.hpp
Author:     
Date:
Description:    基于openssl提供的aes算法支持进行封装
NOTE: 密码生成
    1 进入对应openssl下运行目录(比如bin)
    2 新建文件夹(比如keygen),在该文件夹下创建密码本(比如passphrase.txt),在密码本中输入原始密码(比如128位时输入:1234567812345678)
    3 openssl运行目录中
            windows下打开cmd,执行openssl enc -aes-256-ctr -kfile keygen/passphrase.txt -md md5 -P -salt
            输出salt=EA11981133F4D757
                key=824210E90E71933AB0D31F41B4FBF50B57286D155F7A455F4DFD8C4E7CBDEA67
                iv =B70A4392545F7E293685C4B3A54BA58F
            salt每次均为随机生成,key与iv生成规则参考https://superuser.com/questions/455463/openssl-hash-function-for-generating-aes-key
*************************************************/
#pragma once

#include <openssl/aes.h>
#include "encrypt_base.hpp"

namespace BTool {
    class AES : public EncryptBase
    {
        enum AESBit : int {
            AESNULL = 0,    // 无效
            AES128 = 128,
            AES192 = 192,
            AES256 = 256,
        };

    public:
        // 加密方式
        enum class Mode : int {
            ECB = 0,// Electronic Code Book, 电子密码本模式
            CBC,    // Cipher Block Chaining, 密码块链模式
            CFB,    // Cipher FeedBack, 密文反馈模式
            OFB,    // Output-Feedback, 输出反馈模式
            CTR,    // CounTeR, 计数器模式
        };

        /************************************************************************
         *功能: AES加密
         *参数: key: 密钥,长度为 128/192/256 bit(部分java不支持256)
                src_buff: 输入待加密参数内容
                src_len:  输入待加密参数长度
                out_buff: 输出加密后数据指针
                out_len:  输出解密后数据长度
                mode:     加密方式, ECB和CBC需要0位补齐至16的整数倍,外部需要确保src_buff的长度最少为 src_len + AES_BLOCK_SIZE - src_len % AES_BLOCK_SIZE
                iv:       ECB模式无需此值
          返回是否正确加密
         ************************************************************************/
        static bool encrypt(const unsigned char* key, size_t key_len
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode = Mode::CTR, unsigned char iv[AES_BLOCK_SIZE] = { 0 }) 
        {
            return encrypt_and_decrypt(key, key_len, src_buff, src_len, out_buff, out_len, mode, iv, AES_ENCRYPT);
        }
        /************************************************************************
         *功能: AES加密 + base64加密
         *目的: 为实现字符串传输,对AES加密后的内存数据进行base64加密,改为可识别字符串进行传输
         *参数: key:  密钥,长度为 128/192/256 bit(部分java不支持256)
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   ECB模式无需此值
          返回加密数据,失败则返回空数据
         ************************************************************************/
        static std::string encrypt_base64(const unsigned char* key, size_t key_len
            , const std::string& data, Mode mode = Mode::CTR, unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            size_t src_len = data.length();
            if (mode == Mode::CBC || mode == Mode::ECB)
                src_len += AES_BLOCK_SIZE - src_len % AES_BLOCK_SIZE;
            unsigned char* out_buf = new unsigned char[src_len];
            size_t out_len(0);
            if (!encrypt(key, key_len, (const unsigned char*)data.c_str(), data.length(), out_buf, out_len, mode, iv)) {
                delete[] out_buf;
                return "";
            }

            std::string rslt;
            if (!base64_encode(out_buf, data.length(), rslt)) {
                delete[] out_buf;
                return "";
            }

            delete[] out_buf;
            return rslt;
        }
        /************************************************************************
         *功能: AES解密
         *参数: key:      密钥,长度为 128/192/256 bit(部分java不支持256)
                src_buff: 输入待解密参数内容
                src_len:  输入待解密参数长度
                out_buff: 输出解密后数据指针
                out_len:  输出解密后数据长度
                mode:     解密方式
                iv:       ECB模式无需此值
          返回是否正确解密
         ************************************************************************/
        static bool decrypt(const unsigned char* key, size_t key_len
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode = Mode::CTR, unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            return encrypt_and_decrypt(key, key_len, src_buff, src_len, out_buff, out_len, mode, iv, AES_DECRYPT);
        }
        /************************************************************************
         *功能: base64解密 + AES解密
         *目的: 先对数据进行base64解密,再采用AES解密
         *参数: key:  密钥,长度为 128/192/256 bit(部分java不支持256)
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   ECB模式无需此值
          返回解密数据,失败则返回空数据
         ************************************************************************/
        static std::string decrypt_base64(const unsigned char* key, size_t key_len
            , const std::string& data, Mode mode = Mode::CTR, unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            size_t out_len(data.length());
            unsigned char* out_buf = new unsigned char[out_len];
            if (!base64_decode(data, out_buf, out_len)) {
                delete[] out_buf;
                return "";
            }
            size_t rslt_len(out_len);
            unsigned char* rslt_buf = new unsigned char[rslt_len];
            if (!decrypt(key, key_len, out_buf, out_len, rslt_buf, rslt_len, mode, iv)) {
                delete[] out_buf;
                delete[] rslt_buf;
                return "";
            }
            std::string rslt((const char*)rslt_buf, rslt_len);
            delete[] out_buf;
            delete[] rslt_buf;
            return rslt;
        }

    private:
        // 校验密钥
        static AESBit check_key(const unsigned char* key) {
            return check_key_len(strlen((const char*)key));
        }
        // 校验密钥长度
        static AESBit check_key_len(size_t len) {
            if (len != 16 && len != 24 && len != 32)
                return AESNULL;

            return (AESBit)(len * 8);
        }

        static bool encrypt_and_decrypt(const unsigned char* key, size_t key_len
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode, unsigned char* iv, const int enc)
        {
            out_len = 0;
            AESBit bits = check_key_len(key_len);
            if (bits == AESNULL)
                return false;

            AES_KEY aes;

            int number(0);
            switch (mode)
            {
            case Mode::ECB:
                if (enc == AES_ENCRYPT) {
                    if (AES_set_encrypt_key(key, bits, &aes) < 0)
                        return false;
                }
                else {
                    if (AES_set_decrypt_key(key, bits, &aes) < 0)
                        return false;
                }

                {
                    size_t block = 0;
                    size_t mod_val = src_len % AES_BLOCK_SIZE;
                    if (mod_val == 0) {
                        block = src_len / AES_BLOCK_SIZE;
                        out_len = src_len;
                    }
                    else {
                        block = src_len / AES_BLOCK_SIZE + 1;
                        out_len = src_len + AES_BLOCK_SIZE - src_len % AES_BLOCK_SIZE;
                    }

                    unsigned char tmpData[AES_BLOCK_SIZE] = { 0 };
                    for (size_t i = 0; i < block; i++)
                    {
                        if (i != block - 1)
                        {
                            memcpy(tmpData, src_buff + i * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
                        }
                        else
                        {
                            memset(tmpData, 0, sizeof(tmpData));
                            memcpy(tmpData, src_buff + i * AES_BLOCK_SIZE, (mod_val == 0) ? AES_BLOCK_SIZE : mod_val);
                        }
                        AES_ecb_encrypt(tmpData, out_buff + i * AES_BLOCK_SIZE, &aes, enc);
                    }
                }
                break;
            case Mode::CBC:
                if (enc == AES_ENCRYPT) {
                    if (AES_set_encrypt_key(key, bits, &aes) < 0)
                        return false;
                }
                else {
                    if (AES_set_decrypt_key(key, bits, &aes) < 0)
                        return false;
                }

                if (src_len % AES_BLOCK_SIZE == 0)
                    out_len = src_len;
                else
                    out_len = src_len + AES_BLOCK_SIZE - src_len % AES_BLOCK_SIZE;
                AES_cbc_encrypt(src_buff, out_buff, out_len, &aes, iv, enc);
                break;
            case Mode::CFB:
                if (AES_set_encrypt_key(key, bits, &aes) < 0)
                    return false;

                if (bits != AES128)
                    return false;

                out_len = src_len;

                AES_cfb128_encrypt(src_buff, out_buff, src_len, &aes, iv, (int*)&number, enc);
                break;
            case Mode::OFB:
                if (AES_set_encrypt_key(key, bits, &aes) < 0)
                    return false;

                if (bits != AES128)
                    return false;

                out_len = src_len;

                AES_ofb128_encrypt(src_buff, out_buff, src_len, &aes, iv, (int*)&number);
                break;
            case Mode::CTR:
            default:
                if (AES_set_encrypt_key(key, bits, &aes) < 0)
                    return false;

                if (bits != AES128)
                    return false;

                out_len = src_len;

                {
                    unsigned char ecount_buf[AES_BLOCK_SIZE] = { 0 };
                    AES_ctr128_encrypt(src_buff, out_buff, src_len, &aes, iv, ecount_buf, (unsigned int*)&number);
                }
                break;
            }

            return true;
        }
    };
};
