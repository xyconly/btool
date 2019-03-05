/*************************************************
File name:  des.hpp
Author:     
Date:
Description:    ����openssl�ṩ��des�㷨֧�ֽ��з�װ
*************************************************/
#pragma once

#include <string>
#include <openssl/des.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace BTool {
    class Des
    {
    private:
        // У����Կ
        // ������Կ����
        static size_t check_key(const unsigned char* key) {
            size_t len = strlen((const char*)key);
            if (len != 8 && len != 16 && len != 24)
                return 0;

            return len / 8;
        }

    public:
        // ���ܷ�ʽ
        enum class Mode : int {
            CBC = 0,
            CFB,
            ECB
        };

        /************************************************************************
         *����: �ַ���ת��Ϊ16��������,�� :"AABB"->0xAA 0xBB
         *����: str: ��ת���ַ���
                len: ��ת���ַ�������,ע��,��ֵ����Ϊż��
                num: ��������׵�ַָ��
          �����Ƿ���ȷת��
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

#pragma region Base64����/����
        /************************************************************************
         *����: Base64����
         *����: str:     ��ת������
                str_len: ��ת�����鳤��
                encode:  ������
          �����Ƿ���ȷת��
         ************************************************************************/
        static bool Base64Encode(const unsigned char* str, size_t str_len, std::string& encode)
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
         *����: Base64����
         *����: str:        �������ַ�
                decode:     ���ܺ�����ָ��
                buffer_len: ���ܺ����鳤��
          �����Ƿ���ȷ����
         ************************************************************************/
        static bool Base64Decode(std::string str, unsigned char* decode, size_t& buffer_len)
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


#pragma region DES����/����
        /************************************************************************
         *����: DES���� - zeropadding
         *����: key: 8byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: ��������ܲ�������
                src_len:  ��������ܲ�������
                out_buff: ������ܺ�����ָ��
                out_len:  ������ܺ����ݳ���
                mode:     ���ܷ�ʽ
                iv:       ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          �����Ƿ���ȷ����
         ************************************************************************/
        static bool DesEncode(const unsigned char* key, const unsigned char* src_buff, size_t src_len, unsigned char* out_buff, size_t& out_len, Mode mode = Mode::ECB, unsigned char iv[8] = {0})
        {
            size_t key_num = check_key(key);
            if (key_num != 1)
                return false;

            DES_key_schedule ks1;
            DES_set_key_unchecked((const_DES_cblock*)key, &ks1);

            size_t block = 0;
            size_t mod_val = src_len % 8;
            if (mod_val == 0) {
                block = src_len / 8;
            }
            else {
                block = src_len / 8 + 1;
            }

            out_len = block * 8;

            unsigned char tmpData[8] = { 0 };
            switch (mode)
            {
            case Mode::CBC:
                DES_cbc_encrypt(src_buff, out_buff, (long)src_len, &ks1, (DES_cblock *)iv, DES_ENCRYPT);
                break;
            case Mode::CFB:
                DES_cfb_encrypt(src_buff, out_buff, 8, (long)src_len, &ks1, (DES_cblock *)iv, DES_ENCRYPT);
                break;
            case Mode::ECB:
            default:
                for (size_t i = 0; i < block; i++)
                {
                    if (i != block - 1)
                    {
                        memcpy(tmpData, src_buff + i * 8, 8);
                    }
                    else
                    {
                        memset(tmpData, 0, sizeof(tmpData));
                        memcpy(tmpData, src_buff + i * 8, (mod_val == 0) ? 8 : mod_val);
                    }

                    DES_ecb_encrypt((const_DES_cblock*)tmpData, (DES_cblock *)(out_buff + i * 8), &ks1, DES_ENCRYPT);
                }
                break;
            }
            return true;
        }
        /************************************************************************
         *����: DES���� + base64���� - zeropadding
         *Ŀ��: Ϊʵ���ַ�������,��DES���ܺ���ڴ����ݽ���base64����,��Ϊ��ʶ���ַ������д���
         *����: key:  8byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          ���ؼ�������,ʧ���򷵻ؿ�����
         ************************************************************************/
        static std::string DesEncode_Base64(const unsigned char* key, const std::string& data, Mode mode = Mode::ECB, unsigned char iv[8] = { 0 })
        {
            unsigned char out_buf[8192] = { 0 };

            size_t out_len(0);
            if (!DesEncode(key, (const unsigned char*)data.c_str(), data.length(), out_buf, out_len, mode, iv)) {
                return "";
            }

            std::string rslt;
            if (!Base64Encode(out_buf, out_len, rslt)) {
                return "";
            }

            return rslt;
        }
        /************************************************************************
         *����: DES���� - zeropadding
         *����: key:      8byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: ��������ܲ�������
                src_len:  ��������ܲ�������
                out_buff: ������ܺ�����ָ��
                out_len:  ������ܺ����ݳ���
                mode:     ���ܷ�ʽ
                iv:       ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          �����Ƿ���ȷ����
         ************************************************************************/
        static bool DesDecode(const unsigned char* key, const unsigned char* src_buff, size_t src_len, unsigned char* out_buff, size_t& out_len, Mode mode = Mode::ECB, unsigned char iv[8] = { 0 })
        {
            size_t key_num = check_key(key);
            if (key_num != 1)
                return false;

            DES_key_schedule ks1;
            DES_set_key_unchecked((const_DES_cblock*)key, &ks1);

            size_t block = 0;
            size_t mod_val = src_len % 8;
            if (mod_val == 0) {
                block = src_len / 8;
            }
            else {
                block = src_len / 8 + 1;
            }

            out_len = block * 8;

            unsigned char tmpData[8] = { 0 };
            switch (mode)
            {
            case Mode::CBC:
                DES_cbc_encrypt(src_buff, out_buff, (long)src_len, &ks1, (DES_cblock *)iv, DES_DECRYPT);
                break;
            case Mode::CFB:
                DES_cfb_encrypt(src_buff, out_buff, 8, (long)src_len, &ks1, (DES_cblock *)iv, DES_DECRYPT);
                break;
            case Mode::ECB:
            default:
                for (size_t i = 0; i < block; i++)
                {
                    if (i != block - 1)
                    {
                        memcpy(tmpData, src_buff + i * 8, 8);
                    }
                    else
                    {
                        memset(tmpData, 0, sizeof(tmpData));
                        memcpy(tmpData, src_buff + i * 8, (mod_val == 0) ? 8 : mod_val);
                    }

                    DES_ecb_encrypt((const_DES_cblock*)tmpData, (DES_cblock *)(out_buff + i * 8), &ks1, DES_DECRYPT);
                }
                break;
            }
            return true;
        }
        /************************************************************************
         *����: base64���� + DES���� - zeropadding
         *Ŀ��: �ȶ����ݽ���base64����,�ٲ���DES����
         *����: key:  8byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          ���ؽ�������,ʧ���򷵻ؿ�����
         ************************************************************************/
        static std::string DesDecode_Base64(const unsigned char* key, const std::string& data, Mode mode = Mode::ECB, unsigned char iv[8] = { 0 })
        {
            unsigned char out_buf[8192] = { 0 };
            size_t out_len(0);
            if (!Base64Decode(data, out_buf, out_len)) {
                return "";
            }
            unsigned char rslt_buf[8192] = { 0 };
            size_t rslt_len(0);
            if (!DesDecode(key, out_buf, out_len, rslt_buf, rslt_len, mode, iv)) {
                return "";
            }
            return std::string((const char*)rslt_buf, rslt_len);
        }
#pragma endregion

#pragma region 3DES����/����
        /************************************************************************
         *����: 3DES���� - zeropadding
         *����: key: 8/16/24byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: ��������ܲ�������
                src_len:  ��������ܲ�������
                out_buff: ������ܺ�����ָ��
                out_len:  ������ܺ����ݳ���
                mode:     ���ܷ�ʽ
                iv:       ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          �����Ƿ���ȷ����
         ************************************************************************/
        static bool TripleDesEncode(const unsigned char* key, const unsigned char* src_buff, size_t src_len, unsigned char* out_buff, size_t& out_len, Mode mode = Mode::ECB, unsigned char iv[8] = { 0 })
        {
            size_t key_num = check_key(key);
            if (key_num == 0)
                return false;

            unsigned char  ke1[8] = { 0 }, ke2[8] = { 0 }, ke3[8] = { 0 };
            DES_key_schedule ks1, ks2, ks3;

            switch (key_num)
            {
            case 1:
                memcpy(ke1, key, 8);
                memcpy(ke2, ke1, 8);
                memcpy(ke3, ke1, 8);
                break;
            case 2:
                memcpy(ke1, key, 8);
                memcpy(ke2, key + 8, 8);
                memcpy(ke3, ke1, 8);
                break;
            case 3:
                memcpy(ke1, key, 8);
                memcpy(ke2, key + 8, 8);
                memcpy(ke3, key + 16, 8);
                break;
            default:
                break;
            }

            DES_set_key_unchecked((const_DES_cblock*)ke1, &ks1);
            DES_set_key_unchecked((const_DES_cblock*)ke2, &ks2);
            DES_set_key_unchecked((const_DES_cblock*)ke3, &ks3);

            size_t block = 0;
            size_t mod_val = src_len % 8;
            if (mod_val == 0) {
                block = src_len / 8;
            }
            else {
                block = src_len / 8 + 1;
            }

            out_len = block * 8;

            unsigned char tmpData[8] = { 0 };

            switch (mode)
            {
            case Mode::CBC:
                memcpy(tmpData, iv, 8);
                DES_ede3_cbc_encrypt(src_buff, out_buff, (long)src_len, &ks1, &ks2, &ks3, (DES_cblock *)tmpData, DES_ENCRYPT);
                break;
            case Mode::CFB:
                memcpy(tmpData, iv, 8);
                DES_ede3_cfb_encrypt(src_buff, out_buff, 8, (long)src_len, &ks1, &ks2, &ks3, (DES_cblock *)tmpData, DES_ENCRYPT);
            case Mode::ECB:
            default:
                for (size_t i = 0; i < block; i++)
                {
                    if (i != block - 1)
                    {
                        memcpy(tmpData, src_buff + i * 8, 8);
                    }
                    else
                    {
                        memset(tmpData, 0, sizeof(tmpData));
                        memcpy(tmpData, src_buff + i * 8, (mod_val == 0) ? 8 : mod_val);
                    }
                    DES_ecb3_encrypt((const_DES_cblock*)tmpData, (DES_cblock *)(out_buff + i * 8), &ks1, &ks2, &ks3, DES_ENCRYPT);
                }
                break;
            }
            return true;
        }
        /************************************************************************
         *����: 3DES���� + base64���� - zeropadding
         *Ŀ��: Ϊʵ���ַ�������,��3DES���ܺ���ڴ����ݽ���base64����,��Ϊ��ʶ���ַ������д���
         *����: key:  8/16/24byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          ���ؼ�������,ʧ���򷵻ؿ�����
         ************************************************************************/
        static std::string TripleDesEncode_Base64(const unsigned char* key, const std::string& data, Mode mode = Mode::ECB, unsigned char iv[8] = {0})
        {
            if (data.empty())
                return "";

            std::vector<unsigned char> out_buf(data.length() + 8, 0);

            size_t out_len(0);
            if (!TripleDesEncode(key, (const unsigned char*)data.c_str(), data.length(), &out_buf[0], out_len, mode, iv)) {
                return "";
            }

            std::string rslt;
            if (!Base64Encode(&out_buf[0], out_len, rslt)) {
                return "";
            }

            return rslt;
        }
        /************************************************************************
         *����: 3DES���� - zeropadding
         *����: key: 8/16/24byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                src_buff: ��������ܲ�������
                src_len:  ��������ܲ�������
                out_buff: ������ܺ�����ָ��
                out_len:  ������ܺ����ݳ���
                mode:     ���ܷ�ʽ
                iv:       ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          �����Ƿ���ȷ����
         ************************************************************************/
        static bool TripleDesDecode(const unsigned char* key, const unsigned char* src_buff, size_t src_len, unsigned char* out_buff, size_t& out_len, Mode mode = Mode::ECB, unsigned char iv[8] = { 0 })
        {
            size_t key_num = check_key(key);
            if (key_num == 0)
                return false;

            unsigned char  ke1[8] = { 0 }, ke2[8] = { 0 }, ke3[8] = { 0 };
            DES_key_schedule ks1, ks2, ks3;

            switch (key_num)
            {
            case 1:
                memcpy(ke1, key, 8);
                memcpy(ke2, ke1, 8);
                memcpy(ke3, ke1, 8);
                break;
            case 2:
                memcpy(ke1, key, 8);
                memcpy(ke2, key + 8, 8);
                memcpy(ke3, ke1, 8);
                break;
            case 3:
                memcpy(ke1, key, 8);
                memcpy(ke2, key + 8, 8);
                memcpy(ke3, key + 16, 8);
                break;
            default:
                break;
            }

            DES_set_key_unchecked((const_DES_cblock*)ke1, &ks1);
            DES_set_key_unchecked((const_DES_cblock*)ke2, &ks2);
            DES_set_key_unchecked((const_DES_cblock*)ke3, &ks3);

            size_t block = 0;
            size_t mod_val = src_len % 8;
            if (mod_val == 0) {
                block = src_len / 8;
            }
            else {
                block = src_len / 8 + 1;
            }

            out_len = block * 8;

            unsigned char tmpData[8] = { 0 };
            switch (mode)
            {
            case Mode::CBC:
                DES_ede3_cbc_encrypt(src_buff, out_buff, (long)src_len, &ks1, &ks2, &ks3, (DES_cblock *)iv, DES_DECRYPT);
                break;
            case Mode::CFB:
                DES_ede3_cfb_encrypt(src_buff, out_buff, 8, (long)src_len, &ks1, &ks2, &ks3, (DES_cblock *)iv, DES_DECRYPT);
                break;
            case Mode::ECB:
            default:
                for (size_t i = 0; i < block; i++)
                {
                    if (i != block - 1)
                    {
                        memcpy(tmpData, src_buff + i * 8, 8);
                    }
                    else
                    {
                        memset(tmpData, 0, sizeof(tmpData));
                        memcpy(tmpData, src_buff + i * 8, (mod_val == 0) ? 8 : mod_val);
                    }
                    DES_ecb3_encrypt((const_DES_cblock*)tmpData, (DES_cblock *)(out_buff + i * 8), &ks1, &ks2, &ks3, DES_DECRYPT);
                }
                break;
            }
            return true;
        }
        /************************************************************************
         *����: base64���� + 3DES���� - zeropadding
         *Ŀ��: �ȶ����ݽ���base64����,�ٲ���3DES����
         *����: key:  8/16/24byte��Կ,��: 0xAA 0xBB 0xCC 0xDD 0xAA 0xBB 0xCC 0xDD
                data: ����������ַ�������
                mode: ���ܷ�ʽ
                iv:   ��CBC CFB����ģʽ���д�ֵ,����Ϊ8byte
          ���ؽ�������,ʧ���򷵻ؿ�����
         ************************************************************************/
        static std::string TripleDesDecode_Base64(const unsigned char* key, const std::string& data, Mode mode = Mode::ECB, unsigned char iv[8] = { 0 })
        {
            std::vector<unsigned char> out_buf(data.length() + 8, 0);
            size_t out_len(0);
            if (!Base64Decode(data, &out_buf[0], out_len)) {
                return "";
            }
            unsigned char rslt_buf[8192] = { 0 };
            size_t rslt_len(0);
            if (!TripleDesDecode(key, &out_buf[0], out_len, rslt_buf, rslt_len, mode, iv)) {
                return "";
            }
            return std::string((const char*)rslt_buf, rslt_len);
        }
#pragma endregion

    };
};
