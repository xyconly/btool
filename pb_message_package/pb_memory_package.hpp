/*
* Purpose: �ڴ���Ϣ������
*/

#pragma once

#include <memory>

#ifdef _MSC_VER
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif // !WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#elif defined(__GNUC__)
# include <arpa/inet.h>
#endif

namespace NPPbMemoryPackage
{
#pragma pack(push)
#pragma pack(1) 
    // ��ͷ
    struct PackHeader {
        uint16_t command_;
        uint32_t body_length_;
    };

    // ��β
    struct PackTail {
        uint16_t checksum_;
    };

    // ������
    struct HeartBeat
    {
        PackHeader	Header;
        PackTail	Tail;
    };
#pragma pack(pop)

    class Package
    {
    protected:
        static uint16_t generate_checksum(const char* send_msg, size_t len)
        {
            uint32_t cks(0);
            for (size_t idx = 0; idx < len; idx++)
                cks += (uint8_t)send_msg[idx];
            return (cks % 256);
        }
    };

    // ���
    class DecodePackage : public Package
    {
        // ���״̬
        enum DecodeStatus
        {
            DecodeInit = 0, // ��ʼ��δ����
            DecodeMsg,      // ��ǰ���ѽ���
        };

    public:
        DecodePackage(const char* recv_msg, uint32_t len)
            : m_buffer(recv_msg)
            , m_buff_len(len)
            , m_offset(0)
        {
            init_data();
        }

    public:
        // ��ǰ���Ƿ���������
        bool is_entire() const
        {
            if (m_status == DecodeInit)
                return false;

            return vaild(m_pack_len);
        }

        // ��ȡ��ǰ���ܳ���
        uint32_t get_package_length() const {
            return m_pack_len;
        }

        // ��ȡ��ǰ��������
        uint32_t get_command() const {
            return m_head.command_;
        }

        // ��ȡ��ǰ���峤��
        uint32_t get_body_length() const {
            return m_head.body_length_;
        }

        // �������
        bool decode_msg()
        {
            if (!vaild(sizeof(PackHeader) + sizeof(PackTail)))
                return false;

            PackHeader tmp_head = { 0 };
            memcpy(&tmp_head, m_buffer + m_offset, sizeof(PackHeader));

            m_head.command_ = ntohs(tmp_head.command_);
            m_head.body_length_ = ntohl(tmp_head.body_length_);

            m_pack_len = sizeof(PackHeader) + m_head.body_length_ + sizeof(PackTail);

            if (!vaild(m_pack_len))
                return false;

            memcpy(&m_tail, m_buffer + m_offset + sizeof(PackHeader) + m_head.body_length_, sizeof(PackTail));
            m_tail.checksum_ = ntohs(m_tail.checksum_);
            m_status = DecodeMsg;
            return true;
        }

        // ��ȡ����
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

            return generate_checksum(get_body(), get_body_length()) == m_tail.checksum_;
        }

        // �Ƿ�������δ���
        bool has_next()
        {
            if (m_status == DecodePackage::DecodeInit)
                return vaild(sizeof(PackHeader) + sizeof(PackTail));

            return vaild(m_pack_len + 1);
        }

        // ��ʼ�����һ��
        void decode_next()
        {
            m_offset += m_pack_len;
            init_data();
        }

        // ��ȡ�Ѵ�����ϰ�����
        uint32_t get_deal_len()
        {
            return m_offset;
        }

        // ��ȡʣ�೤��,������ǰ�������
        uint32_t get_res_len()
        {
            return m_buff_len - m_offset;
        }

    private:
        // �Ƿ���Ч
        bool vaild(uint32_t read_length) const {
            return m_offset + read_length <= m_buff_len;
        }

        void init_data()
        {
            m_status = DecodePackage::DecodeInit;
            m_pack_len = 0;
            m_body_len = 0;
            m_head = PackHeader{ 0,0 };
            m_tail = PackTail{ 0 };
        }

    private:
        const char*     m_buffer;   // ������
        uint32_t        m_buff_len; // �����ݳ���
        uint32_t        m_offset;   // ��ͷ����Ư��λ

        DecodeStatus    m_status;   // ��ǰ���״̬
        uint32_t        m_pack_len; // ��ǰ���ܳ���
        uint32_t        m_body_len; // ��ǰ���峤��
        PackHeader      m_head;     // ��ǰ��ͷ
        PackTail        m_tail;     // ��ǰ��β
    };

    // ���
    class EncodePackage : public Package
    {
    public:
        EncodePackage(uint16_t command, const char* send_msg, size_t len)
            : m_head({ htons(command),htonl((uint32_t)len) })
            , m_tail({ htons(generate_checksum(send_msg, len)) })
            , m_send_len(sizeof(PackHeader) + len + sizeof(PackTail))
        {
            m_send_msg = new char[m_send_len];

            memcpy(m_send_msg, (const char*)(&m_head), sizeof(PackHeader));

            size_t offset(sizeof(PackHeader));
            memcpy(m_send_msg + offset, send_msg, len);

            offset += len;
            memcpy(m_send_msg + offset, (const char*)(&m_tail), sizeof(PackTail));
        }

        ~EncodePackage()
        {
            delete[] m_send_msg;
        }

        // ��ȡ�������Ͱ�����
        const char* get_package() const {
            return m_send_msg;
        }

        // ��ȡ�������Ͱ�����
        size_t get_length() const {
            return m_send_len;
        }

    private:
        char*           m_send_msg; // ��������
        size_t          m_send_len;
        PackHeader      m_head;
        PackTail        m_tail;
    };
}