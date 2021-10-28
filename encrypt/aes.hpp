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

#include <string>
#include <openssl/aes.h>
#include "encrypt_base.hpp"
#include "../scope_guard.hpp"

namespace BTool {
    class AES : public EncryptBase
    {
    public:
        // 数据块位数
        enum AESBit : int {
            AESNULL = 0,    // 无效
            AES128 = 128,
            AES192 = 192,
            AES256 = 256,
        };

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
        static std::string EncryptToHex(const char* key
            , const char* data, size_t data_len
            , Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128
            , const char* iv = nullptr)
        {
            size_t padding_len = GetPaddingLen(data_len, pad, get_blocksize());
            size_t src_len = data_len + padding_len;

            unsigned char* src_buff = new unsigned char[src_len] {0};
            unsigned char* out_buff = new unsigned char[src_len] {0};
            ScopeGuard ext([&] {
                delete[] out_buff;
                delete[] src_buff;
                });

            PaddingData(data, data_len, src_buff, pad, padding_len);

            if (!encrypt_and_decrypt(key, src_buff, src_len, out_buff, get_blocksize(), mode, pad, bits, iv, AES_ENCRYPT)) {
                return "";
            }
            return std::string((const char*)out_buff, src_len);
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
        static std::string EncryptToBase64(const char* key, const std::string& data
            , Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128
            , const char* iv = nullptr)
        {
            return EncryptToBase64(key, data.c_str(), data.length(), mode, pad, bits, iv);
        }
        static std::string EncryptToBase64(const char* key
            , const char* data, size_t data_len
            , Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128
            , const char* iv = nullptr)
        {
            std::string hex = EncryptToHex(key, data, data_len, mode, pad, bits, iv);
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
        static std::string DecryptFromHex(const char* key
            , const unsigned char* src_buff, size_t src_len
            , Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128
            , const char* iv = nullptr)
        {
            //size_t blocksize = get_blocksize(/*bits, pad*/);

            unsigned char* out_buff = new unsigned char[src_len];
            ScopeGuard ext([&] {
                delete[] out_buff;
                });

            if (!encrypt_and_decrypt(key, src_buff, src_len, out_buff, get_blocksize(), mode, pad, bits, iv, AES_DECRYPT)) {
                return "";
            }

            size_t out_len = ClearDataPadding(out_buff, src_len, pad, get_blocksize());
            return std::string((const char*)out_buff, out_len);
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
        static std::string DecryptFromBase64(const char* key
            , const std::string& data
            , Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128
            , const char* iv = nullptr)
        {
            size_t src_len(data.length());
            unsigned char* src_buff = new unsigned char[src_len];
            ScopeGuard ext([&] {
                delete[] src_buff;
                });
            if (!FromBase64(data, src_buff, src_len)) {
                return "";
            }
            return DecryptFromHex(key, src_buff, src_len, mode, pad, bits, iv);
        }

    private:
        // 校验密钥长度
        static inline bool check_len_valid(const char* key, Mode mode, AESBit bits, const char* iv, unsigned char*& tmp_iv) {
            if (!check_len(strlen(key), bits))
                return false;

            if (mode == Mode::ECB)
                return true;

            if (!iv /*|| strlen(iv) > AES_BLOCK_SIZE*/)
                return false;

            tmp_iv = new unsigned char[AES_BLOCK_SIZE];
            memcpy(tmp_iv, iv, AES_BLOCK_SIZE);
            return true;
        }
        static inline bool check_len(size_t len, AESBit bits) {
            return len >= (bits / 8);
        }

        static constexpr inline size_t get_blocksize(/*AESBit bits, Padding pad*/) {
            return AES_BLOCK_SIZE;
            // CryptoJS采用下方注释的算法
            //if (pad == Padding::PKCS5Padding)
            //    return AES_BLOCK_SIZE;

            //// 取AES_BLOCK_SIZE倍数
            //if (bits == BTool::AES::AES192)
            //    return AES_BLOCK_SIZE * 2;
            //
            //return bits / 8;
        }

        static bool encrypt_and_decrypt(const char* key
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t blocksize
            , Mode mode, Padding pad, AESBit bits, const char* iv, const int enc)
        {
            unsigned char* tmp_iv(nullptr);
            ScopeGuard ext([&] {
                if (tmp_iv)
                    delete[] tmp_iv;
                });
            if (!check_len_valid(key, mode, bits, iv, tmp_iv))
                return false;

            AES_KEY aes;
            if (enc == AES_ENCRYPT) {
                if (AES_set_encrypt_key((const unsigned char*)key, bits, &aes) < 0)
                    return false;
            }
            else {
                if (AES_set_decrypt_key((const unsigned char*)key, bits, &aes) < 0)
                    return false;
            }

            switch (mode)
            {
            case Mode::ECB:
                for (size_t i = 0; i < src_len; i += AES_BLOCK_SIZE) {
                    AES_ecb_encrypt(src_buff + i, out_buff + i, &aes, enc);
                }
                break;
            case Mode::CBC:
                AES_cbc_encrypt(src_buff, out_buff, src_len, &aes, tmp_iv, enc);
                break;
            case Mode::CFB:
                if (bits != AES128)
                    return false;
                {
                    int number(0);
                    AES_cfb128_encrypt(src_buff, out_buff, src_len, &aes, tmp_iv, (int*)&number, enc);
                }
                break;
            case Mode::OFB:
                if (bits != AES128)
                    return false;
                {
                    int number(0);
                    AES_ofb128_encrypt(src_buff, out_buff, src_len, &aes, tmp_iv, (int*)&number);
                }
                break;
            //case Mode::CTR:
            default:
                if (bits != AES128)
                    return false;
                {
                    unsigned char ecount_buf[AES_BLOCK_SIZE] = { 0 };
                    int number(0);

                    //AES_ctr128_encrypt(src_buff, out_buff, src_len, &aes, tmp_iv, ecount_buf, (unsigned int*)&number);
                }
                break;
            }

            return true;
        }
    };
};
