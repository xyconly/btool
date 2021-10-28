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

#include <string>
#include <openssl/aes.h>
#include "encrypt_base.hpp"
#include "../scope_guard.hpp"

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
         *����: AES���� + base64����
         *Ŀ��: Ϊʵ���ַ�������,��AES���ܺ���ڴ����ݽ���base64����,��Ϊ��ʶ���ַ������д���
         *����: key:  ��Կ,����Ϊ 128/192/256 bit(����java��֧��256)
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ECBģʽ�����ֵ
          ���ؼ�������,ʧ���򷵻ؿ�����
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
         *����: base64���� + AES����
         *Ŀ��: �ȶ����ݽ���base64����,�ٲ���AES����
         *����: key:  ��Կ,����Ϊ 128/192/256 bit(����java��֧��256)
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ECBģʽ�����ֵ
          ���ؽ�������,ʧ���򷵻ؿ�����
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
        // У����Կ����
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
            // CryptoJS�����·�ע�͵��㷨
            //if (pad == Padding::PKCS5Padding)
            //    return AES_BLOCK_SIZE;

            //// ȡAES_BLOCK_SIZE����
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
