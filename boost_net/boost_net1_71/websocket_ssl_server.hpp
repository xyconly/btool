/*************************************************
File name:      websocket_ssl_server.hpp
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
#include "websocket_ssl_session.hpp"

namespace BTool
{
    namespace BoostNet1_71
    {
        // Websocket����
        class WebsocketSslServer : public std::enable_shared_from_this<WebsocketSslServer>
        {
            typedef AsioContextPool::ioc_type                   ioc_type;
            typedef boost::asio::ip::tcp::acceptor              accept_type;
            typedef std::shared_ptr<WebsocketSslSession>        WebsocketSslSessionPtr;
            typedef BoostNet::NetCallBack::SessionID            SessionID;
            typedef std::map<SessionID, WebsocketSslSessionPtr> WebsocketSslSessionMap;
        
        public:
            // Websocket����
            // handler: session���ػص�
            WebsocketSslServer(AsioContextPool& ioc, boost::asio::ssl::context& ctx, size_t max_wbuffer_size = WebsocketSslSession::NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = WebsocketSslSession::MAX_READSINGLE_BUFFER_SIZE)
                : m_ioc_pool(ioc)
                , m_acceptor(ioc.get_io_context())
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
                , m_ctx(ctx)
            {
            }

            ~WebsocketSslServer() {
                m_handler = BoostNet::NetCallBack();
                m_error_handler = nullptr;
                stop();
                boost::system::error_code ec;
                m_acceptor.close(ec);
            }

            // ���ü�������ص�
            WebsocketSslServer& register_error_cbk(const BoostNet::NetCallBack::server_error_cbk& cbk) {
                m_error_handler = cbk;
                return *this;
            }

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(const BoostNet::NetCallBack& handler) {
                m_handler = handler;
            }
            // ���ÿ������ӻص�
            WebsocketSslServer& register_open_cbk(const BoostNet::NetCallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // ���ùر����ӻص�
            WebsocketSslServer& register_close_cbk(const BoostNet::NetCallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // ���ö�ȡ��Ϣ�ص�
            WebsocketSslServer& register_read_cbk(const BoostNet::NetCallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // �����ѷ�����Ϣ�ص�
            WebsocketSslServer& register_write_cbk(const BoostNet::NetCallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // ������ʽ��������
            // ip: ����IP,֧������,Ĭ�ϱ���IPV4��ַ
            // port: �����˿�
            // reuse_address: �Ƿ����ö˿ڸ���
            bool start(unsigned short port, bool reuse_address = true) {
                return start(nullptr, port, reuse_address);
            }
            bool start(const char* ip, unsigned short port, bool reuse_address = true) {
                if (!start_listen(ip, port, reuse_address)) {
                    return false;
                }
                m_ioc_pool.start();
                return true;
            }

            // ����ʽ��������,ʹ��join_all�ȴ�
            // ip: ����IP,֧������,Ĭ�ϱ���IPV4��ַ
            // port: �����˿�
            // reuse_address: �Ƿ����ö˿ڸ���
            void run(unsigned short port, bool reuse_address = true) {
                run(nullptr, port, reuse_address);
            }
            void run(const char* ip, unsigned short port, bool reuse_address = true) {
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
                boost::asio::ip::tcp::endpoint endpoint = BoostNet::GetEndPointByHost(ip, port, ec);
                if (ec)
                    return false;;

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
                        boost::beast::bind_front_handler(&WebsocketSslServer::handle_accept, shared_from_this()));
                }
                catch (std::exception&) {
                    stop();
                    if (m_error_handler)
                        m_error_handler();
                }
            }

            // ��������ص�
            void handle_accept(const boost::beast::error_code& ec, boost::asio::ip::tcp::socket socket)
            {
                start_accept();

                if (ec) {
                    return;
                }

                auto session_ptr = std::make_shared<WebsocketSslSession>(std::move(socket), m_ctx, m_max_wbuffer_size, m_max_rbuffer_size);
                session_ptr->register_cbk(m_handler).register_close_cbk(std::bind(&WebsocketSslServer::on_close_cbk, shared_from_this(), std::placeholders::_1));

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bool rslt = m_sessions.emplace(session_ptr->get_session_id(), session_ptr).second;
                    if (!rslt)
                        return session_ptr->shutdown();
                }

                session_ptr->start();
            }

            // �������Ӷ���
            WebsocketSslSessionPtr find_session(SessionID session_id) const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return WebsocketSslSessionPtr();
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
            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id)
            {
                remove_session(session_id);
                if (m_handler.close_cbk_)
                    m_handler.close_cbk_(session_id);
            }

        private:
            AsioContextPool&                            m_ioc_pool;
            accept_type                                 m_acceptor;
            BoostNet::NetCallBack                       m_handler;
            BoostNet::NetCallBack::server_error_cbk     m_error_handler = nullptr;
            size_t                                      m_max_wbuffer_size;
            size_t                                      m_max_rbuffer_size;

            boost::asio::ssl::context&                  m_ctx;

            mutable std::mutex                          m_mutex;
            // �������Ӷ��󣬺��ڸ�Ϊ�ڴ�飬��ʡ����/�ͷ��ڴ�ʱ��
            WebsocketSslSessionMap                      m_sessions;
        };
    }
}
