/*
* Purpose: json字符串消息封包解包
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

    // 最小包长度
#define COMM_JSON_MIN_PACKAGE_LEN    28

    // 包头
    struct PackHeader {
        uint32_t command_;
    };

    // 包尾
    struct PackTail {
        uint32_t checksum_;
    };

    // 心跳包
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

    // 解包
    class DecodePackage : public Package
    {
        // 解包状态
        enum DecodeStatus
        {
            DecodeInit = 0, // 初始化未解析
            DecodeMsg,      // 当前包已解析
            DecodeError,    // 当前包非指定协议
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
        // 当前包是否是完整包
        bool is_entire()
        {
            if (m_status == DecodeInit)
                return false;

            return m_buff_len - m_offset >= m_pack_len;
        }

        // 获取当前包总长度
        size_t get_package_length() const {
            return m_pack_len;
        }

        // 获取当前包命令码
        uint32_t get_command() const {
            return m_head.command_;
        }

        // 获取当前包体长度
        size_t get_body_length() const {
            return m_body.length();
        }

        // 解包数据
        bool decode_msg()
        {
            if (m_offset == m_buff_len)
                return false;

            m_status = DecodeMsg;
            return vaild(m_offset);
        }

        // 获取包体
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

        // 是否还有数据未解包
        bool has_next()
        {
            if (m_status == DecodePackage::DecodeInit)
                return vaild(m_offset);

            return vaild(m_offset + m_pack_len);
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


                // 包长度获取
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

                // 消息解析
                Json::Value& msgVaalue = rootValue[mem_msg];
                if (msgVaalue.isNull() || !msgVaalue.isObject())
                    return true;

                // 命令码解析
                auto& cmd = msgVaalue[mem_cmd];
                if (cmd.isNull() || !cmd.isUInt())
                    return true;
                m_head.command_ = cmd.asUInt();

                // 包体解析
                auto& body = msgVaalue[mem_data];
                if (body.isNull() || !body.isString())
                    return true;
                m_body = body.asString();

                // 校验码解析
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
        std::string         m_buffer;   // 包总数据
        size_t              m_buff_len; // 包总数据长度
        size_t              m_pack_len; // 当前解的包数据总长度
        PackHeader          m_head;     // 当前包头
        std::string         m_body;     // 包体内容
        PackTail            m_tail;     // 当前包尾

        DecodeStatus        m_status;       // 当前解包状态
        size_t              m_offset;       // 当前包头所处漂移位
        bool                m_cur_isvaild;  // 当前解的包是否有效
        bool                m_cur_resolve_isvaild;// 当前包是否解析过
    };

    // 打包
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

        // 获取完整发送包数据
        const std::string& get_package() const {
            return m_send_msg;
        }

        // 获取完整发送包长度
        size_t get_length() const {
            return m_send_msg.length();
        }

    private:
        std::string     m_send_msg; // 发送数据
        PackHeader      m_head;
        PackTail        m_tail;
    };
}