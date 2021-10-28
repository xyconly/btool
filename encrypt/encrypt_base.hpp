/*************************************************
File name:  encrypt_base.hpp
Author:     
Date:
Description:    基于openssl提供的aes算法支持进行封装
*************************************************/
#pragma once

#include <string>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace BTool {
    class EncryptBase {
    protected:
        /************************************************************************
         *功能: char字符转换为10进制数字,如 :"A"->10
         *参数: byte: 待转换字符
         ************************************************************************/
        static inline int get_byte_to_number(unsigned char byte) {
            if (byte >= 'a')
                return byte - 'a' + 10;
            else if (byte >= 'A')
                return byte - 'A' + 10;
            else
                return byte - '0';
        }
        /************************************************************************
         *功能: 10进制数字转换为char字符,如 :10->"A"
         *参数: byte: 待转换字符
         ************************************************************************/
        static inline unsigned char get_number_to_byte(int number) {
            if (number >= 10)
                return 'A' + number - 10;
            else
                return '0' + number;
        }

    public:
        /************************************************************************
         *功能: 字符串转换为16进制数字,如 :"AABB"->0xAA 0xBB
         *参数: source: 待转换字符串
                len   : 待转换字符串长度,注意,该值必须为偶数
                num   : 输出数组首地址指针
         ************************************************************************/
        static inline void StringToHex(const char* source, size_t len, unsigned char* num) {
            unsigned char highByte, lowByte;

            for (size_t i = 0; i < len; i += 2) {
                highByte = get_byte_to_number(source[i]);
                lowByte = get_byte_to_number(source[i + 1]);

                num[i / 2] = (highByte << 4) | lowByte;
            }
        }

        /************************************************************************
         *功能: 二进制转换为字符串,如 :0xAA 0xBB->"AABB"
         *参数: source: 待转换内存二进制
                len   : 待转换内存二进制长度
                num   : 输出数组首地址指针,注意,输出长度为len*2
         *返回
                是否正确转换
         ************************************************************************/
        static inline void HexToString(const char* source, size_t len, unsigned char* num) {
            unsigned char highByte, lowByte;
            for (size_t i = 0; i < len; ++i) {
                auto& byte = source[i];
                highByte = (byte & 0xf0) >> 4;
                lowByte = byte & 0x0f;
                num[i*2] = get_number_to_byte(highByte);
                num[i*2+1] = get_number_to_byte(lowByte);
            }
        }

#pragma region Base64加密/解密
        /************************************************************************
         *功能: Base64加密
         *参数: str:     待转换数组
                str_len: 待转换数组长度
                encode:  输出结果
         *返回
                是否正确转换
         ************************************************************************/
        static bool ToBase64(const char* str, size_t str_len, std::string& encode) {
            BIO *bmem, *b64;
            BUF_MEM *bptr;

            if (!str || str_len == 0) {
                return false;
            }

            b64 = BIO_new(BIO_f_base64());
            bmem = BIO_new(BIO_s_mem());
            b64 = BIO_push(b64, bmem);
            BIO_write(b64, str, (int)str_len);
            if (BIO_flush(b64)) {
                BIO_get_mem_ptr(b64, &bptr);
                encode = std::string(bptr->data, bptr->length);
            }
            BIO_free_all(b64);

            size_t pos = encode.find('\n');
            while (pos != std::string::npos) {
                encode.replace(pos, 1, "");
                pos = encode.find('\n', (pos + 1));
            }

            return true;
        }

        /************************************************************************
         *功能: Base64解密
         *参数: str:        待解密字符
                decode:     解密后数组指针
                buffer_len: 解密后数组长度
         *返回:
                是否正确解密
         ************************************************************************/
        static bool FromBase64(std::string str, unsigned char* decode, size_t& buffer_len) {
            BIO *bmem, *b64;
            if (str.empty()) {
                return false;
            }
            if (str.rfind('\n') == std::string::npos || str.rfind('\n') != str.length() - 1) {
                str += '\n';
            }

            b64 = BIO_new(BIO_f_base64());
            bmem = BIO_new_mem_buf((void*)str.c_str(), (int)str.length());
            bmem = BIO_push(b64, bmem);
            buffer_len = BIO_read(bmem, decode, (int)str.length());
            decode[buffer_len] = 0;
            BIO_free_all(bmem);
            return true;
        }

#pragma endregion

        // 填充方式
        enum class Padding : int {
            NonePadding = 0,    // 无padding
            ZeroPadding,        // Electronic Code Book, 电子密码本模式
            PKCS5Padding,       // Cipher Block Chaining, 密码块链模式
            PKCS7Padding,       // Cipher FeedBack, 密文反馈模式
        };

#pragma region Padding
        static size_t GetPaddingLen(size_t src_len, Padding padding, size_t blocksize) {
            size_t padding_len = blocksize - src_len % blocksize;
            if (padding == Padding::NonePadding 
                || (padding == Padding::ZeroPadding && padding_len == blocksize)
                ) {
                return 0;
            }
            return padding_len;
        }

        /************************************************************************
         *功能: 对数据追加padding
         *参数: src_buff:   原字符
                src_len:    原字符数组长度
                out_buff:   输出字符
                padding_len: 待补齐长度
         ************************************************************************/
        static void PaddingData(const char* src_buff, size_t src_len, unsigned char* out_buff, Padding padding, size_t padding_len) {
            memcpy(out_buff, src_buff, src_len);
            if (padding_len > 0) {
                char padding_chr = padding == Padding::ZeroPadding ? 0 : padding_len;
                memset(out_buff + src_len, padding_chr, padding_len);
            }
        }
        /************************************************************************
         *功能: 对数据清除padding
         *参数: src_buff:   原字符
                src_len:    原字符数组长度
         *返回:
                padding前, 字符数组长度
         ************************************************************************/
        static size_t ClearDataPadding(const unsigned char* src_buff, size_t src_len, Padding padding, size_t block_size) {
            if (padding == Padding::NonePadding
                || padding == Padding::ZeroPadding) {
                return src_len;
            }
            return src_len - (size_t)src_buff[src_len - 1];
        }
#pragma endregion

    };
};
