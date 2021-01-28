/************************************************************************
* Purpose: 用于http通讯的json字符串消息封包解包
    Json格式:
    {
        "Code": 200,
        #"Msg": "",# // 无错误时为空
        "ServerTime": 2256612312,
        "Data": {}
    }
/************************************************************************/

#pragma once

#include <memory>
#include <algorithm>
#include <string>
#include <sstream>
#include <openssl/sha.h> 
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <json/writer.h>
#include <json/reader.h>
#include <json/json.h>
#include "../comm_str_os.hpp"
#include "../datetime_convert.hpp"

#ifdef _MSC_VER
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif // !WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#elif defined(__GNUC__)
# include <arpa/inet.h>
#endif

namespace NPHttpJsonPackage
{
#define HEAD_MSG_ContentType       "Content-Type"   // 内容类型,一般为application/json,get请求时无需该参数
#define HEAD_MSG_Nonce             "Nonce"          // 唯一码,公钥
#define HEAD_MSG_CurTime           "CurTime"        // 当前时间(unix时间戳),秒
#define HEAD_MSG_CheckSum          "CheckSum"       // 校验码(Secret(私钥)+Nonce+CurTime(字符串),SHA1 ,然后全小写)

#define SHA_DIGEST_LENGTH   20  // SHA散列值长度

#define RSP_Code            "code"          // 返回码,int,200:成功; 500:失败
#define RSP_Msg             "msg"           // Error Message,无错误时为空
#define RSP_ServerTime      "serverTime"    // 服务器时间
#define RSP_Data            "data"          // 数据

    // 包头
    struct PackHeader {
        std::string content_type_;  // 内容类型
        std::string nonce_;         // 唯一码
        std::string cur_time_;      // 当前时间
        std::string check_sum_;     // 校验码
    };

    class Package
    {
    public:
        typedef boost::beast::http::request<boost::beast::http::string_body>    request_type;
        typedef boost::beast::http::response<boost::beast::http::string_body>   response_type;

    protected:
        static std::string generate_checksum(const char* const secret, const std::string& nonce, const std::string& cur_time)
        {
            std::string src = secret + nonce + cur_time;
            unsigned char digest[SHA_DIGEST_LENGTH] = { 0 };
            SHA1((const unsigned char*)src.c_str(), src.length(), (unsigned char*)&digest);

            char mdString[SHA_DIGEST_LENGTH * 2 + 1] = { 0 };
            for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
                sprintf_s(&mdString[i * 2], 3, "%02x", (unsigned int)digest[i]);

            return mdString;
        }
    };

    // 解包
    class DecodePackage : public Package
    {
    public:
        DecodePackage(const request_type& request, const char* const secret)
            : m_method(request.method())
        {
            if(m_method != boost::beast::http::verb::get
                && m_method != boost::beast::http::verb::post
                /*&& m_method != boost::beast::http::verb::head*/)
            {
                m_cur_isvaild = false;
                return;
            }

            if (request.target().empty() ||
                request.target()[0] != '/' ||
                request.target().find("..") != boost::beast::string_view::npos)
            {
                m_cur_isvaild = false;
                return;
            }
            auto split_pos = request.target().find_first_of('?');
            if(split_pos != boost::beast::string_view::npos)
            {
                m_path = request.target().substr(0, split_pos).to_string();
                std::transform(m_path.begin(), m_path.end(), m_path.begin(), ::tolower);
                m_params = request.target().substr(split_pos + 1).to_string();
                //std::transform(m_params.begin(), m_params.end(), m_params.begin(), ::tolower);
            }
            else
            {
                m_path = request.target().to_string();
                std::transform(m_path.begin(), m_path.end(), m_path.begin(), ::tolower);
            }
            
            //auto content_type_iter = request.find(HEAD_MSG_ContentType);
            //m_cur_isvaild = content_type_iter != request.end();
            //if (m_cur_isvaild)
                m_head.content_type_ = /*content_type_iter->value().to_string()*/"application/json";
            //else if(m_method != boost::beast::http::verb::get)
            //    return;

            //auto nonce_iter = request.find(HEAD_MSG_Nonce);
            //m_cur_isvaild = nonce_iter != request.end();
            //if (!m_cur_isvaild)
            //    return;
            //m_head.nonce_ = nonce_iter->value().to_string();

            /*auto cur_time_iter = request.find(HEAD_MSG_CurTime);
            m_cur_isvaild = cur_time_iter != request.end();
            if (!m_cur_isvaild)
                return;
            m_head.cur_time_ = cur_time_iter->value().to_string();

            auto check_sum_iter = request.find(HEAD_MSG_CheckSum);
            m_cur_isvaild = check_sum_iter != request.end();
            if (!m_cur_isvaild)
                return;*/
            //m_head.check_sum_ = check_sum_iter->value().to_string();
        /*    std::transform(m_head.check_sum_.begin(), m_head.check_sum_.end(), m_head.check_sum_.begin(), ::tolower);

            m_cur_isvaild = generate_checksum(secret, m_head.nonce_, m_head.cur_time_) == m_head.check_sum_;
            if (!m_cur_isvaild)
                return;*/

            m_cur_isvaild = true;
            m_body = request.body();
        }

