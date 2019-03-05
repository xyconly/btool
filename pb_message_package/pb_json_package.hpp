/*
* Purpose: json�ַ�����Ϣ������
*/

#pragma once

#include <memory>
#include <algorithm>
#include <string>
#include <sstream>
#include <json/writer.h>
#include <json/reader.h>
#include <json/json.h>

#ifdef _MSC_VER
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif // !WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#elif defined(__GNUC__)
# include <arpa/inet.h>
#endif

namespace NPPbJsonPackage
{
#define COMM_JSON_MSG       "msg"
#define COMM_JSON_COMMAND   "cmd"
#define COMM_JSON_LEN       "len"
#define COMM_JSON_DATA      "data"
#define COMM_JSON_CHK       "chk"

    // ��С������
#define COMM_JSON_MIN_PACKAGE_LEN    28

    // ��ͷ
    struct PackHeader {
        uint32_t command_;
    };

    // ��β
    struct PackTail {
        uint32_t checksum_;
    };

    // ������
    struct HeartBeat
    {
        PackHeader	Header;
        PackTail	Tail;
    };

    class Package
    {
    protected:
        static uint32_t generate_checksum(const char* send_msg, size_t len)
        {
            uint32_t cks(0);
            for (size_t idx = 0; idx < len; idx++)
                cks += (uint32_t)send_msg[idx];
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
            DecodeError,    // ��ǰ����ָ��Э��
        };

    public:
        DecodePackage(std::string&& recv_msg)
            : m_buffer(std::move(recv_msg))
            , m_offset(0)
            , m_buff_len(m_buffer.length())
        {
            init_data();
        }

    public:
        // ��ǰ���Ƿ���������
        bool is_entire()
        {
            if (m_status == DecodeInit)
                return false;

            return m_buff_len - m_offset >= m_pack_len;
        }

        // ��ȡ��ǰ���ܳ���
        size_t get_package_length() const {
            return m_pack_len;
        }

        // ��ȡ��ǰ��������
        uint32_t get_command() const {
            return m_head.command_;
        }

        // ��ȡ��ǰ���峤��
        size_t get_body_length() const {
            return m_body.length();
        }

        // �������
        bool decode_msg()
        {
            if (m_offset == m_buff_len)
                return false;

            m_status = DecodeMsg;
            return vaild(m_offset);
        }

        // ��ȡ����
        const std::string& get_body()
        {
            return m_body;
        }

        bool check_sum()
        {
            if (m_status == DecodeInit || !m_cur_isvaild)
                return false;

            return generate_checksum(get_body().c_str(), get_body_length()) == m_tail.checksum_;
        }

        // �Ƿ�������δ���
        bool has_next()
        {
            if (m_status == DecodePackage::DecodeInit)
                return vaild(m_offset);

            return vaild(m_offset + m_pack_len);
        }

        // ��ʼ�����һ��
        void decode_next()
        {
            m_offset += m_pack_len;
            init_data();
        }

        // ��ȡ�Ѵ�����ϰ�����
        size_t get_deal_len()
        {
            return m_offset;
        }

        // ��ȡʣ�೤��,������ǰ�������
        size_t get_res_len()
        {
            return m_buff_len - m_offset;
        }

