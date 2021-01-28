/*************************************************
File name:      websocket_server.hpp
Author:			AChar
Version:
Date:
Purpose: 利用boost实现监听服务端口
Note:    server本身存储session对象,外部仅提供ID进行操作
*************************************************/

#pragma once

#include <mutex>
#include <map>
#include <boost/lexical_cast.hpp>
#include "../../io_context_pool.hpp"
#include "../net_callback.hpp"
#include "websocket_session.hpp"

namespace BTool
{
    namespace BoostNet1_71
    {
        // Websocket服务
        class WebsocketServer : public std::enable_shared_from_this<WebsocketServer>
        {
            typedef AsioContextPool::ioc_type                   ioc_type;
            typedef boost::asio::ip::tcp::acceptor              accept_type;
            typedef std::shared_ptr<WebsocketSession>           WebsocketSessionPtr;
            typedef BoostNet::NetCallBack::SessionID            SessionID;
            typedef std::map<SessionID, WebsocketSessionPtr>    WebsocketSessionMap;
        
        public:
            // Websocket服务
            // handler: session返回回调
            WebsocketServer(AsioContextPool& ioc, size_t max_wbuffer_size = WebsocketSession::NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = WebsocketSession::MAX_READSINGLE_BUFFER_SIZE)
                : m_ioc_pool(ioc)
                , m_acceptor(ioc.get_io_context())
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
            {
            }

            ~WebsocketServer() {
                stop();
                boost::system::error_code ec;
                m_acceptor.close(ec);
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(const BoostNet::NetCallBack& handler) {
                m_handler = handler;
            }
            // 设置开启连接回调
            WebsocketServer& register_open_cbk(const BoostNet::NetCallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // 设置关闭连接回调
            WebsocketServer& register_close_cbk(const BoostNet::NetCallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // 设置读取消息回调
            WebsocketServer& register_read_cbk(const BoostNet::NetCallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // 设置已发送消息回调
            WebsocketServer& register_write_cbk(const BoostNet::NetCallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // 非阻塞式启动服务
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
            bool write(SessionID session_id, const char* send_msg, size_t size)
            {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write(send_msg, size);
            }

            // 在当前消息尾追加
            // max_package_size: 单个消息最大包长
            bool write_tail(SessionID session_id, const char* send_msg, size_t size, size_t max_package_size = 65535)
            {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write_tail(send_msg, size, max_package_size);
            }

            // 消费掉指定长度的读缓存
            void consume_read_buf(SessionID session_id, size_t bytes_transferred)
            {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->consume_read_buf(bytes_transferred);
                }
            }

            // 消费掉指定长度的读缓存
            void close(SessionID session_id)
            {
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
            // 启动监听端口
            // reuse_address: 是否启用端口复用
            bool start_listen(const char* ip, unsigned short port, bool reuse_address)
            {
                boost::beast::error_code ec;
                boost::asio::ip::tcp::endpoint endpoint;
                if (!ip) {
                    endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
                }
                else {
                    endpoint = WebsocketSession::GetEndpointByHost(ip, port, ec);
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

            // 开始监听
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

            // 处理接听回调
            void handle_accept(const boost::beast::error_code& ec, boost::asio::ip::tcp::socket socket)
            {
                start_accept();

                if (ec) {
                    return;
                }

                auto session_ptr = std::make_shared<WebsocketSession>(std::move(socket), m_max_wbuffer_size, m_max_rbuffer_size);
                session_ptr->register_cbk(m_handler).register_close_cbk(std::bind(&WebsocketServer::on_close_cbk, shared_from_this(), std::placeholders::_1));

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bool rslt = m_sessions.emplace(session_ptr->get_session_id(), session_ptr).second;
                    if (!rslt)
                        return session_ptr->shutdown();
                }

                session_ptr->start();
            }

            // 查找连接对象
            WebsocketSessionPtr find_session(SessionID session_id) const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return WebsocketSessionPtr();
                }
                return iter->second;
            }

            // 删除连接对象
            void remove_session(SessionID session_id) 
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.erase(session_id);
            }

        private:
            // 关闭连接回调
            void on_close_cbk(SessionID session_id)
            {
                remove_session(session_id);
                if (m_handler.close_cbk_)
                    m_handler.close_cbk_(session_id);
            }

        private:
            AsioContextPool&        m_ioc_pool;
            accept_type             m_acceptor;
            BoostNet::NetCallBack   m_handler;
            size_t                  m_max_wbuffer_size;
            size_t                  m_max_rbuffer_size;

            mutable std::mutex      m_mutex;
            // 所有连接对象，后期改为内存块，节省开辟/释放内存时间
            WebsocketSessionMap     m_sessions;
        };
    }
}
