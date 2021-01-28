/*
* Purpose: 内存消息封包解包
*/

#pragma once

#include <string>
#include <memory>
#include "../comm_str_os.hpp"
#include "../value_convert.hpp"

#ifdef _MSC_VER
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif // !WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#elif defined(__GNUC__)
# include <arpa/inet.h>
#endif

namespace NPPbWebsocketPackage
{
    // 包头
    struct PackHeader {
        char command_[4];
        char pb_body_length_[8];
        char checksum_[4];
        char body_length_[8];
    };

    // 心跳包
    struct HeartBeat
    {
        PackHeader	Header;
    };

    class Package
    {
    protected:
        static uint32_t generate_checksum(const char* send_msg, size_t len)
        {
            uint32_t cks(0);
            for (size_t idx = 0; idx < len; idx++)
                cks += send_msg[idx];
            cks = cks % 256;
            return cks;
        }
    };

    // 解包
    class DecodePackage : public Package
    {
        // 解包状态
        enum DecodeStatus
        {
            DecodeInit = 0, // 初始化未解析
            DecodeMsg,      // 当前包已解析
        };

    public:
        DecodePackage(const char* recv_msg, size_t len)
            : m_buffer(recv_msg)
            , m_buff_len(len)
            , m_offset(0)
        {
            init_data();
        }

    public:
        // 当前包是否是完整包
        bool is_entire() const
        {
            if (m_status == DecodeInit)
                return false;

            return vaild(m_pack_len);
        }

        // 获取当前包总长度
        size_t get_package_length() const {
            return m_pack_len;
        }

        // 获取当前包命令码
        size_t get_command() const {
            size_t cmd(0);
            BTool::ValueConvert::SafeConvert(m_head.command_, sizeof(m_head.command_), cmd);
            return cmd;
        }

        // 获取当前包体长度
        size_t get_body_length() const {
            size_t len(0);
            BTool::ValueConvert::SafeConvert(m_head.body_length_, sizeof(m_head.body_length_), len);
            return len;
        }

        // 获取当前包校验码
        size_t get_checksum() const {
            size_t chk(0);
            BTool::ValueConvert::SafeConvert(m_head.checksum_, sizeof(m_head.checksum_), chk);
            return chk;
        }

        // 解包数据
        bool decode_msg()
        {
            if (!vaild(sizeof(PackHeader)))
                return false;

            memcpy(&m_head, m_buffer + m_offset, sizeof(PackHeader));

            size_t body_len(0);
            if(!BTool::ValueConvert::SafeConvert(m_head.body_length_, sizeof(m_head.body_length_), body_len))
                return false;

            m_pack_len = sizeof(PackHeader) + body_len;

            if (!vaild(m_pack_len))
                return false;

            m_status = DecodeMsg;
            return true;
        }

        // 获取包体
        const char* get_body()
        {
            if (m_status == DecodeInit)
                return nullptr;

            return m_buffer + m_offset + sizeof(PackHeader);
        }

        bool check_sum()
        {
            if (m_status == DecodeInit)
                return false;

            uint32_t chk(0);
            if (BTool::ValueConvert::SafeConvert(m_head.checksum_, sizeof(m_head.checksum_), chk))
                return generate_checksum(get_body(), get_body_length()) == chk;

            return false;
        }

        // 是否还有数据未解包
        bool has_next()
        {
            if (m_status == DecodePackage::DecodeInit)
                return vaild(sizeof(PackHeader));

            return vaild(m_pack_len + 1);
        }

        // 开始解包下一个
        void decode_next()
        {
            m_offset += m_pack_len;
            init_data();
        }

        // 获取已处理完毕包长度
        size_t get_deal_len()
        {
            return m_offset;
        }

        // 获取剩余长度,包含当前解包长度
        size_t get_res_len()
        {
            return m_buff_len - m_offset;
        }

    private:
        // 是否有效
        bool vaild(size_t read_length) const {
            return m_buffer && m_offset + read_length <= m_buff_len;
        }

        void init_data()
        {
            m_status = DecodePackage::DecodeInit;
            m_pack_len = 0;
            m_body_len = 0;
            m_head = PackHeader{ 0 };
        }

    private:
        const char*     m_buffer;   // 包数据
        size_t          m_buff_len; // 当前包数据长度
        size_t          m_offset;   // 当前包头所处漂移位

        DecodeStatus    m_status;   // 当前解包状态
        size_t          m_pack_len; // 当前包总长度
        size_t          m_body_len; // 当前包体长度
        PackHeader      m_head;     // 当前包头
    };

    // 打包
    class EncodePackage : public Package
    {
    public:
        EncodePackage(uint32_t command, const char* send_msg, size_t len, size_t pb_len)
            : m_head({ 0 })
            , m_send_len(sizeof(PackHeader) + len)
        {
            char len_tmp[9] = { 0 };
            sprintf_s(len_tmp, 5, "%04d", command);
            memcpy(m_head.command_, len_tmp, sizeof(m_head.command_));

            memset(len_tmp, 0, 9);
            sprintf_s(len_tmp, 9, "%08d", (int)pb_len);
            memcpy(m_head.pb_body_length_, len_tmp, sizeof(m_head.pb_body_length_));

            memset(len_tmp, 0, 9);
            sprintf_s(len_tmp, 5, "%04d", generate_checksum(send_msg, len));
            memcpy(m_head.checksum_, len_tmp, sizeof(m_head.checksum_));

            memset(len_tmp, 0, 9);
            sprintf_s(len_tmp, 9, "%08d", (int)len);
            memcpy(m_head.body_length_, len_tmp, sizeof(m_head.body_length_));
            
            m_send_msg = new char[m_send_len];

            memcpy(m_send_msg, (const char*)(&m_head), sizeof(PackHeader));

            memcpy(m_send_msg + sizeof(PackHeader), send_msg, len);
        }

        ~EncodePackage()
        {
            delete[] m_send_msg;
        }

        // 获取完整发送包数据
        const char* get_package() const {
            return m_send_msg;
        }

        // 获取完整发送包长度
        size_t get_length() const {
            return m_send_len;
        }

    private:
        char*           m_send_msg; // 发送数据
        size_t          m_send_len;
        PackHeader      m_head;
    };
}