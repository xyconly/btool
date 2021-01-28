/*************************************************
File name:      tcp_server.hpp
Author:			AChar
Version:
Date:
Purpose: ����boostʵ�ּ�������˿�
Note:    server����洢session����,�ⲿ���ṩID���в���
*************************************************/

#pragma once

#include <mutex>
#include <set>
#include <map>
#include <boost/lexical_cast.hpp>
#include "../io_context_pool.hpp"
#include "tcp_session.hpp"
#include "net_callback.hpp"
namespace BTool
{
    namespace BoostNet
    {
        // TCP����
        class TcpServer
        {
            typedef NetCallBack::SessionID                  SessionID;
            typedef AsioContextPool::ioc_type               ioc_type;
            typedef boost::asio::ip::tcp::acceptor          accept_type;
            typedef std::shared_ptr<TcpSession>             TcpSessionPtr;
            typedef std::map<SessionID, TcpSessionPtr>      TcpSessionMap;

        public:
            // TCP����
            // handler: session���ػص�
            TcpServer(AsioContextPool& ioc, size_t max_wbuffer_size = TcpSession::NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = TcpSession::MAX_READSINGLE_BUFFER_SIZE)
                : m_ioc_pool(ioc)
                , m_acceptor(ioc.get_io_context())
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
            {
            }

            ~TcpServer() {
                stop();
                boost::system::error_code ec;
                m_acceptor.close(ec);
            }

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(const NetCallBack& handler) {
                m_handler = handler;
            }
            // ���ÿ������ӻص�
            TcpServer& register_open_cbk(const NetCallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // ���ùر����ӻص�
            TcpServer& register_close_cbk(const NetCallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // ���ö�ȡ��Ϣ�ص�
            TcpServer& register_read_cbk(const NetCallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // �����ѷ�����Ϣ�ص�
            TcpServer& register_write_cbk(const NetCallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // ������ʽ��������,
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
            void run(unsigned short port, bool reuse_address = false) {
                run(nullptr, port, reuse_address);
            }
            void run(const char* ip, unsigned short port, bool reuse_address = false)
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
            bool write(SessionID session_id, const char* send_msg, size_t size) {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write(send_msg, size);
            }
            // �첽����������Ϣ
            // set�з���ʧ�ܵ�session id
            std::set<SessionID> writeAll(const char* send_msg, size_t size) {
                std::set<SessionID> err_session;
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& sess_ptr : m_sessions) {
                    if (!sess_ptr.second->write(send_msg, size))
                        err_session.emplace(sess_ptr.first);
                }
                return err_session;
            }

            // �ڵ�ǰ��Ϣβ׷��
            // max_package_size: ������Ϣ������,������δ������ϻ��߳�������ֵ,���ְ�,�ȴ��´η���
            bool write_tail(SessionID session_id, const char* send_msg, size_t size, size_t max_package_size = 65535) {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write_tail(send_msg, size, max_package_size);
            }

            // ���ѵ�ָ�����ȵĶ�����
            void consume_read_buf(SessionID session_id, size_t bytes_transferred) {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->consume_read_buf(bytes_transferred);
                }
            }

            // ͬ���ر�����,ע���ʱ��close_cbk�����ڵ�ǰ�߳���
            void close(SessionID session_id) {
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
            static boost::asio::ip::tcp::endpoint GetPointByName(const char* host, unsigned short port, boost::system::error_code& ec) {
                ec = boost::asio::error::host_not_found;
                boost::asio::io_context ioc;
                boost::asio::ip::tcp::resolver rslv(ioc);
                boost::asio::ip::tcp::resolver::query qry(host, boost::lexical_cast<std::string>(port));
                try {
                    boost::asio::ip::tcp::resolver::iterator iter = rslv.resolve(qry);
                    if (iter != boost::asio::ip::tcp::resolver::iterator()) {
                        ec.clear();
                        return iter->endpoint();
                    }
                }
                catch (...) {
                    ec = boost::asio::error::fault;
                }
                return boost::asio::ip::tcp::endpoint();
            }
            // ���������˿�
            bool start_listen(const char* ip, unsigned short port, bool reuse_address)
            {
                boost::system::error_code ec;
                boost::asio::ip::tcp::endpoint endpoint;
                if (!ip) {
                    endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
                }
                else {
                    endpoint = GetPointByName(ip, port, ec);
                    if (ec)
                        return false;
                }

                try {
                    m_acceptor.open(endpoint.protocol());
                    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(reuse_address));
                    m_acceptor.bind(endpoint);
                    m_acceptor.listen();
                }
                catch (boost::system::system_error&) {
                    return false;
                }

                if (!m_acceptor.is_open())
                    return false;

                start_accept();
                return true;
            }

            // ��ʼ����
            void start_accept() {
                try {
                    TcpSessionPtr session = std::make_shared<TcpSession>(m_ioc_pool.get_io_context(), m_max_wbuffer_size, m_max_rbuffer_size);
                    m_acceptor.async_accept(session->get_socket(), std::bind(&TcpServer::handle_accept, this, std::placeholders::_1, session));
                }
                catch (std::exception&) {
                    stop();
                }
            }

            // ��������ص�
            void handle_accept(const boost::system::error_code& ec, const TcpSessionPtr& session_ptr) {
                start_accept();
                if (ec) {
                    session_ptr->shutdown();
                    return;
                }

                session_ptr->register_cbk(m_handler).register_close_cbk(std::bind(&TcpServer::on_close_cbk, this, std::placeholders::_1));

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bool rslt = m_sessions.emplace(session_ptr->get_session_id(), session_ptr).second;
                    if (!rslt)
                        return session_ptr->shutdown();
                }

                // ��tcp_session��start�ĵ��ý���io_context,��io_context��������ʱִ��,�������Ӳ�����
                session_ptr->get_io_context().dispatch(std::bind(&TcpSession::start, session_ptr));
            }

            // �������Ӷ���
            TcpSessionPtr find_session(SessionID session_id) const {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return TcpSessionPtr();
                }
                return iter->second;
            }

            // ɾ�����Ӷ���
            void remove_session(SessionID session_id) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.erase(session_id);
            }

        private:
            // �ر����ӻص�
            void on_close_cbk(SessionID session_id) {
                remove_session(session_id);
                if (m_handler.close_cbk_)
                    m_handler.close_cbk_(session_id);
            }

        private:
            AsioContextPool&    m_ioc_pool;
            accept_type         m_acceptor;
            NetCallBack         m_handler;
            size_t              m_max_wbuffer_size;
            size_t              m_max_rbuffer_size;

            mutable std::mutex  m_mutex;
            // �������Ӷ��󣬺��ڸ�Ϊ�ڴ�飬��ʡ����/�ͷ��ڴ�ʱ��
            TcpSessionMap       m_sessions;
        };
    }
}
