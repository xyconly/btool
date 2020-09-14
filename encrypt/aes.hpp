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
    public:
        // ���ݿ�λ��
        enum AESBit : int {
            AESNULL = 0,    // ��Ч
            AES128 = 128,
            AES192 = 192,
            AES256 = 256,
        };

        // ���ܷ�ʽ
        enum class Mode : int {
            ECB = 0,// Electronic Code Book, �������뱾ģʽ
            CBC,    // Cipher Block Chaining, �������ģʽ
            CFB,    // Cipher FeedBack, ���ķ���ģʽ
            OFB,    // Output-Feedback, �������ģʽ
            CTR,    // CounTeR, ������ģʽ
        };

        // ��䷽ʽ
        enum class Padding : int {
            ZeroPadding = 0,    // Electronic Code Book, �������뱾ģʽ
            PKCS5Padding,       // Cipher Block Chaining, �������ģʽ
            PKCS7Padding,       // Cipher FeedBack, ���ķ���ģʽ
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
        static bool encrypt(const unsigned char* key
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128, const unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            return encrypt_and_decrypt(key, src_buff, src_len, out_buff, out_len, mode, pad, bits, iv, AES_ENCRYPT);
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
        static std::string encrypt_base64(const unsigned char* key
            , const std::string& data, Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128, const unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            size_t src_len = data.length();
            size_t blocksize = get_blocksize(bits, pad);
            src_len += blocksize - src_len % blocksize;

            unsigned char* out_buf = new unsigned char[src_len] {0};
            size_t out_len(0);
            if (!encrypt(key, (const unsigned char*)data.c_str(), data.length(), out_buf, out_len, mode, pad, bits, iv)) {
                delete[] out_buf;
                return "";
            }

            std::string rslt;
            if (!base64_encode(out_buf, src_len, rslt)) {
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
        static bool decrypt(const unsigned char* key
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128, const unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            return encrypt_and_decrypt(key, src_buff, src_len, out_buff, out_len, mode, pad, bits, iv, AES_DECRYPT);
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
        static std::string decrypt_base64(const unsigned char* key
            , const std::string& data, Mode mode = Mode::CTR, Padding pad = Padding::ZeroPadding, AESBit bits = AESBit::AES128, const unsigned char iv[AES_BLOCK_SIZE] = { 0 })
        {
            size_t out_len(data.length());
            unsigned char* out_buf = new unsigned char[out_len];
            if (!base64_decode(data, out_buf, out_len)) {
                delete[] out_buf;
                return "";
            }
            size_t rslt_len(out_len);
            unsigned char* rslt_buf = new unsigned char[rslt_len];
            if (!decrypt(key, out_buf, out_len, rslt_buf, rslt_len, mode, pad, bits, iv)) {
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
        // У����Կ����
        static inline bool check_len_valid(const unsigned char* key, Mode mode, AESBit bits, const unsigned char* iv, unsigned char*& tmp_iv) {
            if (!check_len(strlen((const char*)key), bits))
                return false;

            if (mode != Mode::ECB && strlen((const char*)iv) != AES_BLOCK_SIZE)
                return false;

            tmp_iv = new unsigned char[AES_BLOCK_SIZE];
            memcpy(tmp_iv, iv, AES_BLOCK_SIZE);
            return true;
        }
        static inline bool check_len(size_t len, AESBit bits) {
            return len >= (bits / 8);
        }

        static inline size_t get_blocksize(AESBit bits, Padding pad) {
            if (pad == Padding::PKCS5Padding)
                return AES_BLOCK_SIZE;
            return bits / 8;
            //return AES_BLOCK_SIZE;
        }

        static void calculation_padding(Padding pad, size_t blocksize, size_t src_len, size_t& padding_len, int& padding_chr) {
            padding_len = blocksize - src_len % blocksize;
            if(pad == Padding::ZeroPadding)
                padding_chr = 0;
            else
                padding_chr = padding_len;
        }

        struct raii_st {
            raii_st(unsigned char* tmp_iv, unsigned char* inData) :tmp_iv_(tmp_iv), inData_(inData) {}
            ~raii_st() {
                delete[] tmp_iv_;
                delete[] inData_;
            }
            unsigned char* tmp_iv_;
            unsigned char* inData_;
        };
        static bool encrypt_and_decrypt(const unsigned char* key
            , const unsigned char* src_buff, size_t src_len
            , unsigned char* out_buff, size_t& out_len
            , Mode mode, Padding pad, AESBit bits, const unsigned char* iv, const int enc)
        {
            out_len = 0;
            unsigned char* tmp_iv(nullptr);
            if (!check_len_valid(key, mode, bits, iv, tmp_iv))
                return false;

            size_t blocksize = get_blocksize(bits, pad);
            size_t padding_len(0);
            int padding_chr(0);
            if (enc == AES_ENCRYPT) {
                calculation_padding(pad, blocksize, src_len, padding_len, padding_chr);
                out_len = src_len + padding_len;
            }
            else {
                out_len = src_len;
            }

            unsigned char* inData = new unsigned char[out_len];
            memcpy(inData, src_buff, src_len);
            memset(inData + src_len, padding_chr, out_len - src_len);
            
            raii_st auto_st(tmp_iv, inData);

            AES_KEY aes;

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

                for (size_t i = 0; i < out_len;) {
                    AES_ecb_encrypt(inData + i, out_buff + i, &aes, enc);
                    i += AES_BLOCK_SIZE;
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


                AES_cbc_encrypt(inData, out_buff, out_len, &aes, tmp_iv, enc);

                break;
            case Mode::CFB:
                if (AES_set_encrypt_key(key, bits, &aes) < 0)
                    return false;

                if (bits != AES128)
                    return false;

                {
                    int number(0);
                    AES_cfb128_encrypt(inData, out_buff, out_len, &aes, tmp_iv, (int*)&number, enc);
                }
                break;
            case Mode::OFB:
                if (AES_set_encrypt_key(key, bits, &aes) < 0)
                    return false;

                if (bits != AES128)
                    return false;

                {
                    int number(0);
                    AES_ofb128_encrypt(inData, out_buff, out_len, &aes, tmp_iv, (int*)&number);
                }
                break;
            case Mode::CTR:
            default:
                if (AES_set_encrypt_key(key, bits, &aes) < 0)
                    return false;

                if (bits != AES128)
                    return false;

                {
                    unsigned char ecount_buf[AES_BLOCK_SIZE] = { 0 };
                    int number(0);

                    AES_ctr128_encrypt(inData, out_buff, out_len, &aes, tmp_iv, ecount_buf, (unsigned int*)&number);
                }
                break;
            }

            // ������Ҫȥ����������
            if (enc == AES_DECRYPT && mode != Mode::ECB) {
                unsigned char padding_value = out_buff[out_len - 1];
                out_len -= padding_value;
            }

            return true;
        }
    };
};
