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
                m_session = std::make_shared<session_type>(get_io_service());
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
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include "../http_net_callback.hpp"

namespace BTool
{
    namespace BoostNet1_71
    {
        // Http连接对象
        template<bool isRequest, class ReadType, class WriteType = ReadType, class Fields = boost::beast::http::fields>
        class HttpSession : public std::enable_shared_from_this<HttpSession<isRequest, ReadType, WriteType, Fields>>
        {
        public:
            typedef boost::asio::ip::tcp::socket                                        socket_type;
            typedef boost::asio::ip::tcp::resolver                                      resolver_type;
            typedef boost::beast::tcp_stream                                            stream_type;
            typedef boost::beast::flat_buffer                                           read_buffer_type;

            typedef HttpSession<isRequest, ReadType, WriteType, Fields>                 SessionType;
            typedef BoostNet::HttpNetCallBack<isRequest, ReadType, WriteType, Fields>   callback_type;
            typedef typename callback_type::read_msg_type                               read_msg_type;
            typedef typename callback_type::send_msg_type                               send_msg_type;
            typedef typename callback_type::SessionID                                   SessionID;

        public:
            // Http连接对象
            // ios: io读写动力服务
            // max_rbuffer_size:单次读取最大缓冲区大小
            HttpSession(socket_type&& socket)
                : m_resolver(socket.get_executor())
                , m_stream(std::move(socket))
                , m_started_flag(false)
                , m_stop_flag(true)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
            {
            }
            HttpSession(boost::asio::io_context& ioc)
                : m_resolver(boost::asio::make_strand(ioc))
                , m_stream(boost::asio::make_strand(ioc))
                , m_started_flag(false)
                , m_stop_flag(true)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
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
                return m_stream.socket();
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
                    boost::beast::bind_front_handler(&SessionType::handle_resolve, SessionType::shared_from_this()));
            }

            // 客户端开启连接,同时开启读取
            void reconnect()
            {
                m_resolver.async_resolve(m_connect_ip, std::to_string(m_connect_port),
                    boost::beast::bind_front_handler(&SessionType::handle_resolve, SessionType::shared_from_this()));
            }

            // 服务端开启连接,同时开启读取
            void start()
            {
                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                if (m_connect_ip.empty()) {
                    boost::beast::error_code ec;
                    m_connect_ip = get_socket().remote_endpoint(ec).address().to_v4().to_string();
                    m_connect_port = get_socket().remote_endpoint(ec).port();
                }

                // 服务端解析请求信息,当服务端开启时先开启读端口
                m_stop_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }

                if(isRequest)
                    read(false);
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

            // 异步写
            bool async_write(send_msg_type&& msg)
            {
                m_send_msg = std::forward<send_msg_type>(msg);
                m_send_msg.set(boost::beast::http::field::host, m_connect_ip);

                boost::beast::http::async_write(
                    m_stream, m_send_msg
                    , boost::beast::bind_front_handler(&SessionType::handle_write, SessionType::shared_from_this(), m_send_msg.need_eof()));

                return true;
            }

            // 获取待发送post请求信息
            // target: 路径,包含Query
            // version: https协议版本
            static send_msg_type GetSendPostRequest(const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                return GetSendRequest(boost::beast::http::verb::post, target, content_type, version);
            }
            // 获取待发送get请求信息
            // target: 路径,包含Query
            // version: https协议版本
            static send_msg_type GetSendGetRequest(const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                return GetSendRequest(boost::beast::http::verb::get, target, content_type, version);
            }

