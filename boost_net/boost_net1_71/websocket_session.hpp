/******************************************************************************
File name:  websocket_session.hpp
Author:	    AChar
Purpose:    websocket连接类
Note:       为了外部尽可能的无缓存,外部操作读取数据后需要主动调用consume_read_buf,
            以此来删除读缓存

Special Note: 构造函数中ios_type& ios为外部引用,需要优先释放该对象之后才能释放ios对象
            这就导致外部单独使用使用需要先声明ios对象,然后声明该对象,例如:
                class WebsocketClient{
                    ...
                private:
                    ioc_type                m_ioc;
                    WebsocketSession    m_session;
                };
            当然如果外部主动控制其先后顺序会更好,例如:
                class WebsocketClient {
                public:
                    WebsocketClient(ioc_type& ioc) {
                        m_session = std::make_shared<WebsocketSession>(ioc);
                    }
                    ~WebsocketClient() {
                        m_session.reset();
                    }
                private:
                    std::shared_ptr<WebsocketSession> m_session;
                };
*****************************************************************************/

#pragma once


#include <mutex>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/cast.hpp>

#include "../net_callback.hpp"
#include "../net_buffer.hpp"
#include "../../atomic_switch.hpp"

// 启用自定义beast中的websocket目录下impl目录下的accept.hpp文件
#define USE_SELF_BEAST_WEBSOCKET_ACCEPT_HPP

namespace BTool
{
    namespace BoostNet1_71
    {
        // Websocket连接对象
        class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
        {
        public:
            typedef boost::beast::websocket::stream<boost::beast::tcp_stream>       websocket_stream_type;
            typedef websocket_stream_type::next_layer_type::socket_type             socket_type;
            typedef BoostNet::ReadBuffer                                            ReadBufferType;
            typedef BoostNet::WriteBuffer                                           WriteBufferType;
            typedef WriteBufferType::WriteMemoryStreamPtr                           WriteMemoryStreamPtr;
            typedef BoostNet::NetCallBack::SessionID                                SessionID;

            enum {
                NOLIMIT_WRITE_BUFFER_SIZE = 0, // 无限制
                MAX_WRITE_BUFFER_SIZE = 30000,
                MAX_READSINGLE_BUFFER_SIZE = 20000,
            };

        public:
            // Websocket连接对象
            // ioc: io读写动力服务, 为外部引用, 需要优先释放该对象之后才能释放ioc对象
            // max_wbuffer_size: 最大写缓冲区大小
            // max_rbuffer_size: 单次读取最大缓冲区大小
            WebsocketSession(boost::asio::ip::tcp::socket&& socket, size_t max_wbuffer_size, size_t max_rbuffer_size)
                : m_resolver(socket.get_executor())
                , m_socket(std::move(socket))
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
                , m_current_send_msg(nullptr)
            {
                m_socket.binary(true);
                m_socket.read_message_max(max_rbuffer_size);
            }

            WebsocketSession(boost::asio::io_context& ioc, size_t max_wbuffer_size = MAX_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = MAX_READSINGLE_BUFFER_SIZE)
                : m_socket(boost::asio::make_strand(ioc))
                , m_resolver(boost::asio::make_strand(ioc))
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
                , m_current_send_msg(nullptr)
            {
                m_socket.binary(true);
                m_socket.read_message_max(max_rbuffer_size);
            }

            ~WebsocketSession() {
                shutdown();
            }

