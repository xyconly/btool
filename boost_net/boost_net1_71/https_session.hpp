/******************************************************************************
File name:  https_session.hpp
Author:	    AChar
Purpose:    https连接类, http的ssl实现
Note:       客户端可直接使用HttpsClientSession,调用HttpClientNetCallBack回调

示例代码:
        class TestHttpClient : public BTool::BoostNet::HttpClientNetCallBack
        {
            BTool::BoostNet1_71::HttpsClientSession     session_type;
            typedef std::shared_ptr<session_type>       session_ptr_type;
        public:
            TestHttpClient()
                : m_context({ boost::asio::ssl::context::sslv23_client })
            {
                boost::beast::error_code ignore_ec;
                load_root_certificates(m_context, ignore_ec);
                m_session = std::make_shared<session_type>(get_io_service(), m_context);
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
            boost::asio::ssl::context   m_context;
            session_ptr_type            m_session;
        }

备注:
        也可直接自定义发送及返回消息类型, 如
            using SelfHttpClientNetCallBack = HttpNetCallBack<false, boost::beast::http::file_body, boost::beast::http::string_body>;
            using SelfHttpsClientSession = HttpsSession<false, boost::beast::http::file_body, boost::beast::http::string_body>
*****************************************************************************/

#pragma once

#include <tuple>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include "../http_net_callback.hpp"
#include "../../atomic_switch.hpp"

namespace BTool
{
    namespace BoostNet1_71
    {
        // Https连接对象
        template<bool isRequest, typename ReadType, typename WriteType = ReadType, typename Fields = boost::beast::http::fields>
        class HttpsSession : public std::enable_shared_from_this<HttpsSession<isRequest, ReadType, WriteType, Fields>>
        {
        public:
            typedef boost::asio::ip::tcp::resolver                              resolver_type;
            typedef boost::asio::ip::tcp::socket                                socket_type;
            typedef boost::asio::ssl::context                                   ssl_context_type;
            typedef boost::asio::ssl::stream<boost::beast::tcp_stream>          stream_type;
            typedef boost::beast::flat_buffer                                   read_buffer_type;

            typedef HttpsSession<isRequest, ReadType, WriteType, Fields>                SessionType;
            typedef BoostNet::HttpNetCallBack<isRequest, ReadType, WriteType, Fields>   callback_type;
            typedef typename callback_type::read_msg_type                               read_msg_type;
            typedef typename callback_type::send_msg_type                               send_msg_type;
            typedef typename callback_type::SessionID                                   SessionID;

        public:
            // Http连接对象
            // ios: io读写动力服务
            // max_rbuffer_size:单次读取最大缓冲区大小
            HttpsSession(socket_type&& socket, boost::asio::ssl::context& ctx)
                : m_resolver(socket.get_executor())
                , m_stream(std::move(socket), ctx)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
            {
            }

            HttpsSession(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx)
                : m_resolver(boost::asio::make_strand(ioc))
                , m_stream(boost::asio::make_strand(ioc), ctx)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
            {
            }

