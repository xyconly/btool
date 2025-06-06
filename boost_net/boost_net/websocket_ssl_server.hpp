/*************************************************
File name:      websocket_ssl_server.hpp
Author:			AChar
Version:
Date:
Purpose: 利用boost实现监听服务端口
Note:    server本身存储session对象,外部仅提供ID进行操作
*************************************************/

#pragma once

#include <mutex>
#include <map>
#include "../../io_context_pool.hpp"
#include "../net_callback.hpp"
#include "websocket_ssl_session.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // Websocket服务
        template<bool IsBinary, bool IsReadSome = IsBinary>
        class WebsocketSslServer : public std::enable_shared_from_this<WebsocketSslServer<IsBinary, IsReadSome>>
        {
            typedef AsioContextPool::ioc_type                   ioc_type;
            typedef boost::asio::ip::tcp::acceptor              accept_type;
            typedef WebsocketSslSession<IsBinary, IsReadSome>   WebsocketSslSessionType;
            typedef WebsocketSslServer<IsBinary, IsReadSome>    WebsocketSslServerType;
            typedef std::shared_ptr<WebsocketSslSessionType>    WebsocketSslSessionPtr;
            typedef BoostNet::NetCallBack::SessionID            SessionID;
            typedef std::map<SessionID, WebsocketSslSessionPtr> WebsocketSslSessionMap;
        
        public:
            // Websocket服务
            // handler: session返回回调
            WebsocketSslServer(AsioContextPool& ioc, boost::asio::ssl::context& ctx, size_t max_wbuffer_size = WebsocketSslSessionType::NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = WebsocketSslSessionType::MAX_READSINGLE_BUFFER_SIZE)
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

            // 设置监听错误回调
            WebsocketSslServer& register_error_cbk(const BoostNet::NetCallBack::server_error_cbk& cbk) {
                m_error_handler = cbk;
                return *this;
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(const BoostNet::NetCallBack& handler) {
                m_handler = handler;
            }
            // 设置开启连接回调
            WebsocketSslServer& register_open_cbk(const BoostNet::NetCallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // 设置关闭连接回调
            WebsocketSslServer& register_close_cbk(const BoostNet::NetCallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // 设置读取消息回调
            WebsocketSslServer& register_read_cbk(const BoostNet::NetCallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // 设置已发送消息回调
            WebsocketSslServer& register_write_cbk(const BoostNet::NetCallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // 非阻塞式启动服务
            // ip: 监听IP,支持域名,默认本地IPV4地址
            // port: 监听端口
            // reuse_address: 是否启用端口复用
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

            // 阻塞式启动服务,使用join_all等待
            // ip: 监听IP,支持域名,默认本地IPV4地址
            // port: 监听端口
            // reuse_address: 是否启用端口复用
            void run(unsigned short port, bool reuse_address = true) {
                run(nullptr, port, reuse_address);
            }
            void run(const char* ip, unsigned short port, bool reuse_address = true) {
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
                auto& sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write(send_msg, size);
            }

            // 在当前消息尾追加
            // max_package_size: 单个消息最大包长
            bool write_tail(SessionID session_id, const char* send_msg, size_t size, size_t max_package_size = 65535)
            {
                auto& sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->write_tail(send_msg, size, max_package_size);
            }

            // 消费掉指定长度的读缓存
            void consume_read_buf(SessionID session_id, size_t bytes_transferred)
            {
                auto& sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->consume_read_buf(bytes_transferred);
                }
            }

            // 消费掉指定长度的读缓存
            void close(SessionID session_id)
            {
                auto& sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->shutdown();
                }
            }

            // 获取连接者IP
            bool get_ip(SessionID session_id, std::string& ip) const {
                auto& sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    ip = sess_ptr->get_ip();
                    return true;
                }
                return false;
            }

            // 获取连接者port
            bool get_port(SessionID session_id, unsigned short& port) const {
                auto& sess_ptr = find_session(session_id);
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

            // 开始监听
            void start_accept()
            {
                try {
                    m_acceptor.async_accept(
                        boost::asio::make_strand(m_ioc_pool.get_io_context()),
                        boost::beast::bind_front_handler(&WebsocketSslServerType::handle_accept, this->shared_from_this()));
                }
                catch (std::exception&) {
                    stop();
                    if (m_error_handler)
                        m_error_handler();
                }
            }

            // 处理接听回调
            void handle_accept(const boost::beast::error_code& ec, boost::asio::ip::tcp::socket&& socket)
            {
                start_accept();

                if (ec) {
                    return;
                }

                auto session_ptr = std::make_shared<WebsocketSessionType>(std::move(socket), m_ctx, m_max_wbuffer_size, m_max_rbuffer_size);
                session_ptr->register_cbk(m_handler).register_close_cbk(std::bind(&WebsocketSslServerType::on_close_cbk, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    bool rslt = m_sessions.emplace(session_ptr->get_session_id(), session_ptr).second;
                    if (!rslt)
                        return session_ptr->shutdown();
                }

                session_ptr->start();
            }

            // 查找连接对象
            WebsocketSslSessionPtr& find_session(SessionID session_id)
            {
                static WebsocketSessionPtr s_null = nullptr;
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return WebsocketSslSessionPtr();
                }
                return iter->second;
            }
            const WebsocketSslSessionPtr& find_session(SessionID session_id) const
            {
                static WebsocketSessionPtr s_null = nullptr;
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return WebsocketSslSessionPtr();
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
            virtual void on_close_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred)
            {
                remove_session(session_id);
                if (m_handler.close_cbk_)
                    m_handler.close_cbk_(session_id, msg, bytes_transferred);
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
            // 所有连接对象，后期改为内存块，节省开辟/释放内存时间
            WebsocketSslSessionMap                      m_sessions;
        };
    }
}
