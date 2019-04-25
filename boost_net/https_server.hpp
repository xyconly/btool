/******************************************************************************
File name:  https_server.hpp
Author:	    AChar
Purpose:    http������
Note:       ��ֱ��ʹ��HttpsService,����HttpServiceNetCallBack�ص�

ʾ������:
        class TestHttpsService : public BTool::BoostNet::HttpServiceNetCallBack
        {
            BTool::BoostNet::HttpsService           service_type;
            typedef std::shared_ptr<service_type>   service_ptr_type;
        public:
            TestHttpsService()
            {
                m_service = std::make_shared<service_type>(get_io_service());
                m_service->register_cbk(this);
                bool rslt = m_service->start(ip);
            }

        protected:
            // �������ӻص�
            virtual void on_open_cbk(SessionID session_id) override;

            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id) override;

            // ��ȡ��Ϣ�ص�,��ʱread_msg_typeΪboost::beast::http::request<boost::beast::http::string_body>
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) override;

            // д����Ϣ�ص�,��ʱsend_msg_typeΪboost::beast::http::response<boost::beast::http::string_body>
            virtual void on_write_cbk(SessionID session_id, const send_msg_type& send_msg) override;

        private:
            service_ptr_type            m_service;
        }

��ע:
        Ҳ��ֱ���Զ��巢�ͼ�������Ϣ����, ��
            using SelfHttpServiceNetCallBack = HttpNetCallBack<true, boost::beast::http::string_body, boost::beast::http::file_body>;
            using SelfHttpsServer = HttpsServer<true, boost::beast::http::string_body, boost::beast::http::file_body>
*****************************************************************************/

#pragma once