            static boost::asio::ip::tcp::endpoint GetEndpointByHost(const char* host, unsigned short port, boost::system::error_code& ec) {
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

            // 设置回调,采用该形式可回调至不同类中分开处理
            WebsocketSession& register_cbk(const BoostNet::NetCallBack& handler) {
                m_handler = handler;
                return *this;
            }
            // 设置开启连接回调
            WebsocketSession& register_open_cbk(const BoostNet::NetCallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // 设置关闭连接回调
            WebsocketSession& register_close_cbk(const BoostNet::NetCallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // 设置读取消息回调
            WebsocketSession& register_read_cbk(const BoostNet::NetCallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // 设置已发送消息回调
            WebsocketSession& register_write_cbk(const BoostNet::NetCallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // 是否已开启
            bool is_open() const {
                return  m_atomic_switch.has_started() && m_socket.is_open();
            }

            // 获取连接ID
            SessionID get_session_id() const {
                return m_session_id;
            }

            // 获取连接者IP
            const std::string& get_ip() const {
                return m_connect_ip;
            }

            // 获取连接者port
            unsigned short get_port() const {
                return m_connect_port;
            }

            // 客户端开启连接,同时开启读取
            void connect(const char* host, unsigned short port, char const* addr = "/") {
                if (!m_atomic_switch.init())
                    return;

                m_connect_ip = host;
                m_connect_port = port;
                m_hand_addr = addr;

                m_resolver.async_resolve(host, std::to_string(port).c_str(),
                    boost::beast::bind_front_handler(&WebsocketSession::handle_resolve, shared_from_this()));
            }

            // 客户端重连
            void reconnect() {
                connect(m_connect_ip.c_str(), m_connect_port, m_hand_addr.c_str());
            }

            // 服务端开启连接,同时开启读取
            void start() {
                if (!m_atomic_switch.init())
                    return;

                m_socket.set_option(
                    boost::beast::websocket::stream_base::timeout::suggested(
                        boost::beast::role_type::server));
                m_socket.set_option(boost::beast::websocket::stream_base::decorator(
                    [](boost::beast::websocket::response_type& res) {
                        res.set(boost::beast::http::field::server,
                            std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
                    }));

                
#ifdef USE_SELF_BEAST_WEBSOCKET_ACCEPT_HPP
                m_socket.async_accept_ex(
                    [this](boost::beast::http::response<boost::beast::http::string_body>& res, const boost::beast::http::request<boost::beast::http::empty_body>& req) {
                        // 阿里云slb增加ssl后转发修改ip地址,原地址在head中X-Forwarded-For字符表示
                        auto real_ip_iter = req.find("X-Forwarded-For");
                        if (real_ip_iter != req.end()) {
                            m_connect_ip = real_ip_iter->value().to_string();
                        }
                        else {
                            real_ip_iter = req.find("X-Real-IP");
                            if (real_ip_iter != req.end()) {
                                m_connect_ip = real_ip_iter->value().to_string();
                            }
                        }
                     },
                    boost::beast::bind_front_handler(&WebsocketSession::handle_start, shared_from_this()));
#else
                m_socket.async_accept(
                    boost::beast::bind_front_handler(
                        &WebsocketSession::handle_start,
                        shared_from_this()));
#endif
            }

            // 同步关闭
            void shutdown() {
                if (!m_atomic_switch.stop())
                    return;

                close();
            }

            // 按顺序写入
            bool write(const char* send_msg, size_t size) {
                if (!m_atomic_switch.has_started()) {
                    return false;
                }

                std::lock_guard<std::recursive_mutex> lock(m_write_mtx);
                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE&& m_write_buf.size() + size > m_max_wbuffer_size) {
                    return false;
                }
                if (!m_write_buf.append(send_msg, size)) {
                    return false;
                }
                // 是否处于发送状态中
                if (m_current_send_msg) {
                    return true;
                }

                write();
                return true;
            }

            // 在当前消息尾追加
            // max_package_size: 单个消息最大包长
            bool write_tail(const char* send_msg, size_t size, size_t max_package_size = 65535) {
                if (!m_atomic_switch.has_started()) {
                    return false;
                }

                std::lock_guard<std::recursive_mutex> lock(m_write_mtx);
                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE&& m_write_buf.size() + size > m_max_wbuffer_size) {
                    return false;
                }
                if (!m_write_buf.append_tail(send_msg, size, max_package_size)) {
                    return false;
                }
                // 是否处于发送状态中
                if (m_current_send_msg) {
                    return true;
                }

                write();
                return true;
            }

            // 消费掉指定长度的读缓存
            void consume_read_buf(size_t bytes_transferred) {
                if (m_atomic_switch.has_stoped()) {
                    return;
                }

                m_read_buf.consume(bytes_transferred);
            }

        protected:
            static SessionID GetNextSessionID() {
                static std::atomic<SessionID> next_session_id(BoostNet::NetCallBack::InvalidSessionID);
                return ++next_session_id;
            }

            socket_type& get_socket() {
                return boost::beast::get_lowest_layer(m_socket).socket();
            }

        private:
            // 异步读
            bool read() {
                try {
                    m_socket.async_read_some(m_read_buf.prepare(m_max_rbuffer_size),
                        boost::beast::bind_front_handler(&WebsocketSession::handle_read, shared_from_this()));
                    return true;
                }
                catch (...) {
                    shutdown();
                    return false;
                }
            }

            // 异步写
            void write() {
                m_current_send_msg = m_write_buf.pop_front();
                m_socket.async_write(boost::asio::buffer(m_current_send_msg->data(), m_current_send_msg->size())
                    , boost::beast::bind_front_handler(&WebsocketSession::handle_write, shared_from_this()));
            }

            void handle_resolve(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& results) {
                if (ec) {
                    close();
                    return;
                }
                boost::beast::get_lowest_layer(m_socket).expires_after(std::chrono::seconds(30));

                boost::beast::get_lowest_layer(m_socket).async_connect(results,
                    boost::beast::bind_front_handler(&WebsocketSession::handle_connect, shared_from_this()));
            }

            // 处理连接回调
            void handle_connect(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type::endpoint_type& end_point)
            {
                if (ec) {
                    close();
                    return;
                }

                boost::beast::get_lowest_layer(m_socket).expires_never();

                if (m_hand_addr.empty())
                    m_hand_addr = "/";

                m_socket.set_option(
                    boost::beast::websocket::stream_base::timeout::suggested(
                        boost::beast::role_type::client));
                
                m_socket.set_option(boost::beast::websocket::stream_base::decorator(
                    [](boost::beast::websocket::request_type& req) {
                        req.set(boost::beast::http::field::user_agent,
                            std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
                    }));

                m_socket.async_handshake(m_connect_ip, m_hand_addr,
                    boost::beast::bind_front_handler(&WebsocketSession::handle_handshake, shared_from_this()));
            }

            void handle_handshake(const boost::beast::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }

                handle_start(ec);
            }

            // 处理开始
            void handle_start(boost::beast::error_code ec) {
                if (ec || !m_atomic_switch.start()) {
                    close();
                    return;
                }

                if (m_connect_ip.empty())
					m_connect_ip = get_socket().remote_endpoint(ec).address().to_string(ec);
                if(m_connect_port == 0)
                    m_connect_port = get_socket().remote_endpoint(ec).port();

                if (read() && m_handler.open_cbk_) {
                    m_handler.open_cbk_(m_session_id);
                }
            }

            // 处理读回调
            void handle_read(const boost::beast::error_code& error, size_t bytes_transferred) {
                if (error) {
                    shutdown();
                    return;
                }

                m_read_buf.commit(bytes_transferred);

                if (m_handler.read_cbk_) {
                    m_handler.read_cbk_(m_session_id, m_read_buf.peek(), m_read_buf.size());
                }
                else {
                    consume_read_buf(bytes_transferred);
                }

                if (m_atomic_switch.has_stoped()) {
                    return;
                }

                read();
            }

            // 处理写回调
            void handle_write(const boost::beast::error_code& ec, size_t bytes_transferred)
            {
                if (ec) {
                    shutdown();
                    return;
                }
                if (m_atomic_switch.has_stoped()) {
                    return;
                }

                if (m_handler.write_cbk_ && m_current_send_msg) {
                    m_handler.write_cbk_(m_session_id, m_current_send_msg->data(), m_current_send_msg->size());
                }

                std::lock_guard<std::recursive_mutex> lock(m_write_mtx);
                m_current_send_msg.reset();
                if (m_write_buf.empty()) {
                    return;
                }
                write();
            }

            void close() {
                boost::beast::error_code ignored_ec;
                get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                m_socket.close(boost::beast::websocket::close_code::normal, ignored_ec);

                m_read_buf.consume(m_read_buf.size());
                {
                    std::lock_guard<std::recursive_mutex> lock(m_write_mtx);
                    m_write_buf.clear();
                }

                m_atomic_switch.reset();

                if (m_handler.close_cbk_) {
                    m_handler.close_cbk_(m_session_id);
                }
            }

        private:
            // TCP解析器
            boost::asio::ip::tcp::resolver m_resolver;
            // asio的socket封装
            websocket_stream_type   m_socket;
            SessionID               m_session_id;

            // 读缓冲
            ReadBufferType          m_read_buf;
            // 最大读缓冲区大小
            size_t                  m_max_rbuffer_size;

            // 写缓存数据保护锁
            std::recursive_mutex    m_write_mtx;
            // 写缓冲
            WriteBufferType         m_write_buf;
            // 当前正在发送的缓存
            WriteMemoryStreamPtr    m_current_send_msg;
            // 最大写缓冲区大小
            size_t                  m_max_wbuffer_size;

            // 回调操作
            BoostNet::NetCallBack   m_handler;

            // 原子启停标志
            AtomicSwitch            m_atomic_switch;

            // 连接者IP
            std::string             m_connect_ip;
            // 连接者Port
            unsigned short          m_connect_port;
            // 地址
            std::string             m_hand_addr;
        };
    }
}