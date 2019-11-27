/******************************************************************************
File name:  https_server.hpp
Author:	    AChar
Purpose:    http������
Note:       ��ֱ��ʹ��HttpsService,����HttpServiceNetCallBack�ص�

ʾ������:
        class TestHttpsService : public BTool::BoostNet::HttpServiceNetCallBack
        {
            typedef BTool::BoostNet1_71::HttpsService   service_type;
            typedef std::shared_ptr<service_type>       service_ptr_type;
        public:
            TestHttpsService()
                : m_context({ boost::asio::ssl::context::sslv23 })
            {
                boost::beast::error_code ignore_ec;
                load_server_certificate(m_context, ignore_ec);
                m_service = std::make_shared<service_type>(get_io_service(), m_context);
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
            boost::asio::ssl::context   m_context;
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
#include "../../io_context_pool.hpp"
#include "https_session.hpp"

namespace BTool
{
    namespace BoostNet1_71
    {
        // Http����
        template<bool isRequest, class ReadType, class WriteType = ReadType, class Fields = boost::beast::http::fields>
        class HttpsServer
            : public BoostNet::HttpNetCallBack<isRequest, ReadType, WriteType, Fields>
            , public std::enable_shared_from_this<HttpsServer<isRequest, ReadType, WriteType, Fields>>
        {
            typedef boost::asio::ssl::context                       ssl_context_type;
            typedef boost::asio::ip::tcp::acceptor                  accept_type;

            // ��������
            typedef HttpsServer<isRequest, ReadType, WriteType, Fields>         ServerType;

            // �ص��������
            typedef BoostNet::HttpNetCallBack<isRequest, ReadType, WriteType, Fields>     callback_type;
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
            HttpsServer(AsioContextPool& ioc, ssl_context_type& ssl_context)
                : m_ioc_pool(ioc)
                , m_acceptor(ioc.get_io_context())
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
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // ������ʽ��������,
            // ip: ����IP,Ĭ�ϱ���IPV4��ַ
            // port: �����˿�
            // reuse_address: �Ƿ����ö˿ڸ���
            bool start(const char* ip = nullptr, unsigned short port = 443, bool reuse_address = true)
            {
                m_ioc_pool.start();
                if (!start_listen(ip, port, reuse_address)) {
                    return false;
                }
                return true;
            }

            // ����ʽ��������,ʹ��join_all�ȴ�
            // ip: ����IP,Ĭ�ϱ���IPV4��ַ
            // port: �����˿�
            // reuse_address: �Ƿ����ö˿ڸ���
            void run(const char* ip = nullptr, unsigned short port = 443, bool reuse_address = true)
            {
                if (!start_listen(ip, port, reuse_address)) {
                    return;
                }
                m_ioc_pool.run();
            }

            // ��ֹ��ǰ����
            void stop() {
                clear();
                m_ioc_pool.stop();
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
                return sess_ptr->async_write(std::forward<send_msg_type>(msg));
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
            bool start_listen(const char* ip, unsigned short port, bool reuse_address)
            {
                boost::beast::error_code ec;
                boost::asio::ip::tcp::endpoint endpoint;
                if (!ip) {
                    endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
                }
                else {
                    boost::asio::ip::tcp::resolver rslv(m_ioc_pool.get_io_context());
                    boost::asio::ip::tcp::resolver::query qry(ip, std::to_string(port));
                    auto iter = rslv.resolve(qry, ec);

                    if (iter != boost::asio::ip::tcp::resolver::iterator())
                        endpoint = iter->endpoint();

                    if (ec)
                        return false;
                }

                try {
                    m_acceptor.open(endpoint.protocol());
                    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(reuse_address));
                    m_acceptor.bind(endpoint);
                    m_acceptor.listen(boost::asio::socket_base::max_listen_connections);
                }
                catch (boost::system::system_error&) {
                    return false;
                }
//                 catch (...) {
//                     return false;
//                 }

                if (!m_acceptor.is_open())
                    return false;

                start_accept();
                return true;
            }

            // ��ʼ����
            void start_accept()
            {
                try {
                    m_acceptor.async_accept(
                        boost::asio::make_strand(m_ioc_pool.get_io_context()),
                        boost::beast::bind_front_handler(&ServerType::handle_accept, ServerType::shared_from_this()));
                }
                catch (std::exception&) {
                    stop();
                }
            }

            // ��������ص�
            void handle_accept(const boost::beast::error_code& ec, boost::asio::ip::tcp::socket socket)
            {
                start_accept();
                if (ec) {
                    return;
                }

                auto session_ptr = std::make_shared<session_type>(std::move(socket), m_ssl_context);
                session_ptr->register_cbk(this);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_sessions.emplace(session_ptr->get_session_id(), session_ptr);
                }

                session_ptr->start();
            }

            // �������Ӷ���
            session_ptr_type find_session(SessionID session_id) const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return session_ptr_type();
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
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& request) override
            {
                if (m_handler)
                    m_handler->on_read_cbk(session_id, request);
            }

        private:
            AsioContextPool&                        m_ioc_pool;
            ssl_context_type&                       m_ssl_context;
            accept_type                             m_acceptor;
            callback_type*                          m_handler;

            mutable std::mutex                      m_mutex;
            // �������Ӷ��󣬺��ڸ�Ϊ�ڴ�飬��ʡ����/�ͷ��ڴ�ʱ��
            session_map_type                        m_sessions;
        };

        // Ĭ�ϵķ����, ��ȡ����,����Ӧ��
        using HttpsService = HttpsServer<true, boost::beast::http::string_body>;
    }
}
