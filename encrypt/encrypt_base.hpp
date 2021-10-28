/*************************************************
File name:  encrypt_base.hpp
Author:     
Date:
Description:    ����openssl�ṩ��aes�㷨֧�ֽ��з�װ
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
         *����: char�ַ�ת��Ϊ10��������,�� :"A"->10
         *����: byte: ��ת���ַ�
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
         *����: 10��������ת��Ϊchar�ַ�,�� :10->"A"
         *����: byte: ��ת���ַ�
         ************************************************************************/
        static inline unsigned char get_number_to_byte(int number) {
            if (number >= 10)
                return 'A' + number - 10;
            else
                return '0' + number;
        }

    public:
        /************************************************************************
         *����: �ַ���ת��Ϊ16��������,�� :"AABB"->0xAA 0xBB
         *����: source: ��ת���ַ���
                len   : ��ת���ַ�������,ע��,��ֵ����Ϊż��
                num   : ��������׵�ַָ��
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
         *����: ������ת��Ϊ�ַ���,�� :0xAA 0xBB->"AABB"
         *����: source: ��ת���ڴ������
                len   : ��ת���ڴ�����Ƴ���
                num   : ��������׵�ַָ��,ע��,�������Ϊlen*2
         *����
                �Ƿ���ȷת��
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

#pragma region Base64����/����
        /************************************************************************
         *����: Base64����
         *����: str:     ��ת������
                str_len: ��ת�����鳤��
                encode:  ������
         *����
                �Ƿ���ȷת��
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
         *����: Base64����
         *����: str:        �������ַ�
                decode:     ���ܺ�����ָ��
                buffer_len: ���ܺ����鳤��
         *����:
                �Ƿ���ȷ����
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

        // ��䷽ʽ
        enum class Padding : int {
            NonePadding = 0,    // ��padding
            ZeroPadding,        // Electronic Code Book, �������뱾ģʽ
            PKCS5Padding,       // Cipher Block Chaining, �������ģʽ
            PKCS7Padding,       // Cipher FeedBack, ���ķ���ģʽ
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
         *����: ������׷��padding
         *����: src_buff:   ԭ�ַ�
                src_len:    ԭ�ַ����鳤��
                out_buff:   ����ַ�
                padding_len: �����볤��
         ************************************************************************/
        static void PaddingData(const char* src_buff, size_t src_len, unsigned char* out_buff, Padding padding, size_t padding_len) {
            memcpy(out_buff, src_buff, src_len);
            if (padding_len > 0) {
                char padding_chr = padding == Padding::ZeroPadding ? 0 : padding_len;
                memset(out_buff + src_len, padding_chr, padding_len);
            }
        }
        /************************************************************************
         *����: ���������padding
         *����: src_buff:   ԭ�ַ�
                src_len:    ԭ�ַ����鳤��
         *����:
                paddingǰ, �ַ����鳤��
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
