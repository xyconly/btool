/******************************************************************************
File name:  http_session.hpp
Author:	    AChar
Purpose:    http������
Note:       �ͻ��˿�ֱ��ʹ��HttpClientSession,����HttpClientNetCallBack�ص�

ʾ������:
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
            // �������ӻص�
            virtual void on_open_cbk(SessionID session_id) override;

            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id) override;

            // ��ȡ��Ϣ�ص�,��ʱread_msg_typeΪboost::beast::http::response<boost::beast::http::string_body>
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) override;

            // д����Ϣ�ص�,��ʱsend_msg_typeΪboost::beast::http::request<boost::beast::http::string_body>
            virtual void on_write_cbk(SessionID session_id, const send_msg_type& send_msg) override;

        private:
            session_ptr_type            m_session;
        }

��ע:
        Ҳ��ֱ���Զ��巢�ͼ�������Ϣ����, ��
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
        // Http���Ӷ���
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
            // Http���Ӷ���
            // ios: io��д��������
            // max_rbuffer_size:���ζ�ȡ��󻺳�����С
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

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // ���socket
            socket_type& get_socket() {
                return m_stream.socket();
            }

            // ��ȡ����ID
            SessionID get_session_id() const {
                return m_session_id;
            }

            // ��ȡ������IP
            const std::string& get_ip() const {
                return m_connect_ip;
            }

            // ��ȡ������port
            unsigned short get_port() const {
                return m_connect_port;
            }

            // �ͻ��˿�������,ͬʱ������ȡ
            void connect(const char* ip, unsigned short port)
            {
                m_connect_ip = ip;
                m_connect_port = port;

                m_resolver.async_resolve(ip, std::to_string(port),
                    boost::beast::bind_front_handler(&SessionType::handle_resolve, SessionType::shared_from_this()));
            }

            // �ͻ��˿�������,ͬʱ������ȡ
            void reconnect()
            {
                m_resolver.async_resolve(m_connect_ip, std::to_string(m_connect_port),
                    boost::beast::bind_front_handler(&SessionType::handle_resolve, SessionType::shared_from_this()));
            }

            // ����˿�������,ͬʱ������ȡ
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

                // ����˽���������Ϣ,������˿���ʱ�ȿ������˿�
                m_stop_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }

                if(isRequest)
                    read(false);
            }

            // ͬ���ر�
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

            // �첽д
            bool async_write(send_msg_type&& msg)
            {
                m_send_msg = std::forward<send_msg_type>(msg);
                m_send_msg.set(boost::beast::http::field::host, m_connect_ip);

                boost::beast::http::async_write(
                    m_stream, m_send_msg
                    , boost::beast::bind_front_handler(&SessionType::handle_write, SessionType::shared_from_this(), m_send_msg.need_eof()));

                return true;
            }

            // ��ȡ������post������Ϣ
            // target: ·��,����Query
            // version: httpsЭ��汾
            static send_msg_type GetSendPostRequest(const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                return GetSendRequest(boost::beast::http::verb::post, target, content_type, version);
            }
            // ��ȡ������get������Ϣ
            // target: ·��,����Query
            // version: httpsЭ��汾
            static send_msg_type GetSendGetRequest(const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                return GetSendRequest(boost::beast::http::verb::get, target, content_type, version);
            }

            // ʹ��ip+port(portΪ0��Ϊhost����)ͬ������,�����ڿͻ���,���̰߳�ȫ
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
                    // ����
                    auto const results = resolver.resolve(host, std::to_string(port));
                    stream.connect(results);

                    read_msg.body() = "write error";
                    // ������Ϣ
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // ��ȡӦ��
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
            // ʹ��ip+port(portΪ0��Ϊhost����)ͬ������,�����ڿͻ���,���̰߳�ȫ
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
                    // ����
                    auto const results = resolver.resolve(host, std::to_string(port));
                    stream.connect(results);

                    // ������Ϣ
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    // ��ȡӦ��
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
            // ʹ������ͬ������,�����ڿͻ���,���̰߳�ȫ
            static std::tuple<bool, read_msg_type> SyncWrite(const char* host, send_msg_type&& send_msg)
            {
                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver resolver(ioc);
                boost::beast::tcp_stream stream(ioc);

                read_msg_type read_msg = {};
                try
                {
                    read_msg.body() = "connect error";
                    // ����
                    boost::asio::ip::tcp::resolver::query query(host, "http");
                    stream.connect(resolver.resolve(query));

                    read_msg.body() = "write error";
                    // ������Ϣ
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // ��ȡӦ��
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
            // ʹ������ͬ������,�����ڿͻ���,���̰߳�ȫ
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, send_msg_type&& send_msg)
            {
                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver resolver(ioc);
                boost::beast::tcp_stream stream(ioc);

                std::vector<read_msg_type> rslt;
                try
                {
                    // ����
                    boost::asio::ip::tcp::resolver::query query(host, "http");
                    stream.connect(resolver.resolve(query));

                    // ������Ϣ
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    // ��ȡӦ��
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

            // �첽��
            // close: ��ȡ��Ϻ��Ƿ�ر�
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

            // ����IP�ص�
            void handle_resolve(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec)
                    return close();

                m_stream.expires_after(std::chrono::seconds(30));

                m_stream.async_connect(results
                    , boost::beast::bind_front_handler(&SessionType::handle_connect, SessionType::shared_from_this()));
            }

            // �������ӻص�
            void handle_connect(const boost::beast::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }

                m_stream.expires_after(std::chrono::seconds(30));
                start();
            }

            // ������ص�
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

            // ����д�ص�
            void handle_write(bool close, const boost::beast::error_code& ec, size_t /*bytes_transferred*/)
            {
                if (ec) {
                    return shutdown();
                }

                if (m_handler)
                    m_handler->on_write_cbk(m_session_id, m_send_msg);

                // �ͻ��˽���Ӧ����Ϣ,���ͻ���д�����ݺ�ȴ���ȡ֮���˳�
                // ����ֱ���˳�
                if (!isRequest || !close)
                    return read(close);

                if (close)
                    return shutdown();
            }

        private:
            resolver_type           m_resolver;
            stream_type             m_stream;
            SessionID               m_session_id;

            // ������
            read_buffer_type        m_read_buf;

            // �ص�����
            callback_type*          m_handler;
            // �Ƿ�������
            std::atomic<bool>	    m_started_flag;
            // �Ƿ���ֹ״̬
            std::atomic<bool>	    m_stop_flag;

            // ������IP
            std::string             m_connect_ip;
            // ������Port
            unsigned short          m_connect_port;

            read_msg_type           m_read_msg;
            send_msg_type           m_send_msg;
        };

        // Ĭ�ϵĿͻ���, ��������,��ȡӦ��
        using HttpClientSession = HttpSession<false, boost::beast::http::string_body>;

    }
}