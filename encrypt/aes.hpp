/*************************************************
File name:  aes.hpp
Author:     
Date:
Description:    ����openssl�ṩ��aes�㷨֧�ֽ��з�װ
NOTE: ��������
    1 �����Ӧopenssl������Ŀ¼(����bin)
    2 �½��ļ���(����keygen),�ڸ��ļ����´������뱾(����passphrase.txt),�����뱾������ԭʼ����(����128λʱ����:1234567812345678)
    3 openssl����Ŀ¼��
            windows�´�cmd,ִ��openssl enc -aes-256-ctr -kfile keygen/passphrase.txt -md md5 -P -salt
            ���salt=EA11981133F4D757
                key=824210E90E71933AB0D31F41B4FBF50B57286D155F7A455F4DFD8C4E7CBDEA67
                iv =B70A4392545F7E293685C4B3A54BA58F
            saltÿ�ξ�Ϊ�������,key��iv���ɹ���ο�https://superuser.com/questions/455463/openssl-hash-function-for-generating-aes-key
*************************************************/
#pragma once

#include <openssl/aes.h>
#include "encrypt_base.hpp"

namespace BTool {
    class AES : public EncryptBase
    {
        enum AESBit : int {
            AESNULL = 0,    // ��Ч
            AES128 = 128,
            AES192 = 192,
            AES256 = 256,
        };

    public:
        // ���ܷ�ʽ
        enum class Mode : int {
            ECB = 0,// Electronic Code Book, �������뱾ģʽ
            CBC,    // Cipher Block Chaining, �������ģʽ
            CFB,    // Cipher FeedBack, ���ķ���ģʽ
            OFB,    // Output-Feedback, �������ģʽ
            CTR,    // CounTeR, ������ģʽ
        };

        /************************************************************************
         *����: AES����
         *����: key: ��Կ,����Ϊ 128/192/256 bit(����java��֧��256)
                src_buff: ��������ܲ�������
                src_len:  ��������ܲ�������
                out_buff: ������ܺ�����ָ��
                out_len:  ������ܺ����ݳ���
                mode:     ���ܷ�ʽ, ECB��CBC��Ҫ0λ������16��������,�ⲿ��Ҫȷ��src_buff�ĳ�������Ϊ src_len + AES_BLOCK_SIZE - src_len % AES_BLOCK_SIZE
                iv:       ECBģʽ�����ֵ
          �����Ƿ���ȷ����
         ************************************************************************/
        static bool encrypt(const unsigned char* key, size_t key_len
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode = Mode::CTR, unsigned char iv[AES_BLOCK_SIZE] = { 0 }) 
        {
            return encrypt_and_decrypt(key, key_len, src_buff, src_len, out_buff, out_len, mode, iv, AES_ENCRYPT);
        }
        /************************************************************************
         *����: AES���� + base64����
         *Ŀ��: Ϊʵ���ַ�������,��AES���ܺ���ڴ����ݽ���base64����,��Ϊ��ʶ���ַ������д���
         *����: key:  ��Կ,����Ϊ 128/192/256 bit(����java��֧��256)
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ECBģʽ�����ֵ
          ���ؼ�������,ʧ���򷵻ؿ�����
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
         *����: AES����
         *����: key:      ��Կ,����Ϊ 128/192/256 bit(����java��֧��256)
                src_buff: ��������ܲ�������
                src_len:  ��������ܲ�������
                out_buff: ������ܺ�����ָ��
                out_len:  ������ܺ����ݳ���
                mode:     ���ܷ�ʽ
                iv:       ECBģʽ�����ֵ
          �����Ƿ���ȷ����
         ************************************************************************/
        static bool decrypt(const unsigned char* key, size_t key_len
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode = Mode::CTR, unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            return encrypt_and_decrypt(key, key_len, src_buff, src_len, out_buff, out_len, mode, iv, AES_DECRYPT);
        }
        /************************************************************************
         *����: base64���� + AES����
         *Ŀ��: �ȶ����ݽ���base64����,�ٲ���AES����
         *����: key:  ��Կ,����Ϊ 128/192/256 bit(����java��֧��256)
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ECBģʽ�����ֵ
          ���ؽ�������,ʧ���򷵻ؿ�����
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
        // У����Կ
        static AESBit check_key(const unsigned char* key) {
            return check_key_len(strlen((const char*)key));
        }
        // У����Կ����
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
