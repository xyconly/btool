/******************************************************************************
File name:  http_session.hpp
Author:	    AChar
Purpose:    http连接类
Note:       客户端可直接使用HttpClientSession,调用HttpClientNetCallBack回调

示例代码:
        class TestHttpClient : public BTool::BoostNet::HttpClientNetCallBack
        {
            BTool::BoostNet::HttpClientSession      session_type;
            typedef std::shared_ptr<session_type>   session_ptr_type;
        public:
            TestHttpClient()
            {
                m_session = std::make_shared<session_type>(get_io_context());
                m_session->register_cbk(this);
                m_session->connect(ip, port);
            }

        protected:
            // 开启连接回调
            virtual void on_open_cbk(SessionID session_id) override;

            // 关闭连接回调
            virtual void on_close_cbk(SessionID session_id) override;

            // 读取消息回调,此时read_msg_type为boost::beast::http::response<boost::beast::http::string_body>
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) override;

            // 写入消息回调,此时send_msg_type为boost::beast::http::request<boost::beast::http::string_body>
            virtual void on_write_cbk(SessionID session_id, const send_msg_type& send_msg) override;

        private:
            session_ptr_type            m_session;
        }

备注:
        也可直接自定义发送及返回消息类型, 如
            using SelfHttpClientNetCallBack = HttpNetCallBack<false, boost::beast::http::file_body, boost::beast::http::string_body>;
            using SelfHttpClientSession = HttpSession<false, boost::beast::http::file_body, boost::beast::http::string_body>
*****************************************************************************/

#pragma once