    public:
        // 当前包是否是完整包
        bool is_entire() const {
            return m_cur_isvaild;
        }
        // 获取path路径,均为小写
        const std::string& get_path() const {
            return m_path;
        }
        // 获取params参数,均为小写
        const std::string& get_params() const {
            return m_params;
        }
        // 获取http请求方法
        const boost::beast::http::verb& get_method() const {
            return m_method;
        }
        // 获取内容类型,一般为application/json
        const std::string& get_content_type() const {
            return m_head.content_type_;
        }
        // 获取唯一码
        const std::string& get_nonce() const {
            return m_head.nonce_;
        }
        // 获取当前时间
        const std::string& get_cur_time() const {
            return m_head.cur_time_;
        }
        // 获取校验码
        const std::string& get_check_sum() const {
            return m_head.check_sum_;
        }
        // 获取包头
        const PackHeader& get_head() const {
            return m_head;
        }
        // 获取包体
        const std::string& get_body() const {
            return m_body;
        }

    private:
        PackHeader          m_head;                 // 当前包头
        std::string         m_body;                 // 包体内容
        std::string         m_path;                 // path路径(http://127.0.0.1:22/path/)
        std::string         m_params;               // params参数(?之后的参数)
        boost::beast::http::verb    m_method;       // http请求方法

        bool                m_cur_isvaild;          // 当前解的包是否有效
    };

    // 打包
    class EncodePackage : public Package
    {
    public:
        EncodePackage(const request_type& request, const char* const secret)
            : m_request(request)
            , m_secret(secret)
        {
        }

        ~EncodePackage()
        {
        }

        response_type bad_request(const std::string& why)
        {
            response_type res{ boost::beast::http::status::bad_request, m_request.version() };
            rsp_set(res, false, why);
            return std::move(res);
        }

        response_type not_found(const std::string& path)
        {
            response_type res{ boost::beast::http::status::not_found, m_request.version() };
            rsp_set(res, false, "The target '" + path + "' was not support!");
            return std::move(res);
        }

        response_type server_error(const std::string& why) {
            response_type res{ boost::beast::http::status::internal_server_error, m_request.version() };
            rsp_set(res, false, why);
            return std::move(res);
        }

//         boost::beast::http::response<boost::beast::http::empty_body> request_head(size_t size) {
//             boost::beast::http::response<boost::beast::http::empty_body> res{ boost::beast::http::status::ok, m_request.version() };
//             res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
//             res.set(boost::beast::http::field::content_type, "application/json");
//             res.content_length(size);
//             res.keep_alive(m_request.keep_alive());
//             return std::move(res);
//         }

        template<typename BodyType>
        response_type response(bool is_true, const BodyType& body)
        {
            response_type res{ boost::beast::http::status::ok, m_request.version() };
            rsp_set(res, is_true, body);
            return std::move(res);
        }

    private:
        // is_true:为true时body表示发送数据; 为false时表示错误内容
        template<typename BodyType>
        std::string rsp_body(bool is_true, const BodyType& body) {
            Json::Value body_json;
            if(is_true) {
                body_json[RSP_Code] = 200;
                body_json[RSP_Data] = body;
            }
            else {
                body_json[RSP_Code] = 400;
                body_json[RSP_Msg] = body;
            }
            body_json[RSP_ServerTime] = BTool::DateTimeConvert::GetCurrentSystemTime(BTool::DateTimeConvert::DTS_YMDHMS).to_time_t();
            Json::StreamWriterBuilder  builder;
            builder["commentStyle"] = "None";
            builder["indentation"] = "";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            std::stringstream os;
            writer->write(body_json, &os);
            return os.str();
        }

        template<typename BodyType>
        void rsp_set(response_type& res, bool is_true, const BodyType& body) {
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "application/json");
            res.keep_alive(m_request.keep_alive());

//             res.set(HEAD_MSG_ContentType, "application/json");
            res.set(HEAD_MSG_Nonce, m_request[HEAD_MSG_Nonce]);
            std::string cur_time = std::to_string(BTool::DateTimeConvert::GetCurrentSystemTime(BTool::DateTimeConvert::DTS_YMDHMS).to_time_t());
            res.set(HEAD_MSG_CurTime, cur_time);
            std::string send_body = rsp_body(is_true, body);
            res.set(HEAD_MSG_CheckSum, generate_checksum(m_secret, m_request[HEAD_MSG_Nonce].to_string(), cur_time));

            res.content_length(send_body.length());
            res.body() = send_body;
            res.prepare_payload();
        }

    private:
        const request_type&     m_request;
        const char* const       m_secret;
    };
}