    private:
        // �Ƿ���Ч
        bool vaild(size_t offset) {
            if (m_cur_resolve_isvaild)
                return true;

            m_cur_isvaild = false;
            m_cur_resolve_isvaild = true;

            try {
                std::string cur_package = m_buffer.substr(offset);

                Json::Value rootValue;
                Json::CharReaderBuilder b;
                Json::CharReader* reader(b.newCharReader());
                JSONCPP_STRING errs;
                bool ok = reader->parse(cur_package.c_str(), cur_package.c_str() + cur_package.length(), &rootValue, &errs);
                if (!ok || errs.size() > 0)
                    return true;

                std::string mem_msg, mem_chk, mem_cmd, mem_data;

                auto msgs = rootValue.getMemberNames();
                if (msgs.size() != 1)
                    return true;

                auto str0 = msgs[0];
                std::transform(str0.begin(), str0.end(), str0.begin(), ::tolower);
                if (str0 != COMM_JSON_MSG)
                    return true;
                mem_msg = msgs[0];


                for (auto iter = rootValue.begin(); iter != rootValue.end(); iter++)
                {
                    auto mems = iter->getMemberNames();
                    for (auto& item : mems)
                    {
                        auto str1 = item;
                        std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
                        if (str1 == COMM_JSON_COMMAND)
                            mem_cmd = item;
                        else if (str1 == COMM_JSON_DATA)
                            mem_data = item;
                        else if (str1 == COMM_JSON_CHK)
                            mem_chk = item;
                    }
                }

                if (mem_msg.empty() || mem_cmd.empty()
                    || mem_data.empty() || mem_chk.empty())
                    return true;


                // �����Ȼ�ȡ
                size_t startpos = cur_package.find(mem_msg);
                if (startpos == std::string::npos)
                    return true;

                startpos = cur_package.rfind("{", startpos);
                if (startpos == std::string::npos)
                    return true;

                m_offset += startpos;

                size_t endpos = cur_package.find(mem_msg, startpos + COMM_JSON_MIN_PACKAGE_LEN + 1);
                endpos = cur_package.rfind("}", endpos);
                m_pack_len = endpos - startpos + 1;

                // ��Ϣ����
                Json::Value& msgVaalue = rootValue[mem_msg];
                if (msgVaalue.isNull() || !msgVaalue.isObject())
                    return true;

                // ���������
                auto& cmd = msgVaalue[mem_cmd];
                if (cmd.isNull() || !cmd.isUInt())
                    return true;
                m_head.command_ = cmd.asUInt();

                // �������
                auto& body = msgVaalue[mem_data];
                if (body.isNull() || !body.isString())
                    return true;
                m_body = body.asString();

                // У�������
                auto& tail = msgVaalue[mem_chk];
                if (tail.isNull() || !tail.isUInt())
                    return true;
                m_tail.checksum_ = tail.asUInt();
                m_cur_isvaild = true;
            }
            catch (...) {
                return true;
            }
            return true;
        }

        void init_data()
        {
            m_status = DecodePackage::DecodeInit;
            m_body.clear();
            m_head = PackHeader{ 0 };
            m_tail = PackTail{ 0 };
            m_cur_isvaild = false;
            m_cur_resolve_isvaild = false;
            m_pack_len = 0;
        }

    private:
        std::string         m_buffer;   // ��������
        size_t              m_buff_len; // �������ݳ���
        size_t              m_pack_len; // ��ǰ��İ������ܳ���
        PackHeader          m_head;     // ��ǰ��ͷ
        std::string         m_body;     // ��������
        PackTail            m_tail;     // ��ǰ��β

        DecodeStatus        m_status;       // ��ǰ���״̬
        size_t              m_offset;       // ��ǰ��ͷ����Ư��λ
        bool                m_cur_isvaild;  // ��ǰ��İ��Ƿ���Ч
        bool                m_cur_resolve_isvaild;// ��ǰ���Ƿ������
    };

    // ���
    class EncodePackage : public Package
    {
    public:
        EncodePackage(uint32_t command, const char* send_msg, size_t len, size_t pb_len)
            : m_head({ command })
            , m_tail({ generate_checksum(send_msg, len) })
        {
            Json::Value body;
            body[COMM_JSON_COMMAND] = command;
            body[COMM_JSON_DATA] = send_msg;
            body[COMM_JSON_LEN] = pb_len;
            body[COMM_JSON_CHK] = m_tail.checksum_;

            Json::Value root;
            root[COMM_JSON_MSG] = body;

            Json::StreamWriterBuilder  builder;
            builder["commentStyle"] = "None";
            builder["indentation"] = "";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            std::stringstream os;
            writer->write(root, &os);
            m_send_msg = os.str();
        }

        ~EncodePackage()
        {
        }

        // ��ȡ�������Ͱ�����
        const std::string& get_package() const {
            return m_send_msg;
        }

        // ��ȡ�������Ͱ�����
        size_t get_length() const {
            return m_send_msg.length();
        }

    private:
        std::string     m_send_msg; // ��������
        PackHeader      m_head;
        PackTail        m_tail;
    };
}