            ~HttpsSession() {
                m_handler = nullptr;
                close();
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // 是否已开启
            bool is_open() const {
                return  m_atomic_switch.has_started();
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
            void async_connect(const char* host, unsigned short port)
            {
                if (!m_atomic_switch.init())
                    return;
                
                if (!SSL_set_tlsext_host_name(m_stream.native_handle(), host))
                {
                    boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                    close();
                    return;
                }

                m_connect_ip = host;
                m_connect_port = port;

                m_resolver.async_resolve(
                    host,
                    std::to_string(port),
                    boost::beast::bind_front_handler(&SessionType::handle_resolve, SessionType::shared_from_this()));
            }

            // 客户端开启连接,同时开启读取
            void async_reconnect()
            {
                async_connect(m_connect_ip.c_str(), m_connect_port);
            }

            // 服务端开启连接,同时开启读取
            void start(boost::asio::ssl::stream_base::handshake_type handshake = boost::asio::ssl::stream_base::server)
            {
                if (!m_atomic_switch.init())
                    return;

                boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                m_stream.async_handshake(handshake,
                    boost::beast::bind_front_handler(&SessionType::handle_handshake, SessionType::shared_from_this()));
            }

            // 同步关闭
            void shutdown()
            {
                if (!m_atomic_switch.stop())
                    return;

                m_read_buf.consume(m_read_buf.size());
                close();

                m_atomic_switch.reset();
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

            // 异步写,开启异步写之前先确保开启异步连接
            bool async_write(send_msg_type&& msg)
            {
                m_send_msg = std::forward<send_msg_type>(msg);
                m_send_msg.set(boost::beast::http::field::host, m_connect_ip);

                boost::beast::http::async_write(
                    m_stream, m_send_msg
                    , boost::beast::bind_front_handler(&SessionType::handle_write
                        , SessionType::shared_from_this()
                        , m_send_msg.need_eof()));

                return true;
            }

            // 使用ip+port(port为0则为host解析)同步发送,仅用于客户端,非线程安全
            static std::tuple<bool, read_msg_type> SyncWrite(const char* host, unsigned short port, send_msg_type&& send_msg, boost::asio::ssl::context& ctx)
            {
                if (port == 0)
                    return SyncWrite(host, std::forward<send_msg_type>(send_msg), ctx);

                read_msg_type read_msg = {};
                boost::asio::io_context ioc;
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::asio::ip::tcp::resolver resolver(ioc);
                stream_type stream(ioc, ctx);

                try
                {
                    read_msg.body() = "set tlsext host name error";
                    // 证书
                    if (!SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        read_msg.body() = ec.message();
                        return std::forward_as_tuple(false, std::move(read_msg));
                    }

                    read_msg.body() = "connect error";
                    // 连接
                    auto const results = resolver.resolve(host, std::to_string(port));
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // 握手
                    read_msg.body() = "handshake error";
                    stream.handshake(boost::asio::ssl::stream_base::client);

                    read_msg.body() = "write error";
                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // 读取应答
                    read_buffer_type read_buf;
                    auto read_len = boost::beast::http::read(stream, read_buf, read_msg);
                    boost::beast::error_code ec;
                    stream.shutdown(ec);
                }
                catch (std::exception const&)
                {
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                return std::forward_as_tuple(true, std::move(read_msg));
            }
            // 使用ip+port(port为0则为host解析)同步发送,仅用于客户端
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, unsigned short port, send_msg_type&& send_msg, boost::asio::ssl::context& ctx)
            {
                if (port == 0)
                    return SyncWriteEndOfStream(host, std::forward<send_msg_type>(send_msg), ctx);

                std::vector<read_msg_type> rslt;
                boost::asio::io_context ioc;
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::asio::ip::tcp::resolver resolver(ioc);
                stream_type stream(ioc, ctx);
                
                try
                {
                    // 证书
                    if (!::SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        return std::forward_as_tuple(false, std::move(rslt));
                    }

                    // 连接
                    auto const results = resolver.resolve(host, std::to_string(port));
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // 握手
                    stream.handshake(boost::asio::ssl::stream_base::client);

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
                        if (ec == boost::beast::http::error::end_of_stream)
                            break;
                        rslt.push_back(std::move(read_msg));
                        if (ec)
                            return std::forward_as_tuple(false, std::move(rslt));
                    }

                    stream.shutdown(ec);
                }
                catch (std::exception const&)
                {
                    return std::forward_as_tuple(false, std::move(rslt));
                }

                return std::forward_as_tuple(true, std::move(rslt));
            }
            // 使用域名同步发送,仅用于客户端,非线程安全
            static std::tuple<bool, read_msg_type> SyncWrite(const char* host, send_msg_type&& send_msg, boost::asio::ssl::context& ctx)
            {
                read_msg_type read_msg = {};
                boost::asio::io_context ioc;
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::asio::ip::tcp::resolver resolver(ioc);
                stream_type stream(ioc, ctx);

                try
                {
                    read_msg.body() = "set tlsext host name error";
                    // 设置server_name扩展
                    if (!::SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        read_msg.body() = ec.message();
                        return std::forward_as_tuple(false, std::move(read_msg));
                    }

                    read_msg.body() = "connect error";
                    // 连接
                    boost::asio::ip::tcp::resolver::query query(host, "https");
                    auto const results = resolver.resolve(query);
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // 握手
                    read_msg.body() = "handshake error";
                    stream.handshake(boost::asio::ssl::stream_base::client);

                    read_msg.body() = "write error";
                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // 读取应答
                    read_buffer_type read_buf;
                    auto read_len = boost::beast::http::read(stream, read_buf, read_msg);

                    boost::beast::error_code ec;
                    stream.shutdown(ec);
                }
                catch (std::exception const&)
                {
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                return std::forward_as_tuple(true, std::move(read_msg));
            }
            // 使用域名同步发送,仅用于客户端,非线程安全
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, send_msg_type&& send_msg, boost::asio::ssl::context& ctx)
            {
                std::vector<read_msg_type> rslt;
                boost::asio::io_context ioc;
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::asio::ip::tcp::resolver resolver(ioc);
                stream_type stream(ioc, ctx);
                
                try
                {
                    // 设置server_name扩展
                    if (!::SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        return std::forward_as_tuple(false, std::move(rslt));
                    }

                    // 连接
                    boost::asio::ip::tcp::resolver::query query(host, "https");
                    auto const results = resolver.resolve(query);
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // 握手
                    stream.handshake(boost::asio::ssl::stream_base::client);

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
                        if (ec == boost::beast::http::error::end_of_stream)
                            break;
                        rslt.push_back(std::move(read_msg));
                        if (ec)
                            return std::forward_as_tuple(false, std::move(rslt));
                    }

                    stream.shutdown(ec);
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
                m_stream.shutdown(ignored_ec);
 //               m_stream.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);

                if (m_handler) {
                    m_handler->on_close_cbk(m_session_id);
                }
            }

            // 异步读
            void read(bool close)
            {
                try {
                    m_read_msg = {};
                    boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                    boost::beast::http::async_read(m_stream, m_read_buf, m_read_msg,
                        boost::beast::bind_front_handler(&SessionType::handle_read
                            , SessionType::shared_from_this()
                            , close));
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

                boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                boost::beast::get_lowest_layer(m_stream).async_connect(results
                    , boost::beast::bind_front_handler(&SessionType::handle_connect, SessionType::shared_from_this()));
            }

            // 处理连接回调
            void handle_connect(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type::endpoint_type& endpoint)
            {
                if (ec)
                    return close();

                start(boost::asio::ssl::stream_base::client);
            }

            // 处理连接回调
            void handle_handshake(boost::beast::error_code ec)
            {
                if (ec || !m_atomic_switch.start())
                    return close();

                if (m_connect_ip.empty()) {
                    m_connect_ip = m_stream.lowest_layer().remote_endpoint(ec).address().to_v4().to_string();
                }
                if (m_connect_port == 0)
                    m_connect_port = m_stream.lowest_layer().remote_endpoint(ec).port();

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }

                if (isRequest)
                    read(false);
            }

            // 处理读回调
            void handle_read(bool close, const boost::beast::error_code& ec, size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                if (ec) {
                    shutdown();
                    return;
                }

                if (m_handler)
                    m_handler->on_read_cbk(m_session_id, m_read_msg);

                if (close)
                    shutdown();
            }

            // 处理写回调
            void handle_write(bool close, const boost::beast::error_code& ec, size_t /*bytes_transferred*/)
            {
                if (ec) {
                    auto mmm = ec.message();
                    return shutdown();
                }

                if (m_handler)
                    m_handler->on_write_cbk(m_session_id, m_send_msg);

                if (!isRequest || !close)
                    return read(close);

                if (close)
                    return shutdown();
            }

        private:
            // asio的socket封装
            resolver_type           m_resolver;
            stream_type             m_stream;
            SessionID               m_session_id;

            // 读缓冲
            read_buffer_type        m_read_buf;

            // 回调操作
            callback_type*          m_handler;

            // 原子启停标志
            AtomicSwitch            m_atomic_switch;

            // 连接者IP
            std::string             m_connect_ip;
            // 连接者Port
            unsigned short          m_connect_port;

            read_msg_type           m_read_msg;
            send_msg_type           m_send_msg;
        };

        // 默认的客户端, 发送请求,读取应答
        using HttpsClientSession = HttpsSession<false, boost::beast::http::string_body>;

    }
}