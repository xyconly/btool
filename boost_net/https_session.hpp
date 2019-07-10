/******************************************************************************
File name:  https_session.hpp
Author:	    AChar
Purpose:    https连接类, http的ssl实现
Note:       客户端可直接使用HttpsClientSession,调用HttpClientNetCallBack回调

示例代码:
        class TestHttpClient : public BTool::BoostNet::HttpClientNetCallBack
        {
            BTool::BoostNet::HttpsClientSession     session_type;
            typedef std::shared_ptr<session_type>   session_ptr_type;
        public:
            TestHttpClient()
                : m_context({ boost::asio::ssl::context::sslv23_client })
            {
                boost::system::error_code ignore_ec;
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

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl.hpp>
#include "http_net_callback.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // Https连接对象
        template<bool isRequest, typename ReadType, typename WriteType = ReadType, typename Fields = boost::beast::http::fields>
        class HttpsSession : public std::enable_shared_from_this<HttpsSession<isRequest, ReadType, WriteType, Fields>>
        {
        public:
            typedef boost::asio::ip::tcp::socket                                socket_type;
            typedef boost::asio::ssl::stream<socket_type>                       ssl_socket_type;
            typedef boost::asio::io_service                                     ios_type;
            typedef boost::asio::strand<boost::asio::io_context::executor_type> strand_type;
            typedef boost::beast::flat_buffer                                   read_buffer_type;

            typedef HttpsSession<isRequest, ReadType, WriteType, Fields>                SessionType;
            typedef HttpNetCallBack<isRequest, ReadType, WriteType, Fields>             callback_type;
            typedef typename callback_type::read_msg_type                               read_msg_type;
            typedef typename callback_type::send_msg_type                               send_msg_type;
            typedef typename callback_type::SessionID                                   SessionID;

        public:
            // Http连接对象
            // ios: io读写动力服务
            // max_rbuffer_size:单次读取最大缓冲区大小
            HttpsSession(ios_type& ios, boost::asio::ssl::context& context)
                : m_resolver(ios)
                , m_ssl_socket(ios, context)
                , m_started_flag(false)
                , m_stop_flag(true)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
            {
            }

            ~HttpsSession() {
                m_handler = nullptr;
                shutdown();
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // 获得socket
            socket_type::lowest_layer_type& get_socket() {
                return m_ssl_socket.lowest_layer();
            }

            // 获得io_service
            ios_type& get_io_service() {
                return m_ssl_socket.get_io_service();
            }

            // 是否已开启
            bool is_open() const {
                return  m_started_flag.load();
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
                if (!::SSL_set_tlsext_host_name(m_ssl_socket.native_handle(), ip))
                {
                    boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                    close();
                    return;
                }

                m_connect_ip = ip;
                m_connect_port = port;

                m_resolver.async_resolve(ip, std::to_string(port),
                    std::bind(&SessionType::handle_resolve, SessionType::shared_from_this()
                        , std::placeholders::_1, std::placeholders::_2));
            }

            // 客户端开启连接,同时开启读取
            void reconnect()
            {
                connect(m_connect_ip.c_str(), m_connect_port);
            }

            // 服务端开启连接,同时开启读取
            void start()
            {
                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                boost::system::error_code ec;
                m_connect_ip = get_socket().remote_endpoint(ec).address().to_v4().to_string();
                m_connect_port = get_socket().remote_endpoint(ec).port();

                read();

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

            // 异步写
            bool write(send_msg_type&& msg)
            {
                m_send_msg = std::forward<send_msg_type>(msg);

                boost::beast::http::async_write(
                    m_ssl_socket, m_send_msg
                    , std::bind(&SessionType::handle_write
                        , SessionType::shared_from_this()
                        , std::placeholders::_1
                        , std::placeholders::_2
                        , m_send_msg.need_eof()));

                return true;
            }

        protected:
            static SessionID GetNextSessionID()
            {
                static std::atomic<SessionID> next_session_id(callback_type::InvalidSessionID);
                return ++next_session_id;
            }

        private:
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
            void read()
            {
                try {
                    m_read_msg = {};

                    boost::beast::http::async_read(m_ssl_socket, m_read_buf, m_read_msg,
                        std::bind(
                            &SessionType::handle_read,
                            SessionType::shared_from_this(),
                            std::placeholders::_1,
                            std::placeholders::_2));
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

                boost::asio::async_connect(m_ssl_socket.next_layer(), results.begin(), results.end()
                    , std::bind(&SessionType::handle_connect, SessionType::shared_from_this(), std::placeholders::_1));
            }

            // 处理连接回调
            void handle_connect(const boost::system::error_code& ec)
            {
                if (ec)
                    return close();

                m_ssl_socket.async_handshake(boost::asio::ssl::stream_base::client
                    , std::bind(&SessionType::handle_handshake, SessionType::shared_from_this(), std::placeholders::_1));
            }

            // 处理连接回调
            void handle_handshake(boost::system::error_code ec)
            {
                if (ec)
                    return close();

                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                m_connect_ip = get_socket().remote_endpoint(ec).address().to_v4().to_string();
                m_connect_port = get_socket().remote_endpoint(ec).port();

                m_stop_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }
            }

            // 处理读回调
            void handle_read(const boost::system::error_code& ec, size_t bytes_transferred)
            {
                if (ec) {
                    shutdown();
                    return;
                }

                if (m_handler)
                    m_handler->on_read_cbk(m_session_id, m_read_msg);
            }

            // 处理写回调
            void handle_write(const boost::system::error_code& ec, size_t /*bytes_transferred*/, bool close)
            {
                if (ec || close) {
                    auto mmm = ec.message();
                    return shutdown();
                }

                read();

                if (m_handler)
                    m_handler->on_write_cbk(m_session_id, m_send_msg);
            }

        private:
            // asio的socket封装
            ssl_socket_type         m_ssl_socket;
            SessionID               m_session_id;

            boost::asio::ip::tcp::resolver m_resolver;

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
        using HttpsClientSession = HttpsSession<false, boost::beast::http::string_body>;

    }
}