#include <mutex>
#include <map>
#include "../io_service_pool.hpp"
#include "https_session.hpp"
#include "http_net_callback.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // Http����
        template<bool isRequest, class ReadType, class WriteType = ReadType, class Fields = boost::beast::http::fields>
        class HttpsServer : public HttpNetCallBack<isRequest, ReadType, WriteType, Fields>
        {
            // ��������
            typedef HttpsServer<isRequest, ReadType, WriteType, Fields>         ServerType;

            // �ص��������
            typedef HttpNetCallBack<isRequest, ReadType, WriteType, Fields>     callback_type;
            typedef typename callback_type::read_msg_type                       read_msg_type;
            typedef typename callback_type::send_msg_type                       send_msg_type;
            typedef typename callback_type::SessionID                           SessionID;

            // Session����
            typedef HttpsSession<isRequest, ReadType, WriteType, Fields>        session_type;
            typedef std::shared_ptr<session_type>                               session_ptr_type;
            typedef std::map<SessionID, session_ptr_type>                       session_map_type;

        public:
            // Http����
            // handler: session���ػص�
            HttpsServer(AsioServicePool& ios, boost::asio::ssl::context& ssl_context)
                : m_ios_pool(ios)
                , m_acceptor(ios.get_io_service())
                , m_handler(nullptr)
                , m_ssl_context(ssl_context)
            {
            }

            ~HttpsServer() {
                m_handler = nullptr;
                stop();
                m_acceptor.close();
            }

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(HttpNetCallBack* handler) {
                m_handler = handler;
            }

            // ������ʽ��������,
            // ip: ����IP,Ĭ�ϱ���IPV4��ַ
            // port: �����˿�
            bool start(const char* ip = nullptr, unsigned short port = 443)
            {
                if (!start_listen(ip, port)) {
                    return false;
                }
                m_ios_pool.start();
                return true;
            }

            // ����ʽ��������,ʹ��join_all�ȴ�
            void run(const char* ip = nullptr, unsigned short port = 443)
            {
                if (!start_listen(ip, port)) {
                    return;
                }
                m_ios_pool.run();
            }

            // ��ֹ��ǰ����
            void stop() {
                clear();
                m_ios_pool.stop();
            }

            // ��յ�ǰ��������
            // ע��,�ú���������ֹ��ǰ����,����ֹ����յ�ǰ��������,�������ֹ��stop()�в���
            void clear() {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.clear();
            }

            // �첽д��
            bool write(SessionID session_id, send_msg_type&& msg)
            {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write(std::forward<send_msg_type>(msg));
            }

            // ͬ���ر�����,ע���ʱ��close_cbk�����ڵ�ǰ�߳���
            void close(SessionID session_id)
            {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->shutdown();
                }
            }

            // ��ȡ������IP
            bool get_ip(SessionID session_id, std::string& ip) const {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    ip = sess_ptr->get_ip();
                    return true;
                }
                return false;
            }

            // ��ȡ������port
            bool get_port(SessionID session_id, unsigned short& port) const {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    port = sess_ptr->get_port();
                    return true;
                }
                return false;
            }

        private:
            // ���������˿�
            bool start_listen(const char* ip, unsigned short port)
            {
                boost::system::error_code ec;
                boost::asio::ip::tcp::endpoint endpoint;
                if (!ip) {
                    endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
                }
                else {
                    endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip, ec), port);
                    if (ec)
                        return false;
                }
                
                try {
                    m_acceptor.open(endpoint.protocol());
                    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
                    m_acceptor.bind(endpoint);
                    m_acceptor.listen(boost::asio::socket_base::max_listen_connections);
                }
                catch (...) {
                    return false;
                }

                if (!m_acceptor.is_open())
                    return false;

                start_accept();
                return true;
            }

            // ��ʼ����
            void start_accept()
            {
                try {
                    HttpSessionPtr session = std::make_shared<HttpsSession>(m_ios_pool.get_io_service(), m_ssl_context, m_max_rbuffer_size);
                    m_acceptor.async_accept(session->get_socket(), bind(&ServerType::handle_accept, this, boost::placeholders::_1, session));
                }
                catch (std::exception&) {
                    stop();
                }
            }

            // ��������ص�
            void handle_accept(const boost::system::error_code& ec, const session_ptr_type& session_ptr)
            {
                start_accept();
                if (ec) {
                    session_ptr->shutdown();
                    return;
                }

                session_ptr->register_cbk(this);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_sessions.emplace(session_ptr->get_session_id(), session_ptr);
                }

//                 session_ptr->start();

                // ��tcp_session��start�ĵ��ý���io_service,��io_service��������ʱִ��,�������Ӳ�����
                session_ptr->get_io_service().dispatch(boost::bind(&session_type::start, session_ptr));
            }

            // �������Ӷ���
            HttpSessionPtr find_session(SessionID session_id) const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return HttpSessionPtr();
                }
                return iter->second;
            }

            // ɾ�����Ӷ���
            void remove_session(SessionID session_id) 
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.erase(session_id);
            }

        private:
            // �������ӻص�
            virtual void on_open_cbk(SessionID session_id) override
            {
                if(m_handler)
                    m_handler->on_open_cbk(session_id);
            }
            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id) override
            {
                remove_session(session_id);
                if (m_handler)
                    m_handler->on_close_cbk(session_id);
            }
            // ��ȡ��Ϣ�ص�
            virtual void on_read_cbk(SessionID session_id, const request_type& request) override
            {
                if (m_handler)
                    m_handler->on_read_cbk(session_id, request);
            }

        private:
            AsioServicePool&                        m_ios_pool;
            accept_type                             m_acceptor;
            callback_type*                          m_handler;

            boost::asio::ssl::context&              m_ssl_context;

            mutable std::mutex                      m_mutex;
            // �������Ӷ��󣬺��ڸ�Ϊ�ڴ�飬��ʡ����/�ͷ��ڴ�ʱ��
            session_map_type                        m_sessions;
        };

        // Ĭ�ϵķ����, ��ȡ����,����Ӧ��
        using HttpsService = HttpsServer<true, boost::beast::http::string_body>;
    }
}