#include <tuple>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include "http_net_callback.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // Http连接对象
        template<bool isRequest, class ReadType, class WriteType = ReadType, class Fields = boost::beast::http::fields>
        class HttpSession : public std::enable_shared_from_this<HttpSession<isRequest, ReadType, WriteType, Fields>>
        {
        public:
            typedef boost::asio::ip::tcp::socket                                socket_type;
            typedef boost::asio::io_service                                     ios_type;
            typedef boost::asio::strand<boost::asio::io_context::executor_type> strand_type;
            typedef boost::beast::flat_buffer                                   read_buffer_type;

            typedef HttpSession<isRequest, ReadType, WriteType, Fields>         SessionType;
            typedef HttpNetCallBack<isRequest, ReadType, WriteType, Fields>     callback_type;
            typedef typename callback_type::read_msg_type                       read_msg_type;
            typedef typename callback_type::send_msg_type                       send_msg_type;
            typedef typename callback_type::SessionID                           SessionID;

        public:
            // Http连接对象
            // ios: io读写动力服务
            // max_rbuffer_size:单次读取最大缓冲区大小
            HttpSession(ios_type& ios)
                : m_resolver(ios)
                , m_socket(ios)
                , m_started_flag(false)
                , m_stop_flag(true)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
                , m_strand(m_socket.get_executor())
            {
            }

            ~HttpSession() {
                m_handler = nullptr;
                close();
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // 获得socket
            socket_type& get_socket() {
                return m_socket;
            }

            // 获得io_service
            ios_type& get_io_context() {
                return m_socket.get_io_context();
            }

            // 是否已开启
            bool is_open() const {
                return  m_started_flag.load() && m_socket.is_open();
            }

            // 获取连接ID
            SessionID get_session_id() const {
                return m_session_id;
            }

            // 获取连接者IP
            const std::string& get_ip() const {
                return m_connect_ip;
            }

            // 获取连接者port
            unsigned short get_port() const {
                return m_connect_port;
            }

            // 客户端开启连接,同时开启读取
            void connect(const char* ip, unsigned short port)
            {
                m_connect_ip = ip;
                m_connect_port = port;

                m_resolver.async_resolve(ip, std::to_string(port),
                    std::bind(&SessionType::handle_resolve, SessionType::shared_from_this()
                        , std::placeholders::_1, std::placeholders::_2));
            }

            // 客户端开启连接,同时开启读取
            void reconnect()
            {
                m_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(m_connect_ip), m_connect_port)
                    , std::bind(&SessionType::handle_connect, SessionType::shared_from_this(), std::placeholders::_1));
            }

            // 服务端开启连接,同时开启读取
            void start()
            {
                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                boost::system::error_code ec;
                m_connect_ip = m_socket.remote_endpoint(ec).address().to_v4().to_string();
                m_connect_port = m_socket.remote_endpoint(ec).port();

                // 服务端解析请求信息,当服务端开启时先开启读端口
                if(isRequest)
                    read(false);

                m_stop_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }
            }

            // 同步关闭
            void shutdown()
            {
                bool expected = false;
                if (!m_stop_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                m_read_buf.consume(m_read_buf.size());
                close();
                m_started_flag.exchange(false);
            }

            // 获取待发送post请求信息
            // target: 路径,包含Query
            // version: https协议版本
            send_msg_type get_send_post_request(const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                return get_send_request(boost::beast::http::verb::post, target, content_type, version);
            }
            // 获取待发送get请求信息
            // target: 路径,包含Query
            // version: https协议版本
            send_msg_type get_send_get_request(const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                return get_send_request(boost::beast::http::verb::get, target, content_type, version);
            }

            // 异步写
            bool async_write(send_msg_type&& msg)
            {
                m_send_msg = std::forward<send_msg_type>(msg);
                m_send_msg.set(boost::beast::http::field::host, m_connect_ip);

                boost::beast::http::async_write(
                    m_socket, m_send_msg
                    , boost::asio::bind_executor(
                        m_strand,
                        std::bind(
                            &SessionType::handle_write,
                            SessionType::shared_from_this(),
                            std::placeholders::_1,
                            std::placeholders::_2,
                            m_send_msg.need_eof())));

                return true;
            }

            // 使用ip+port同步发送,仅用于客户端,非线程安全
            std::tuple<bool, read_msg_type> sync_write(const char* ip, unsigned short port, send_msg_type&& send_msg)
            {
                read_msg_type read_msg = {};
                try
                {
                    read_msg.body() = "connect error";
                    // 连接
                    auto const results = m_resolver.resolve(ip, std::to_string(port));
                    boost::asio::connect(m_socket, results.begin(), results.end());

                    read_msg.body() = "write error";
                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, ip);
                    boost::beast::http::write(m_socket, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // 读取应答
                    read_buffer_type read_buf;
                    auto read_len = boost::beast::http::read(m_socket, read_buf, read_msg);
                }
                catch (std::exception const&)
                {
                    close();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                return std::forward_as_tuple(true, std::move(read_msg));
            }
            // 使用ip+port同步发送,仅用于客户端,非线程安全
            std::tuple<bool, std::vector<read_msg_type>> sync_write_end_of_stream(const char* ip, unsigned short port, send_msg_type&& send_msg)
            {
                std::vector<read_msg_type> rslt;
                try
                {
                    // 连接
                    auto const results = m_resolver.resolve(ip, std::to_string(port));
                    boost::asio::connect(m_socket, results.begin(), results.end());

                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, ip);
                    boost::beast::http::write(m_socket, std::forward<send_msg_type>(send_msg));

                    // 读取应答
                    boost::system::error_code ec;
                    for (;;)
                    {
                        read_msg_type read_msg = {};
                        read_buffer_type read_buf;
                        auto read_len = boost::beast::http::read(m_socket, read_buf, read_msg, ec);
                        if (ec == boost::beast::http::error::end_of_stream)
                            break;
                        rslt.push_back(std::move(read_msg));
                        if (ec)
                            return std::forward_as_tuple(false, std::move(rslt));
                    }
                }
                catch (std::exception const&)
                {
                    close();
                    return std::forward_as_tuple(false, std::move(rslt));
                }

                return std::forward_as_tuple(true, std::move(rslt));
            }
            // 使用域名同步发送,仅用于客户端,非线程安全
            std::tuple<bool, read_msg_type> sync_write(const char* host, send_msg_type&& send_msg)
            {
                read_msg_type read_msg = {};
                try
                {
                    read_msg.body() = "connect error";
                    // 连接
                    boost::asio::ip::tcp::resolver::query query(host, "http");
                    boost::asio::connect(m_socket, m_resolver.resolve(query));

                    read_msg.body() = "write error";
                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(m_socket, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // 读取应答
                    read_buffer_type read_buf;
                    auto read_len = boost::beast::http::read(m_socket, read_buf, read_msg);
                }
                catch (std::exception const&)
                {
                    close();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                return std::forward_as_tuple(true, std::move(read_msg));
            }
            // 使用域名同步发送,仅用于客户端,非线程安全
            std::tuple<bool, std::vector<read_msg_type>> sync_write_end_of_stream(const char* host, send_msg_type&& send_msg)
            {
                std::vector<read_msg_type> rslt;
                try
                {
                    // 连接
                    boost::asio::ip::tcp::resolver::query query(host, "http");
                    boost::asio::connect(m_socket, m_resolver.resolve(query));

                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(m_socket, std::forward<send_msg_type>(send_msg));

                    // 读取应答
                    boost::system::error_code ec;
                    for (;;)
                    {
                        read_msg_type read_msg = {};
                        read_buffer_type read_buf;
                        auto read_len = boost::beast::http::read(m_socket, read_buf, read_msg, ec);
                        if (ec == boost::beast::http::error::end_of_stream)
                            break;
                        rslt.push_back(std::move(read_msg));
                        if (ec)
                            return std::forward_as_tuple(false, std::move(rslt));
                    }
                }
                catch (std::exception const&)
                {
                    close();
                    return std::forward_as_tuple(false, std::move(rslt));
                }

                return std::forward_as_tuple(true, std::move(rslt));
            }

        protected:
            static SessionID GetNextSessionID()
            {
                static std::atomic<SessionID> next_session_id(callback_type::InvalidSessionID);
                return ++next_session_id;
            }

        private:
            send_msg_type get_send_request(boost::beast::http::verb verb, const std::string& target, const std::string& content_type, int version)
            {
                send_msg_type send_req;
                send_req.version(version);
                send_req.method(verb);
                send_req.target(target);
                send_req.keep_alive(false);
                send_req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
                send_req.set(boost::beast::http::field::content_type, content_type);
                return send_req;
            }

            void close()
            {
                boost::system::error_code ignored_ec;
                get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                get_socket().close(ignored_ec);

                if (m_handler) {
                    m_handler->on_close_cbk(m_session_id);
                }
            }

            // 异步读
            // close: 读取完毕后是否关闭
            void read(bool close)
            {
                try {
                    m_read_msg = {};

                    boost::beast::http::async_read(m_socket, m_read_buf, m_read_msg,
                        boost::asio::bind_executor(
                            m_strand,
                            std::bind(
                                &SessionType::handle_read,
                                SessionType::shared_from_this(),
                                std::placeholders::_1,
                                std::placeholders::_2, 
                                close)));
                }
                catch (...) {
                    shutdown();
                }
            }

            // 解析IP回调
            void handle_resolve(const boost::system::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec)
                    return close();

                boost::asio::async_connect(m_socket, results.begin(), results.end()
                    , std::bind(&SessionType::handle_connect, SessionType::shared_from_this(), std::placeholders::_1));
            }

            // 处理连接回调
            void handle_connect(const boost::system::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }

                start();
            }

            // 处理读回调
            void handle_read(const boost::system::error_code& ec, size_t bytes_transferred, bool close)
            {
                if (ec) {
                    std::string tmp = ec.message();
                    shutdown();
                    return;
                }

                if (m_handler)
                    m_handler->on_read_cbk(m_session_id, m_read_msg);

                if(close)
                    shutdown();
            }

            // 处理写回调
            void handle_write(const boost::system::error_code& ec, size_t /*bytes_transferred*/, bool close)
            {
                if (ec) {
                    return shutdown();
                }

                if (m_handler)
                    m_handler->on_write_cbk(m_session_id, m_send_msg);

                // 客户端解析应答信息,当客户端写出数据后等待读取之后退出
                // 否则直接退出
                if (!isRequest || !close)
                    return read(close);

                if (close)
                    return shutdown();
            }

        private:
            boost::asio::ip::tcp::resolver m_resolver;

            // asio的socket封装
            socket_type             m_socket;
            SessionID               m_session_id;

            // 读缓冲
            read_buffer_type        m_read_buf;

            // 回调操作
            callback_type*          m_handler;

            // 是否已启动
            std::atomic<bool>	    m_started_flag;
            // 是否终止状态
            std::atomic<bool>	    m_stop_flag;

            // 连接者IP
            std::string             m_connect_ip;
            // 连接者Port
            unsigned short          m_connect_port;

            strand_type             m_strand;
            read_msg_type           m_read_msg;
            send_msg_type           m_send_msg;
        };

        // 默认的客户端, 发送请求,读取应答
        using HttpClientSession = HttpSession<false, boost::beast::http::string_body>;

    }
}