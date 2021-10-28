/*************************************************
File name:  des.hpp
Author:
Date:
Description:    基于openssl提供的des算法支持进行封装

NOTE: 密码生成
    1 进入对应openssl下运行目录(比如bin)
    2 新建文件夹(比如keygen),在该文件夹下创建密码本(比如passphrase.txt),在密码本中输入原始密码(比如:12345678)
    3 openssl运行目录中
            windows下打开cmd,执行openssl enc -des-cfb -kfile keygen/passphrase.txt -md md5 -P -salt
            输出salt=2367639D67CF379D
                key=A574B92F77AA3233
                iv =AD27557C4587BB8A
            salt每次均为随机生成
*************************************************/
#pragma once

#include <string>
#include <vector>
#include <openssl/des.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include "encrypt_base.hpp"
#include "../scope_guard.hpp"

namespace BTool {
    class Des : public EncryptBase
    {
    private:
        static constexpr inline size_t get_blocksize() {
            return sizeof(DES_cblock);
        }
        // 校验密钥
        // 返回密钥个数
        static size_t check_key(const char* key, const char* iv) {
            if (strlen(iv) != get_blocksize())
                return 0;

            size_t len = strlen(key);
            if (len != get_blocksize() && len != 2 * get_blocksize() && len != 3 * get_blocksize())
                return 0;

            return len / get_blocksize();
        }

