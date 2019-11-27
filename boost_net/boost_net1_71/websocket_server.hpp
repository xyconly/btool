/*************************************************
File name:      websocket_server.hpp
Author:			AChar
Version:
Date:
Purpose: ����boostʵ�ּ�������˿�
Note:    server����洢session����,�ⲿ���ṩID���в���
*************************************************/

#pragma once

#include <mutex>
#include <map>

#include "../../io_context_pool.hpp"
#include "../net_callback.hpp"
#include "websocket_session.hpp"

namespace BTool
{
    namespace BoostNet1_71
    {
        // Websocket����
        class WebsocketServer : public BoostNet::NetCallBack, public std::enable_shared_from_this<WebsocketServer>
        {
            typedef AsioContextPool::ioc_type                   ioc_type;
            typedef boost::asio::ip::tcp::acceptor              accept_type;
            typedef std::shared_ptr<WebsocketSession>           WebsocketSessionPtr;
            typedef std::map<SessionID, WebsocketSessionPtr>    WebsocketSessionMap;
        
        public:
            // Websocket����
            // handler: session���ػص�
            WebsocketServer(AsioContextPool& ioc, size_t max_wbuffer_size = WebsocketSession::NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = WebsocketSession::MAX_READSINGLE_BUFFER_SIZE)
                : m_ioc_pool(ioc)
                , m_acceptor(ioc.get_io_context())
                , m_handler(nullptr)
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
            {
            }

            ~WebsocketServer() {
                m_handler = nullptr;
                stop();
            }

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(BoostNet::NetCallBack* handler) {
                m_handler = handler;
            }

            // ������ʽ��������
            // ip: ����IP,Ĭ�ϱ���IPV4��ַ
            // port: �����˿�
            // reuse_address: �Ƿ����ö˿ڸ���
            bool start(unsigned short port, bool reuse_address = true) {
                return start(nullptr, port, reuse_address);
            }
            bool start(const char* ip, unsigned short port, bool reuse_address = true)
            {
                if (!start_listen(ip, port, reuse_address)) {
                    return false;
                }
                m_ioc_pool.start();
                return true;
            }

            // ����ʽ��������,ʹ��join_all�ȴ�
            // ip: ����IP,Ĭ�ϱ���IPV4��ַ
            // port: �����˿�
            // reuse_address: �Ƿ����ö˿ڸ���
            void run(unsigned short port, bool reuse_address = true) {
                run(nullptr, port, reuse_address);
            }
            void run(const char* ip, unsigned short port, bool reuse_address = true)
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
            bool write(SessionID session_id, const char* send_msg, size_t size)
            {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write(send_msg, size);
            }

            // �ڵ�ǰ��Ϣβ׷��
            // max_package_size: ������Ϣ������
            bool write_tail(SessionID session_id, const char* send_msg, size_t size, size_t max_package_size = 65535)
            {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write_tail(send_msg, size, max_package_size);
            }

            // ���ѵ�ָ�����ȵĶ�����
            void consume_read_buf(SessionID session_id, size_t bytes_transferred)
            {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->consume_read_buf(bytes_transferred);
                }
            }

            // ���ѵ�ָ�����ȵĶ�����
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
            // reuse_address: �Ƿ����ö˿ڸ���
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

                    if(iter != boost::asio::ip::tcp::resolver::iterator())
                        endpoint = iter->endpoint();

                    if (ec)
                        return false;
                }

                try {
                    m_acceptor.open(endpoint.protocol());
                    m_acceptor.set_option(boost::asio::socket_base::reuse_address(reuse_address));
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
                        boost::beast::bind_front_handler(&WebsocketServer::handle_accept, shared_from_this()));
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

                auto session_ptr = std::make_shared<WebsocketSession>(std::move(socket), m_max_wbuffer_size, m_max_rbuffer_size);
                session_ptr->register_cbk(this);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_sessions.emplace(session_ptr->get_session_id(), session_ptr);
                }

                session_ptr->start();
            }

            // �������Ӷ���
            WebsocketSessionPtr find_session(SessionID session_id) const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return WebsocketSessionPtr();
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
            virtual void on_read_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) override
            {
                if (m_handler)
                    m_handler->on_read_cbk(session_id, msg, bytes_transferred);
            }
            // �ѷ�����Ϣ�ص�
            virtual void on_write_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) override
            {
                if (m_handler)
                    m_handler->on_write_cbk(session_id, msg, bytes_transferred);
            }

        private:
            AsioContextPool&        m_ioc_pool;
            accept_type             m_acceptor;
            BoostNet::NetCallBack*  m_handler;
            size_t                  m_max_wbuffer_size;
            size_t                  m_max_rbuffer_size;

            mutable std::mutex      m_mutex;
            // �������Ӷ��󣬺��ڸ�Ϊ�ڴ�飬��ʡ����/�ͷ��ڴ�ʱ��
            WebsocketSessionMap     m_sessions;
        };
    }
}