            // 使用ip+port(port为0则为host解析)同步发送,仅用于客户端,非线程安全
            static std::tuple<bool, read_msg_type> SyncWrite(const char* host, unsigned short port, send_msg_type&& send_msg)
            {
                if (port == 0)
                    return SyncWrite(host, std::forward<send_msg_type>(send_msg));

                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver resolver(ioc);
                boost::beast::tcp_stream stream(ioc);

                read_msg_type read_msg = {};
                try
                {
                    read_msg.body() = "connect error";
                    // 连接
                    auto const results = resolver.resolve(host, std::to_string(port));
                    stream.connect(results);

                    read_msg.body() = "write error";
                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // 读取应答
                    read_buffer_type read_buf;
                    auto read_len = boost::beast::http::read(stream, read_buf, read_msg);

                    boost::beast::error_code ec;
                    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    stream.socket().close(ec);
                }
                catch (std::exception const&)
                {
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                return std::forward_as_tuple(true, std::move(read_msg));
            }
            // 使用ip+port(port为0则为host解析)同步发送,仅用于客户端,非线程安全
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, unsigned short port, send_msg_type&& send_msg)
            {
                if (port == 0)
                    return SyncWriteEndOfStream(host, std::forward<send_msg_type>(send_msg));

                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver resolver(ioc);
                boost::beast::tcp_stream stream(ioc);

                std::vector<read_msg_type> rslt;
                try
                {
                    // 连接
                    auto const results = resolver.resolve(host, std::to_string(port));
                    stream.connect(results);

                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    // 读取应答
                    boost::beast::error_code ec;
                    for (;;)
                    {
                        read_msg_type read_msg = {};
                        read_buffer_type read_buf;
                        auto read_len = boost::beast::http::read(stream, read_buf, read_msg, ec);
                        if (ec == boost::beast::http::error::end_of_stream
                            /*|| ec == boost::beast::errc::not_connected*/)
                            break;
                        rslt.push_back(std::move(read_msg));
                        if (ec)
                            return std::forward_as_tuple(false, std::move(rslt));
                    }

                    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    stream.socket().close(ec);
                }
                catch (std::exception const&)
                {
                    return std::forward_as_tuple(false, std::move(rslt));
                }

                return std::forward_as_tuple(true, std::move(rslt));
            }
            // 使用域名同步发送,仅用于客户端,非线程安全
            static std::tuple<bool, read_msg_type> SyncWrite(const char* host, send_msg_type&& send_msg)
            {
                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver resolver(ioc);
                boost::beast::tcp_stream stream(ioc);

                read_msg_type read_msg = {};
                try
                {
                    read_msg.body() = "connect error";
                    // 连接
                    boost::asio::ip::tcp::resolver::query query(host, "http");
                    stream.connect(resolver.resolve(query));

                    read_msg.body() = "write error";
                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // 读取应答
                    read_buffer_type read_buf;
                    auto read_len = boost::beast::http::read(stream, read_buf, read_msg);

                    boost::beast::error_code ec;
                    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    stream.socket().close(ec);
                }
                catch (std::exception const&)
                {
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                return std::forward_as_tuple(true, std::move(read_msg));
            }
            // 使用域名同步发送,仅用于客户端,非线程安全
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, send_msg_type&& send_msg)
            {
                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver resolver(ioc);
                boost::beast::tcp_stream stream(ioc);

                std::vector<read_msg_type> rslt;
                try
                {
                    // 连接
                    boost::asio::ip::tcp::resolver::query query(host, "http");
                    stream.connect(resolver.resolve(query));

                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    // 读取应答
                    boost::beast::error_code ec;
                    for (;;)
                    {
                        read_msg_type read_msg = {};
                        read_buffer_type read_buf;
                        auto read_len = boost::beast::http::read(stream, read_buf, read_msg, ec);
                        if (ec == boost::beast::http::error::end_of_stream
                            /*|| ec == boost::beast::errc::not_connected*/)
                            break;
                        rslt.push_back(std::move(read_msg));
                        if (ec)
                            return std::forward_as_tuple(false, std::move(rslt));
                    }

                    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    stream.socket().close(ec);
                }
                catch (std::exception const&)
                {
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
            static send_msg_type GetSendRequest(boost::beast::http::verb verb, const std::string& target, const std::string& content_type, int version)
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
                boost::beast::error_code ignored_ec;
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

                    if (isRequest)
                        m_stream.expires_after(std::chrono::seconds(30));

                    // Read a request
                    boost::beast::http::async_read(m_stream, m_read_buf, m_read_msg,
                        boost::beast::bind_front_handler(
                            &SessionType::handle_read, SessionType::shared_from_this(), close));
                }
                catch (...) {
                    shutdown();
                }
            }

            // 解析IP回调
            void handle_resolve(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec)
                    return close();

                m_stream.expires_after(std::chrono::seconds(30));

                m_stream.async_connect(results
                    , boost::beast::bind_front_handler(&SessionType::handle_connect, SessionType::shared_from_this()));
            }

            // 处理连接回调
            void handle_connect(const boost::beast::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }

                m_stream.expires_after(std::chrono::seconds(30));
                start();
            }

            // 处理读回调
            void handle_read(bool close, const boost::beast::error_code& ec, size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);

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
            void handle_write(bool close, const boost::beast::error_code& ec, size_t /*bytes_transferred*/)
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
            resolver_type           m_resolver;
            stream_type             m_stream;
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

            read_msg_type           m_read_msg;
            send_msg_type           m_send_msg;
        };

        // 默认的客户端, 发送请求,读取应答
        using HttpClientSession = HttpSession<false, boost::beast::http::string_body>;

    }
}