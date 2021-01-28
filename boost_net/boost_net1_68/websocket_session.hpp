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
                    ios_type            m_ios;
                    WebsocketSession    m_session;
                };
            当然如果外部主动控制其先后顺序会更好,例如:
                class WebsocketClient {
                public:
                    WebsocketClient(ios_type& ios) {
                        m_session = std::make_shared<WebsocketSession>(ios);
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

namespace BTool
{
    namespace BoostNet1_68
    {
        // Websocket连接对象
        class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
        {
        public:
            typedef boost::beast::websocket::stream<boost::asio::ip::tcp::socket>   websocket_stream_type;
            typedef websocket_stream_type::lowest_layer_type                        socket_type;
            typedef boost::asio::io_service                                         ios_type;
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
            // ios: io读写动力服务, 为外部引用, 需要优先释放该对象之后才能释放ios对象
            // max_wbuffer_size: 最大写缓冲区大小
            // max_rbuffer_size: 单次读取最大缓冲区大小
            WebsocketSession(ios_type& ios, size_t max_wbuffer_size = NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = MAX_READSINGLE_BUFFER_SIZE)
                : m_io_service(ios)
                , m_socket(ios)
				, m_resolver(ios)
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
                , m_current_send_msg(nullptr)
            {
                m_socket.binary(true);
            }

            ~WebsocketSession() {
                m_handler = nullptr;
                shutdown();
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(BoostNet::NetCallBack* handler) {
                m_handler = handler;
            }

            // 获得socket
            socket_type& get_socket() {
                return m_socket.lowest_layer();
            }

            // 获得io_service
            ios_type& get_io_service() {
                return m_io_service;
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
            void connect(const char* ip, unsigned short port, char const* addr = "/") {
                if (!m_atomic_switch.init())
                    return;

                m_connect_ip = ip;
                m_connect_port = port;
                m_hand_addr = addr;

                m_resolver.async_resolve({ boost::asio::ip::address::from_string(ip), port },
                    std::bind(
                        &WebsocketSession::handle_resolve,
                        shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2));
            }

            // 客户端重连
            void reconnect() {
                connect(m_connect_ip.c_str(), m_connect_port, m_hand_addr.c_str());
            }

            // 服务端开启连接,同时开启读取
            void start() {
                if (!m_atomic_switch.init())
                    return;

                m_socket.async_accept(std::bind(&WebsocketSession::handle_start
                    , shared_from_this()
                    , std::placeholders::_1));
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
                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE && m_write_buf.size() + size > m_max_wbuffer_size) {
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
                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE && m_write_buf.size() + size > m_max_wbuffer_size) {
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

        private:
            // 异步读
            bool read() {
                try {
                    m_socket.async_read_some(m_read_buf.prepare(m_max_rbuffer_size),
                        std::bind(&WebsocketSession::handle_read, shared_from_this(),
                            std::placeholders::_1, std::placeholders::_2));
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
                    , std::bind(&WebsocketSession::handle_write, shared_from_this()
                        , std::placeholders::_1
                        , std::placeholders::_2));
            }

            void handle_resolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator result) {
                if (ec) {
                    close();
                    return;
                }

                boost::asio::async_connect(
                    get_socket(),
                    result,
                    std::bind(&WebsocketSession::handle_connect
                        , shared_from_this()
                        , std::placeholders::_1));
            }

            // 处理连接回调
            void handle_connect(const boost::system::error_code& error)
            {
                if (error) {
                    close();
                    return;
                }

                if (m_hand_addr.empty())
                    m_hand_addr = "/";

                m_socket.async_handshake(m_connect_ip, m_hand_addr,
                    std::bind(&WebsocketSession::handle_handshake
                        , shared_from_this()
                        , std::placeholders::_1));
            }

            void handle_handshake(const boost::system::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }

                handle_start(ec);
            }

            // 处理开始
            void handle_start(const boost::system::error_code& error) {
                boost::system::error_code ec;
                m_connect_ip = get_socket().remote_endpoint(ec).address().to_v4().to_string();
                m_connect_port = get_socket().remote_endpoint(ec).port();

                if (m_atomic_switch.start() && read() && m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }
            }

            // 处理读回调
            void handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
                if (error) {
                    shutdown();
                    return;
                }

                m_read_buf.commit(bytes_transferred);

                if (m_handler) {
                    m_handler->on_read_cbk(m_session_id, m_read_buf.peek(), m_read_buf.size());
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
            void handle_write(const boost::system::error_code& ec, size_t bytes_transferred)
            {
                if (ec) {
                    shutdown();
                    return;
                }
                if (m_atomic_switch.has_stoped()) {
                    return;
                }

                if (m_handler && m_current_send_msg) {
                    m_handler->on_write_cbk(m_session_id, m_current_send_msg->data(), m_current_send_msg->size());
                }

                std::lock_guard<std::recursive_mutex> lock(m_write_mtx);
                m_current_send_msg.reset();
                if (m_write_buf.empty()) {
                    return;
                }
                write();
            }

            void close() {
                boost::system::error_code ignored_ec;
                get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                m_socket.close(boost::beast::websocket::close_code::normal, ignored_ec);

                m_read_buf.consume(m_read_buf.size());
                {
                    std::lock_guard<std::recursive_mutex> lock(m_write_mtx);
                    m_write_buf.clear();
                }

                m_atomic_switch.reset();

                if (m_handler) {
                    m_handler->on_close_cbk(m_session_id);
                }
            }

        private:
            // TCP解析器
            boost::asio::ip::tcp::resolver m_resolver;
            // asio的socket封装
            websocket_stream_type   m_socket;
            ios_type&               m_io_service;
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
            BoostNet::NetCallBack*  m_handler;

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