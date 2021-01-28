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

#include "../../io_service_pool.hpp"
#include "../net_callback.hpp"
#include "websocket_session.hpp"

namespace BTool
{
    namespace BoostNet1_68
    {
        // Websocket服务
        class WebsocketServer : public BoostNet::NetCallBack
        {
            typedef AsioServicePool::ios_type                   ios_type;
            typedef boost::asio::ip::tcp::acceptor              accept_type;
            typedef std::shared_ptr<WebsocketSession>           WebsocketSessionPtr;
            typedef std::map<SessionID, WebsocketSessionPtr>    WebsocketSessionMap;
        
        public:
            // Websocket服务
            // handler: session返回回调
            WebsocketServer(AsioServicePool& ios, size_t max_wbuffer_size = WebsocketSession::NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = WebsocketSession::MAX_READSINGLE_BUFFER_SIZE)
                : m_ios_pool(ios)
                , m_acceptor(ios.get_io_service())
                , m_handler(nullptr)
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
            {
            }

            ~WebsocketServer() {
                m_handler = nullptr;
                stop();
                boost::system::error_code ec;
                m_acceptor.close(ec);
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(BoostNet::NetCallBack* handler)
            {
                m_handler = handler;
            }

            // 非阻塞式启动服务
            // ip: 监听IP,默认本地IPV4地址
            // port: 监听端口
            // reuse_address: 是否启用端口复用
            bool start(unsigned short port, bool reuse_address = false) {
                return start(nullptr, port, reuse_address);
            }
            bool start(const char* ip, unsigned short port, bool reuse_address = false)
            {
                if (!start_listen(ip, port, reuse_address)) {
                    return false;
                }
                m_ios_pool.start();
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
                m_ios_pool.run();
            }

            // 终止当前服务
            void stop() {
                clear();
                m_ios_pool.stop();
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
                    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(reuse_address));
                    m_acceptor.bind(endpoint);
                    m_acceptor.listen();
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
                    WebsocketSessionPtr session = std::make_shared<WebsocketSession>(m_ios_pool.get_io_service(), m_max_wbuffer_size, m_max_rbuffer_size);
                    m_acceptor.async_accept(session->get_socket(), bind(&WebsocketServer::handle_accept, this, boost::placeholders::_1, session));
                }
                catch (std::exception&) {
                    stop();
                }
            }

            // 处理接听回调
            void handle_accept(const boost::system::error_code& ec, const WebsocketSessionPtr& session_ptr)
            {
                start_accept();
                if (ec) {
                    session_ptr->shutdown();
                    return;
                }

                session_ptr->register_cbk(this);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bool rslt = m_sessions.emplace(session_ptr->get_session_id(), session_ptr).second;
                    if (!rslt)
                        return session_ptr->shutdown();
                }

//                 session_ptr->start();

                // 把tcp_session的start的调用交给io_service,由io_service来决定何时执行,可以增加并发度
                session_ptr->get_io_service().dispatch(boost::bind(&WebsocketSession::start, session_ptr));
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
            // 开启连接回调
            virtual void on_open_cbk(SessionID session_id) override
            {
                if(m_handler)
                    m_handler->on_open_cbk(session_id);
            }
            // 关闭连接回调
            virtual void on_close_cbk(SessionID session_id) override
            {
                remove_session(session_id);
                if (m_handler)
                    m_handler->on_close_cbk(session_id);
            }
            // 读取消息回调
            virtual void on_read_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) override
            {
                if (m_handler)
                    m_handler->on_read_cbk(session_id, msg, bytes_transferred);
            }
            // 已发送消息回调
            virtual void on_write_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) override
            {
                if (m_handler)
                    m_handler->on_write_cbk(session_id, msg, bytes_transferred);
            }

        private:
            AsioServicePool&        m_ios_pool;
            accept_type             m_acceptor;
            BoostNet::NetCallBack*  m_handler;
            size_t                  m_max_wbuffer_size;
            size_t                  m_max_rbuffer_size;

            mutable std::mutex      m_mutex;
            // 所有连接对象，后期改为内存块，节省开辟/释放内存时间
            WebsocketSessionMap     m_sessions;
        };
    }
}
