/*************************************************
File name:      tcp_server.hpp
Author:			AChar
Version:
Date:
Purpose: 利用boost实现监听服务端口
Note:    server本身存储session对象,外部仅提供ID进行操作
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
        // TCP服务
        class TcpServer
        {
            typedef NetCallBack::SessionID                  SessionID;
            typedef AsioContextPool::ioc_type               ioc_type;
            typedef boost::asio::ip::tcp::acceptor          accept_type;
            typedef std::shared_ptr<TcpSession>             TcpSessionPtr;
            typedef std::map<SessionID, TcpSessionPtr>      TcpSessionMap;

        public:
            // TCP服务
            // handler: session返回回调
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

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(const NetCallBack& handler) {
                m_handler = handler;
            }
            // 设置开启连接回调
            TcpServer& register_open_cbk(const NetCallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // 设置关闭连接回调
            TcpServer& register_close_cbk(const NetCallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // 设置读取消息回调
            TcpServer& register_read_cbk(const NetCallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // 设置已发送消息回调
            TcpServer& register_write_cbk(const NetCallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // 非阻塞式启动服务,
            // ip: 监听IP,默认本地IPV4地址
            // port: 监听端口
            // reuse_address: 是否启用端口复用
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

            // 阻塞式启动服务,使用join_all等待
            // ip: 监听IP,默认本地IPV4地址
            // port: 监听端口
            // reuse_address: 是否启用端口复用
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

            // 终止当前服务
            void stop() {
                clear();
                m_ioc_pool.stop();
            }

            // 清空当前所有连接
            // 注意,该函数不会终止当前服务,仅终止并清空当前所有连接,服务的终止在stop()中操作
            void clear() {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.clear();
            }

            // 异步写入
            bool write(SessionID session_id, const char* send_msg, size_t size) {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write(send_msg, size);
            }
            // 异步发送所有消息
            // set中返回失败的session id
            std::set<SessionID> writeAll(const char* send_msg, size_t size) {
                std::set<SessionID> err_session;
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto& sess_ptr : m_sessions) {
                    if (!sess_ptr.second->write(send_msg, size))
                        err_session.emplace(sess_ptr.first);
                }
                return err_session;
            }

            // 在当前消息尾追加
            // max_package_size: 单个消息最大包长,单次内未发送完毕或者超出该数值,则会分包,等待下次发送
            bool write_tail(SessionID session_id, const char* send_msg, size_t size, size_t max_package_size = 65535) {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write_tail(send_msg, size, max_package_size);
            }

            // 消费掉指定长度的读缓存
            void consume_read_buf(SessionID session_id, size_t bytes_transferred) {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->consume_read_buf(bytes_transferred);
                }
            }

            // 同步关闭连接,注意此时的close_cbk依旧在当前线程下
            void close(SessionID session_id) {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->shutdown();
                }
            }

            // 获取连接者IP
            bool get_ip(SessionID session_id, std::string& ip) const {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    ip = sess_ptr->get_ip();
                    return true;
                }
                return false;
            }

            // 获取连接者port
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
            // 启动监听端口
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

            // 开始监听
            void start_accept() {
                try {
                    TcpSessionPtr session = std::make_shared<TcpSession>(m_ioc_pool.get_io_context(), m_max_wbuffer_size, m_max_rbuffer_size);
                    m_acceptor.async_accept(session->get_socket(), std::bind(&TcpServer::handle_accept, this, std::placeholders::_1, session));
                }
                catch (std::exception&) {
                    stop();
                }
            }

            // 处理接听回调
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

                // 把tcp_session的start的调用交给io_context,由io_context来决定何时执行,可以增加并发度
                session_ptr->get_io_context().dispatch(std::bind(&TcpSession::start, session_ptr));
            }

            // 查找连接对象
            TcpSessionPtr find_session(SessionID session_id) const {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return TcpSessionPtr();
                }
                return iter->second;
            }

            // 删除连接对象
            void remove_session(SessionID session_id) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.erase(session_id);
            }

        private:
            // 关闭连接回调
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
            // 所有连接对象，后期改为内存块，节省开辟/释放内存时间
            TcpSessionMap       m_sessions;
        };
    }
}
