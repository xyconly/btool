/******************************************************************************
File name:  http_session.hpp
Author:	    AChar
Purpose:    tcp������
Note:       Ϊ���ⲿ�����ܵ��޻���,�ⲿ������ȡ���ݺ���Ҫ��������consume_read_buf,
            �Դ���ɾ��������
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
        class HttpSession : public std::enable_shared_from_this<HttpSession>
        {
        public:
            typedef boost::asio::ip::tcp::socket                                socket_type;
            typedef boost::asio::io_service                                     ios_type;
            typedef boost::asio::strand<boost::asio::io_context::executor_type> strand_type;
            typedef boost::beast::flat_buffer                                   read_buffer_type;

            typedef typename HttpNetCallBack                                    callback_type;
            typedef typename callback_type::request_type                        request_type;
            typedef typename callback_type::SessionID                           SessionID;

            enum {
                MAX_READSINGLE_BUFFER_SIZE = 2000,
            };

        public:
            // Http���Ӷ���
            // ios: io��д��������
            // max_rbuffer_size:���ζ�ȡ��󻺳�����С
            HttpSession(ios_type& ios, size_t max_rbuffer_size = MAX_READSINGLE_BUFFER_SIZE)
                : m_socket(ios)
                , m_started_flag(false)
                , m_stop_flag(true)
                , m_max_rbuffer_size(max_rbuffer_size)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
                , m_strand(m_socket.get_executor())
                , m_res(nullptr)
            {
            }

            ~HttpSession() {
                shutdown();
                m_handler = nullptr;
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
                m_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip), port)
                                    ,std::bind(&HttpSession::handle_connect, shared_from_this(), std::placeholders::_1));
            }

            // �ͻ��˿�������,ͬʱ������ȡ
            void reconnect()
            {
                m_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(m_connect_ip), m_connect_port)
                    , std::bind(&HttpSession::handle_connect, shared_from_this(), std::placeholders::_1));
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

                read();

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

                boost::system::error_code ignored_ec;
                m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                m_socket.close(ignored_ec);

                m_read_buf.consume(m_read_buf.size());
                m_started_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_close_cbk(m_session_id);
                }
            }

            // �첽д
            template<bool isRequest, class Body, class Fields>
            bool write(boost::beast::http::message<isRequest, Body, Fields>&& msg)
            {
                auto sp = std::make_shared<boost::beast::http::message<isRequest, Body, Fields>>(
                    std::forward<boost::beast::http::message<isRequest, Body, Fields>>(msg));

                if (!sp)
                    return false;

                m_res = sp;

                boost::beast::http::async_write(
                    m_socket,
                    *sp,
                    boost::asio::bind_executor(
                        m_strand,
                        std::bind(
                            &HttpSession::handle_write,
                            shared_from_this(),
                            std::placeholders::_1,
                            std::placeholders::_2,
                            sp->need_eof())));

                return true;
            }

        protected:
            static SessionID GetNextSessionID()
            {
                static std::atomic<SessionID> next_session_id(callback_type::InvalidSessionID);
                return ++next_session_id;
            }

        private:
            // �첽��
            void read()
            {
                try {
                    m_req = {};

                    boost::beast::http::async_read(m_socket, m_read_buf, m_req,
                        boost::asio::bind_executor(
                            m_strand,
                            std::bind(
                                &HttpSession::handle_read,
                                shared_from_this(),
                                std::placeholders::_1,
                                std::placeholders::_2)));
                }
                catch (...) {
                    shutdown();
                }
            }

            // �������ӻص�
            void handle_connect(const boost::system::error_code& error)
            {
                if (error)
                {
                    boost::system::error_code ignored_ec;
                    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                    m_socket.close(ignored_ec);

                    if (m_handler) {
                        m_handler->on_close_cbk(m_session_id);
                    }
                    return;
                }

                start();
            }

            // ������ص�
            void handle_read(const boost::system::error_code& error, size_t bytes_transferred)
            {
                if (error) {
                    shutdown();
                    return;
                }
                m_handler->on_read_cbk(m_session_id, m_req);
            }

            // ����д�ص�
            void handle_write(const boost::system::error_code& ec, size_t /*bytes_transferred*/, bool close)
            {
                if (ec || close) {
                    return shutdown();
                }

                m_res = nullptr;
                read();
            }

        private:
            // asio��socket��װ
            socket_type             m_socket;
            SessionID               m_session_id;

            // ������
            read_buffer_type        m_read_buf;
            // ������������С
            size_t                  m_max_rbuffer_size;

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
            request_type            m_req;
            std::shared_ptr<void>   m_res;
        };
    }
}