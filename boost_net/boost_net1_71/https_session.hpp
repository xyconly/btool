/******************************************************************************
File name:  https_session.hpp
Author:	    AChar
Purpose:    https������, http��sslʵ��
Note:       �ͻ��˿�ֱ��ʹ��HttpsClientSession,����HttpClientNetCallBack�ص�

ʾ������:
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
            // �������ӻص�
            virtual void on_open_cbk(SessionID session_id) override;

            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id) override;

            // ��ȡ��Ϣ�ص�,��ʱread_msg_typeΪboost::beast::http::response<boost::beast::http::string_body>
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) override;

            // д����Ϣ�ص�,��ʱsend_msg_typeΪboost::beast::http::request<boost::beast::http::string_body>
            virtual void on_write_cbk(SessionID session_id, const send_msg_type& send_msg) override;

        private:
            boost::asio::ssl::context   m_context;
            session_ptr_type            m_session;
        }

��ע:
        Ҳ��ֱ���Զ��巢�ͼ�������Ϣ����, ��
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
        // Https���Ӷ���
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
            // Http���Ӷ���
            // ios: io��д��������
            // max_rbuffer_size:���ζ�ȡ��󻺳�����С
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

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // �Ƿ��ѿ���
            bool is_open() const {
                return  m_atomic_switch.has_started();
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

            // �ͻ��˿�������,ͬʱ������ȡ
            void async_reconnect()
            {
                async_connect(m_connect_ip.c_str(), m_connect_port);
            }

            // ����˿�������,ͬʱ������ȡ
            void start(boost::asio::ssl::stream_base::handshake_type handshake = boost::asio::ssl::stream_base::server)
            {
                if (!m_atomic_switch.init())
                    return;

                boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                m_stream.async_handshake(handshake,
                    boost::beast::bind_front_handler(&SessionType::handle_handshake, SessionType::shared_from_this()));
            }

            // ͬ���ر�
            void shutdown()
            {
                if (!m_atomic_switch.stop())
                    return;

                m_read_buf.consume(m_read_buf.size());
                close();

                m_atomic_switch.reset();
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

            // �첽д,�����첽д֮ǰ��ȷ�������첽����
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

            // ʹ��ip+port(portΪ0��Ϊhost����)ͬ������,�����ڿͻ���,���̰߳�ȫ
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
                    // ֤��
                    if (!SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        read_msg.body() = ec.message();
                        return std::forward_as_tuple(false, std::move(read_msg));
                    }

                    read_msg.body() = "connect error";
                    // ����
                    auto const results = resolver.resolve(host, std::to_string(port));
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // ����
                    read_msg.body() = "handshake error";
                    stream.handshake(boost::asio::ssl::stream_base::client);

                    read_msg.body() = "write error";
                    // ������Ϣ
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // ��ȡӦ��
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
            // ʹ��ip+port(portΪ0��Ϊhost����)ͬ������,�����ڿͻ���
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
                    // ֤��
                    if (!::SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        return std::forward_as_tuple(false, std::move(rslt));
                    }

                    // ����
                    auto const results = resolver.resolve(host, std::to_string(port));
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // ����
                    stream.handshake(boost::asio::ssl::stream_base::client);

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
            // ʹ������ͬ������,�����ڿͻ���,���̰߳�ȫ
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
                    // ����server_name��չ
                    if (!::SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        read_msg.body() = ec.message();
                        return std::forward_as_tuple(false, std::move(read_msg));
                    }

                    read_msg.body() = "connect error";
                    // ����
                    boost::asio::ip::tcp::resolver::query query(host, "https");
                    auto const results = resolver.resolve(query);
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // ����
                    read_msg.body() = "handshake error";
                    stream.handshake(boost::asio::ssl::stream_base::client);

                    read_msg.body() = "write error";
                    // ������Ϣ
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg));

                    read_msg.body() = "";
                    // ��ȡӦ��
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
            // ʹ������ͬ������,�����ڿͻ���,���̰߳�ȫ
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, send_msg_type&& send_msg, boost::asio::ssl::context& ctx)
            {
                std::vector<read_msg_type> rslt;
                boost::asio::io_context ioc;
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::asio::ip::tcp::resolver resolver(ioc);
                stream_type stream(ioc, ctx);
                
                try
                {
                    // ����server_name��չ
                    if (!::SSL_set_tlsext_host_name(stream.native_handle(), host))
                    {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        return std::forward_as_tuple(false, std::move(rslt));
                    }

                    // ����
                    boost::asio::ip::tcp::resolver::query query(host, "https");
                    auto const results = resolver.resolve(query);
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // ����
                    stream.handshake(boost::asio::ssl::stream_base::client);

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

            // �첽��
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

            // ����IP�ص�
            void handle_resolve(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec)
                    return close();

                boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                boost::beast::get_lowest_layer(m_stream).async_connect(results
                    , boost::beast::bind_front_handler(&SessionType::handle_connect, SessionType::shared_from_this()));
            }

            // �������ӻص�
            void handle_connect(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type::endpoint_type& endpoint)
            {
                if (ec)
                    return close();

                start(boost::asio::ssl::stream_base::client);
            }

            // �������ӻص�
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

            // ������ص�
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

            // ����д�ص�
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
            // asio��socket��װ
            resolver_type           m_resolver;
            stream_type             m_stream;
            SessionID               m_session_id;

            // ������
            read_buffer_type        m_read_buf;

            // �ص�����
            callback_type*          m_handler;

            // ԭ����ͣ��־
            AtomicSwitch            m_atomic_switch;

            // ������IP
            std::string             m_connect_ip;
            // ������Port
            unsigned short          m_connect_port;

            read_msg_type           m_read_msg;
            send_msg_type           m_send_msg;
        };

        // Ĭ�ϵĿͻ���, ��������,��ȡӦ��
        using HttpsClientSession = HttpsSession<false, boost::beast::http::string_body>;

    }
}