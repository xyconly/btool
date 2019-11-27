/*************************************************
File name:  encrypt_base.hpp
Author:     
Date:
Description:    基于openssl提供的aes算法支持进行封装
*************************************************/
#pragma once

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace BTool {
    class EncryptBase {
    public:
        /************************************************************************
         *功能: 字符串转换为16进制数字,如 :"AABB"->0xAA 0xBB
         *参数: source: 待转换字符串
                len   : 待转换字符串长度,注意,该值必须为偶数
                num   : 输出数组首地址指针
          返回是否正确转换
         ************************************************************************/
        static void StringToHex(const char* source, size_t len, unsigned char* num)
        {
            unsigned char highByte, lowByte;

            for (size_t i = 0; i < len; i += 2)
            {
                highByte = toupper(source[i]);
                lowByte = toupper(source[i + 1]);

                if (highByte > '9')
                    highByte -= 'A' - 10;
                else
                    highByte -= '0';

                if (lowByte > '9')
                    lowByte -= 'A' - 10;
                else
                    lowByte -= '0';

                num[i / 2] = (highByte << 4) | lowByte;
            }
        }

#pragma region Base64加密/解密
        /************************************************************************
         *功能: Base64加密
         *参数: str:     待转换数组
                str_len: 待转换数组长度
                encode:  输出结果
          返回是否正确转换
         ************************************************************************/
        static bool base64_encode(const unsigned char* str, size_t str_len, std::string& encode)
        {
            BIO *bmem, *b64;
            BUF_MEM *bptr;

            if (!str || str_len == 0) {
                return false;
            }

            b64 = BIO_new(BIO_f_base64());
            bmem = BIO_new(BIO_s_mem());
            b64 = BIO_push(b64, bmem);
            BIO_write(b64, str, (int)str_len);
            if (BIO_flush(b64))
            {
                BIO_get_mem_ptr(b64, &bptr);
                encode = std::string(bptr->data, bptr->length);
            }
            BIO_free_all(b64);

            size_t pos = encode.find('\n');
            while (pos != std::string::npos)
            {
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
          返回是否正确解密
         ************************************************************************/
        static bool base64_decode(std::string str, unsigned char* decode, size_t& buffer_len)
        {
            BIO *bmem, *b64;
            if (str.empty()) {
                return false;
            }
            if (str.rfind('\n') == std::string::npos || str.rfind('\n') != str.length() - 1)
            {
                str += '\n';
            }

            b64 = BIO_new(BIO_f_base64());
            bmem = BIO_new_mem_buf(str.c_str(), (int)str.length());
            bmem = BIO_push(b64, bmem);
            buffer_len = BIO_read(bmem, decode, (int)str.length());
            decode[buffer_len] = 0;
            BIO_free_all(bmem);
            return true;
        }

#pragma endregion
    };
};