    public:
        // 加密方式
        enum class Mode : int {
            CBC = 0,
            CFB,
            ECB
        };

#pragma region DES加密/解密
        /************************************************************************
         *功能: DES加密
         *参数: key: 8byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: 输入待加密参数内容
                src_len:  输入待加密参数长度
                out_buff: 输出加密后数据指针
                out_len:  输出加密后数据长度
                mode:     加密方式
                iv:       仅CBC CFB两种模式才有此值,长度为8byte
          返回是否正确加密
         ************************************************************************/
        static std::string EncryptToHex(const char* key
            , const char* data, size_t data_len
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
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

            size_t out_len = 0;
            if (!des_encrypt_and_decrypt(key, src_buff, src_len, out_buff, out_len, mode, pad, iv, DES_ENCRYPT)) {
                return "";
            }

            return std::string((const char*)out_buff, out_len);
        }
        /************************************************************************
         *功能: DES加密 + base64加密
         *目的: 为实现字符串传输,对DES加密后的内存数据进行base64加密,改为可识别字符串进行传输
         *参数: key:  8byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   仅CBC CFB两种模式才有此值,长度为8byte
          返回加密数据,失败则返回空数据
         ************************************************************************/
        static std::string EncryptToBase64(const char* key, const std::string& data
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            return EncryptToBase64(key, data.c_str(), data.length(), mode, pad, iv);
        }
        static std::string EncryptToBase64(const char* key
            , const char* data, size_t data_len
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            std::string hex = EncryptToHex(key, data, data_len, mode, pad, iv);
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
         *功能: DES解密
         *参数: key:      8byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: 输入待解密参数内容
                src_len:  输入待解密参数长度
                out_buff: 输出解密后数据指针
                out_len:  输出解密后数据长度
                mode:     解密方式
                iv:       仅CBC CFB两种模式才有此值,长度为8byte
          返回是否正确解密
         ************************************************************************/
        static std::string DecryptFromHex(const char* key
            , const unsigned char* src_buff, size_t src_len
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            unsigned char* out_buff = new unsigned char[src_len] {0};
            ScopeGuard ext([&] {
                delete[] out_buff;
                });

            size_t out_len = 0;
            if (!des_encrypt_and_decrypt(key, src_buff, src_len, out_buff, out_len, mode, pad, iv, DES_DECRYPT)) {
                return "";
            }

            out_len = ClearDataPadding(out_buff, out_len, pad, get_blocksize());

            return std::string((const char*)out_buff, out_len);
        }
        /************************************************************************
         *功能: base64解密 + DES解密
         *目的: 先对数据进行base64解密,再采用DES解密
         *参数: key:  8byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   仅CBC CFB两种模式才有此值,长度为8byte
          返回解密数据,失败则返回空数据
         ************************************************************************/
        static std::string DecryptFromBase64(const char* key, const std::string& data
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            unsigned char* out_buff = new unsigned char[data.length()]{ 0 };
            ScopeGuard ext([&] {
                delete[] out_buff;
                });
            size_t out_len(0);
            if (!FromBase64(data, out_buff, out_len)) {
                return "";
            }
            return DecryptFromHex(key, out_buff, out_len, mode, pad, iv);
        }
#pragma endregion

#pragma region 3DES加密/解密
        /************************************************************************
         *功能: 3DES加密
         *参数: key: 8/16/24byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: 输入待加密参数内容
                src_len:  输入待加密参数长度
                out_buff: 输出加密后数据指针
                out_len:  输出加密后数据长度
                mode:     加密方式
                iv:       仅CBC CFB两种模式才有此值,长度为8byte
          返回是否正确加密
         ************************************************************************/
        static std::string TripleDesEncryptToHex(const char* key
            , const char* data, size_t data_len
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
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
            size_t out_len = 0;
            if (!triple_des_encrypt_and_decrypt(key, src_buff, src_len, out_buff, out_len, mode, pad, iv, DES_ENCRYPT)) {
                return "";
            }
            return std::string((const char*)out_buff, out_len);
        }
        /************************************************************************
         *功能: 3DES加密 + base64加密
         *目的: 为实现字符串传输,对3DES加密后的内存数据进行base64加密,改为可识别字符串进行传输
         *参数: key:  8/16/24byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   仅CBC CFB两种模式才有此值,长度为8byte
          返回加密数据,失败则返回空数据
         ************************************************************************/
        static std::string TripleDesEncryptToBase64(const char* key
            , const std::string& data
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            return TripleDesEncryptToBase64(key, data.c_str(), data.length(), mode, pad, iv);
        }
        static std::string TripleDesEncryptToBase64(const char* key
            , const char* data, size_t data_len
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            std::string hex = TripleDesEncryptToHex(key, data, data_len, mode, pad, iv);
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
         *功能: 3DES解密
         *参数: key: 8/16/24byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: 输入待解密参数内容
                src_len:  输入待解密参数长度
                out_buff: 输出解密后数据指针
                out_len:  输出解密后数据长度
                mode:     解密方式
                iv:       仅CBC CFB两种模式才有此值,长度为8byte
          返回是否正确解密
         ************************************************************************/
        static std::string TripleDesDecryptFromHex(const char* key
            , const unsigned char* src_buff, size_t src_len
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            unsigned char* out_buff = new unsigned char[src_len] {0};
            ScopeGuard ext([&] {
                delete[] out_buff;
                });

            size_t out_len = 0;
            if (!triple_des_encrypt_and_decrypt(key, src_buff, src_len, out_buff, out_len, mode, pad, iv, DES_DECRYPT)) {
                return "";
            }

            out_len = ClearDataPadding(out_buff, out_len, pad, get_blocksize());
            return std::string((const char*)out_buff, out_len);
        }
        /************************************************************************
         *功能: base64解密 + 3DES解密
         *目的: 先对数据进行base64解密,再采用3DES解密
         *参数: key:  8/16/24byte密钥,如: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: 输入待加密字符串内容
                mode: 加密方式
                iv:   仅CBC CFB两种模式才有此值,长度为8byte
          返回解密数据,失败则返回空数据
         ************************************************************************/
        static std::string TripleDesDecryptFromBase64(const char* key
            , const std::string& data
            , Mode mode = Mode::ECB, Padding pad = Padding::ZeroPadding
            , const char* iv = nullptr)
        {
            unsigned char* out_buff = new unsigned char[data.length()]{ 0 };
            ScopeGuard ext([&] {
                delete[] out_buff;
                });
            size_t out_len(0);
            if (!FromBase64(data, out_buff, out_len)) {
                return "";
            }
            return TripleDesDecryptFromHex(key, out_buff, out_len, mode, pad, iv);
        }
#pragma endregion

    private:
        static bool des_encrypt_and_decrypt(const char* key
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode, Padding pad
            , const char* iv, const int enc)
        {
            size_t key_num = check_key(key, iv);
            if (key_num != 1)
                return false;

            DES_key_schedule ks1;
            DES_set_key_unchecked((const_DES_cblock*)key, &ks1);

            out_len = src_len;

            unsigned char tmpData[get_blocksize()] = { 0 };
            switch (mode)
            {
            case Mode::CBC:
                memcpy(tmpData, iv, get_blocksize());
                DES_cbc_encrypt(src_buff, out_buff, (long)src_len, &ks1, (DES_cblock*)tmpData, enc);
                break;
            case Mode::CFB:
                memcpy(tmpData, iv, get_blocksize());
                DES_cfb_encrypt(src_buff, out_buff, get_blocksize(), (long)src_len, &ks1, (DES_cblock*)tmpData, enc);
                break;
            //case Mode::ECB:
            default:
            {
                size_t block_num = src_len / get_blocksize();
                size_t mod_val = src_len % get_blocksize();
                if (mod_val != 0) {
                    ++block_num;
                    out_len = block_num * get_blocksize();
                }

                for (size_t i = 0; i < block_num; i++) {
                    if (mod_val == 0 || i != block_num - 1) {
                        memcpy(tmpData, src_buff + i * get_blocksize(), get_blocksize());
                    }
                    else {
                        memset(tmpData, 0, sizeof(tmpData));
                        memcpy(tmpData, src_buff + i * get_blocksize(), mod_val);
                    }

                    DES_ecb_encrypt((const_DES_cblock*)tmpData, (DES_cblock*)(out_buff + i * get_blocksize()), &ks1, enc);
                }

                //for (size_t i = 0; i < block_num; i++) {
                //    if (mod_val == 0 || i != block_num - 1) {
                //        DES_ecb_encrypt((const_DES_cblock*)src_buff + i * get_blocksize(), (DES_cblock*)(out_buff + i * get_blocksize()), &ks1, DES_ENCRYPT);
                //    }
                //    else {
                //        unsigned char tmpData[get_blocksize()] = { 0 };
                //        memcpy(tmpData, src_buff + i * get_blocksize(), mod_val);
                //        DES_ecb_encrypt((const_DES_cblock*)tmpData, (DES_cblock*)(out_buff + i * get_blocksize()), &ks1, DES_ENCRYPT);

                //    }

                //}
            }
            break;
            }
            return true;
        }
        static bool triple_des_encrypt_and_decrypt(const char* key
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode, Padding pad
            , const char* iv, const int enc)
        {
            size_t key_num = check_key(key, iv);
            if (key_num == 0)
                return false;

            unsigned char  ke1[get_blocksize()] = { 0 }, ke2[get_blocksize()] = { 0 }, ke3[get_blocksize()] = { 0 };
            DES_key_schedule ks1, ks2, ks3;

            switch (key_num)
            {
            case 1:
                memcpy(ke1, key, get_blocksize());
                memcpy(ke2, ke1, get_blocksize());
                memcpy(ke3, ke1, get_blocksize());
                break;
            case 2:
                memcpy(ke1, key, get_blocksize());
                memcpy(ke2, key + get_blocksize(), get_blocksize());
                memcpy(ke3, ke1, get_blocksize());
                break;
            case 3:
                memcpy(ke1, key, get_blocksize());
                memcpy(ke2, key + get_blocksize(), get_blocksize());
                memcpy(ke3, key + 2 * get_blocksize(), get_blocksize());
                break;
            default:
                break;
            }

            DES_set_key_unchecked((const_DES_cblock*)ke1, &ks1);
            DES_set_key_unchecked((const_DES_cblock*)ke2, &ks2);
            DES_set_key_unchecked((const_DES_cblock*)ke3, &ks3);

            out_len = src_len;


            unsigned char tmpData[get_blocksize()] = { 0 };
            switch (mode)
            {
            case Mode::CBC:
                memcpy(tmpData, iv, get_blocksize());
                DES_ede3_cbc_encrypt(src_buff, out_buff, (long)src_len, &ks1, &ks2, &ks3, (DES_cblock*)tmpData, enc);
                break;
            case Mode::CFB:
                memcpy(tmpData, iv, get_blocksize());
                DES_ede3_cfb_encrypt(src_buff, out_buff, 8, (long)src_len, &ks1, &ks2, &ks3, (DES_cblock*)tmpData, enc);
                break;
            //case Mode::ECB:
            default:
            {
                size_t block_num = src_len / get_blocksize();
                size_t mod_val = src_len % get_blocksize();
                if (mod_val != 0) {
                    ++block_num;
                    out_len = block_num * get_blocksize();
                }

                for (size_t i = 0; i < block_num; i++) {
                    if (mod_val == 0 || i != block_num - 1) {
                        memcpy(tmpData, src_buff + i * get_blocksize(), get_blocksize());
                    }
                    else {
                        memset(tmpData, 0, sizeof(tmpData));
                        memcpy(tmpData, src_buff + i * get_blocksize(), mod_val);
                    }

                    DES_ecb3_encrypt((const_DES_cblock*)tmpData, (DES_cblock*)(out_buff + i * get_blocksize()), &ks1, &ks2, &ks3, enc);
                }

            }
            break;
            }
            return true;
        }
    };
};
