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
        // Http���Ӷ���
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
            // Http���Ӷ���
            // ios: io��д��������
            // max_rbuffer_size:���ζ�ȡ��󻺳�����С
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
                shutdown();
            }

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // ���socket
            socket_type& get_socket() {
                return m_socket;
            }

            // ���io_service
            ios_type& get_io_service() {
                return m_socket.get_io_service();
            }

            // �Ƿ��ѿ���
            bool is_open() const {
                return  m_started_flag.load() && m_socket.is_open();
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
                    std::bind(&SessionType::handle_resolve, SessionType::shared_from_this()
                        , std::placeholders::_1, std::placeholders::_2));
            }

            // �ͻ��˿�������,ͬʱ������ȡ
            void reconnect()
            {
                m_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(m_connect_ip), m_connect_port)
                    , std::bind(&SessionType::handle_connect, SessionType::shared_from_this(), std::placeholders::_1));
            }

            // ����˿�������,ͬʱ������ȡ
            void start()
            {
                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                boost::system::error_code ec;
                m_connect_ip = m_socket.remote_endpoint(ec).address().to_v4().to_string();
                m_connect_port = m_socket.remote_endpoint(ec).port();

                // ����˽���������Ϣ,������˿���ʱ�ȿ������˿�
                if(isRequest)
                    read(false);

                m_stop_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }
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

            // ʹ��ip+portͬ������,�����ڿͻ���
            bool sync_write(const char* ip, unsigned short port, send_msg_type&& msg)
            {
                try
                {
                    // ����
                    auto const results = m_resolver.resolve(ip, std::to_string(port));
                    boost::asio::connect(m_socket, results.begin(), results.end());

                    // ������Ϣ
                    m_send_msg = std::forward<send_msg_type>(msg);
                    boost::beast::http::write(m_socket, m_send_msg);

                    // ��ȡӦ��
                    boost::beast::http::read(m_socket, m_read_buf, m_read_msg);

                    if (m_handler)
                        m_handler->on_read_cbk(m_session_id, m_read_msg);

                    boost::system::error_code ec;
                    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    if (ec == boost::asio::error::eof)
                    {
                        ec.assign(0, ec.category());
                    }
                    if (ec)
                        throw boost::system::system_error{ ec };
                }
                catch (std::exception const&)
                {
                    close();
                    return false;
                }

                return true;
            }
            // ʹ������ͬ������,�����ڿͻ���
            bool sync_write(const char* host, send_msg_type&& msg)
            {
                try
                {
                    // ����
                    boost::asio::ip::tcp::resolver::query query(host, "http");
                    boost::asio::connect(m_socket, m_resolver.resolve(query));

                    // ������Ϣ
                    m_send_msg = std::forward<send_msg_type>(msg);
                    boost::beast::http::write(m_socket, m_send_msg);

                    // ��ȡӦ��
                    boost::beast::http::read(m_socket, m_read_buf, m_read_msg);

                    if (m_handler)
                        m_handler->on_read_cbk(m_session_id, m_read_msg);

                    boost::system::error_code ec;
                    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    if (ec == boost::asio::error::eof)
                    {
                        ec.assign(0, ec.category());
                    }
                    if (ec)
                        throw boost::system::system_error{ ec };
                }
                catch (std::exception const&)
                {
                    close();
                    return false;
                }

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

            // �첽��
            // close: ��ȡ��Ϻ��Ƿ�ر�
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

            // ����IP�ص�
            void handle_resolve(const boost::system::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec)
                    return close();

                boost::asio::async_connect(m_socket, results.begin(), results.end()
                    , std::bind(&SessionType::handle_connect, SessionType::shared_from_this(), std::placeholders::_1));
            }

            // �������ӻص�
            void handle_connect(const boost::system::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }

                start();
            }

            // ������ص�
            void handle_read(const boost::system::error_code& ec, size_t bytes_transferred, bool close)
            {
                if (ec) {
                    shutdown();
                    return;
                }

                if (m_handler)
                    m_handler->on_read_cbk(m_session_id, m_read_msg);

                if(close)
                    shutdown();
            }

            // ����д�ص�
            void handle_write(const boost::system::error_code& ec, size_t /*bytes_transferred*/, bool close)
            {
                if (ec) {
                    return shutdown();
                }

                if (m_handler)
                    m_handler->on_write_cbk(m_session_id, m_send_msg);

                // �ͻ��˽���Ӧ����Ϣ,���ͻ���д�����ݺ�ȴ���ȡ֮���˳�
                // ����ֱ���˳�
                if (!isRequest)
                    return read(close);

                if (close)
                    return shutdown();
            }

        private:
            boost::asio::ip::tcp::resolver m_resolver;

            // asio��socket��װ
            socket_type             m_socket;
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

            strand_type             m_strand;
            read_msg_type           m_read_msg;
            send_msg_type           m_send_msg;
        };

        // Ĭ�ϵĿͻ���, ��������,��ȡӦ��
        using HttpClientSession = HttpSession<false, boost::beast::http::string_body>;

    }
}