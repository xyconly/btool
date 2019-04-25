/******************************************************************************
File name:  websocket_session.hpp
Author:	    AChar
Purpose:    websocket连接类
Note:       为了外部尽可能的无缓存,外部操作读取数据后需要主动调用consume_read_buf,
            以此来删除读缓存
*****************************************************************************/

#pragma once

#include <mutex>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/cast.hpp>

#include "net_callback.hpp"
#include "net_buffer.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // Websocket连接对象
        class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
        {
//             struct TcpKeepAliveST
//             {
//                 unsigned long onoff;
//                 unsigned long keepalivetime;
//                 unsigned long keepaliveinterval;
//             };

        public:
            typedef boost::beast::websocket::stream<boost::asio::ip::tcp::socket>   WebsocketSocketStream;
            typedef WebsocketSocketStream::lowest_layer_type                        SocketType;
            typedef boost::asio::io_service                                         IosType;
            typedef ReadBuffer                                                      ReadBufferType;
            typedef WriteBuffer                                                     WriteBufferType;
            typedef WriteBuffer::WriteMemoryStreamPtr                               WriteMemoryStreamPtr;
            typedef NetCallBack::SessionID                                          SessionID;

            enum {
                NOLIMIT_WRITE_BUFFER_SIZE = 0, // 无限制
                MAX_WRITE_BUFFER_SIZE = 30000,
                MAX_READSINGLE_BUFFER_SIZE = 20000,
            };

        public:
            // Websocket连接对象
            // ios: io读写动力服务
            // max_buffer_size: 最大写缓冲区大小
            WebsocketSession(IosType& ios, size_t max_wbuffer_size = NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = MAX_READSINGLE_BUFFER_SIZE)
                : m_socket(ios)
                , m_resolver(ios)
                , m_started_flag(false)
                , m_stop_flag(true)
                , m_sending_flag(false)
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
            void register_cbk(NetCallBack* handler) {
                m_handler = handler;
            }

            // 获得socket
            SocketType& get_socket() {
                return m_socket.lowest_layer();
            }

            // 获得io_service
            IosType& get_io_service() {
                return get_socket().get_io_service();
            }

            // 是否已开启
            bool is_open() const {
                return  m_started_flag.load() && m_socket.is_open();
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
            void connect(const char* ip, unsigned short port, char const* addr = "/")
            {
                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                m_host = ip;
                m_hand_addr = addr;

                do_resolve(ip, port);
            }

            // 客户端重连
            void reconnect()
            {
                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                connect(m_host.c_str(), m_connect_port, m_hand_addr.c_str());
            }
            // 服务端开启连接,同时开启读取
            void start()
            {
                bool expected = false;
                if (!m_started_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                m_socket.async_accept(std::bind(&WebsocketSession::handle_start
                    , shared_from_this()
                    , std::placeholders::_1));
            }

            // 同步关闭当前连接
            void shutdown()
            {
                bool expected = false;
                if (!m_stop_flag.compare_exchange_strong(expected, true)) {
                    return;
                }

                boost::system::error_code ignored_ec;
                get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
//                 m_socket.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                m_socket.close(boost::beast::websocket::close_code::normal, ignored_ec);

                m_started_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_close_cbk(m_session_id);
                }
            }

            // 写入
            bool write(const char* send_msg, size_t size)
            {
                if (m_stop_flag.load() || !m_started_flag.load()) {
                    return false;
                }

                m_write_mtx.lock();

                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE && m_write_buf.size() + size > m_max_wbuffer_size) {
                    m_write_mtx.unlock();
                    return false;
                }

                if (!m_write_buf.append(send_msg, size)) {
                    m_write_mtx.unlock();
                    return false;
                }

                bool expected = false;
                // 是否处于发送状态中
                if (!m_sending_flag.compare_exchange_strong(expected, true)) {
                    m_write_mtx.unlock();
                    return true;
                }

                write();
                m_write_mtx.unlock();
                return true;
            }

            // 消费掉指定长度的读缓存
            void consume_read_buf(size_t bytes_transferred)
            {
                if (m_stop_flag.load() || !m_started_flag.load()) {
                    return;
                }

                m_read_buf.consume(bytes_transferred);
            }

            // 设置心跳检测选项；ultime: KeepAlive探测的时间间隔(毫秒)
//             bool setkeepalive(unsigned long ultime)
//             {
//                 // 开启 KeepAlive
//                 boost::asio::ip::tcp::socket::keep_alive option(true);
//                 get_socket().set_option(option);
// 
// #if defined(_WINDOWS_) // WINDOWS的API实现
//                 // KeepAlive实现心跳
//                 //SOCKET st = reinterpret_cast<SOCKET>(work_socket_.native().as_handle());
//                 SOCKET st = get_socket().native_handle();
// 
//                 TcpKeepAliveST inKeepAlive = { 0 }; //输入参数
//                 unsigned long ulInLen = sizeof(TcpKeepAliveST);
// 
//                 TcpKeepAliveST outKeepAlive = { 0 }; //输出参数
//                 unsigned long ulOutLen = sizeof(TcpKeepAliveST);
//                 unsigned long ulBytesReturn = 0;
// 
//                 //设置socket的keep alive为3秒
//                 inKeepAlive.onoff = 1;
//                 inKeepAlive.keepaliveinterval = ultime;	//两次KeepAlive探测间的时间间隔
//                 inKeepAlive.keepalivetime = 3000;		//开始首次KeepAlive探测前的TCP空闲时间
// 
//                 if (WSAIoctl(st, _WSAIOW(IOC_VENDOR, 4), (LPVOID)&inKeepAlive, ulInLen, (LPVOID)&outKeepAlive, ulOutLen, &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
//                 {
//                     return false;
//                 }
// #elif
//                 ////KeepAlive实现，单位秒
//                 int keepAlive = 1;//设定KeepAlive
//                 int keepIdle = 3;//开始首次KeepAlive探测前的TCP空闭时间
//                 int keepInterval = ultime / 1000;//两次KeepAlive探测间的时间间隔
//                 int keepCount = 3;//判定断开前的KeepAlive探测次数
//                 if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, sizeof(keepAlive)) == -1) {
//                     return false;
//                 }
//                 if (setsockopt(s, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle)) == -1) {
//                     return false;
//                 }
//                 if (setsockopt(s, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) == -1) {
//                     return false;
//                 }
//                 if (setsockopt(s, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)) == -1) {
//                     return false;
//                 }
// #endif
//                 return true;
//             }

        protected:
            static SessionID GetNextSessionID()
            {
                static std::atomic<SessionID> next_session_id(NetCallBack::InvalidSessionID);
                ++next_session_id;
                return next_session_id;
            }

        private:
            // 异步读
            void read()
            {
                try {
                    m_socket.async_read_some(m_read_buf.prepare(m_max_rbuffer_size),
                        std::bind(&WebsocketSession::handle_read, shared_from_this(),
                            std::placeholders::_1, std::placeholders::_2));
                }
                catch (...) {
                    shutdown();
                }
            }

            // 异步写
            void write()
            {
                m_current_send_msg = m_write_buf.pop_front();
                if (!m_current_send_msg) {
                    return;
                }

                m_socket.async_write(boost::asio::buffer(m_current_send_msg->data(), m_current_send_msg->size())
                    , std::bind(&WebsocketSession::handle_write, shared_from_this()
                        , std::placeholders::_1
                        , std::placeholders::_2));
            }

            void do_resolve(char const* host, unsigned short port)
            {
                m_resolver.async_resolve({ boost::asio::ip::address::from_string(host), port },
                    std::bind(
                        &WebsocketSession::handle_resolve,
                        shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2));
            }

            void handle_resolve(
                boost::system::error_code ec,
                boost::asio::ip::tcp::resolver::iterator result)
            {
                if (ec)
                {
                    shutdown();
                    return;
                }

                do_connect(result);
            }

            void do_connect(boost::asio::ip::tcp::resolver::iterator result)
            {
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
                    shutdown();
                    return;
                }

                do_handshake();
            }

            void do_handshake()
            {
                if (m_hand_addr.empty())
                    m_hand_addr = "/";

                m_socket.async_handshake(m_host, m_hand_addr,
                    std::bind(&WebsocketSession::handle_handshake
                        , shared_from_this()
                        , std::placeholders::_1));

            }

            void handle_handshake(const boost::system::error_code& ec)
            {
                if (ec)
                {
                    shutdown();
                    return;
                }

                handle_start(ec);
            }

            // 服务端开启连接,同时开启读取
            void handle_start(const boost::system::error_code& ec)
            {
                if (ec) {
                    shutdown();
                    return;
                }

                boost::system::error_code errc;
                m_connect_ip = get_socket().remote_endpoint(errc).address().to_v4().to_string();
                m_connect_port = get_socket().remote_endpoint(errc).port();

                read();

                m_stop_flag.exchange(false);

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }

//                 setkeepalive(1000);
            }

            // 处理读回调
            void handle_read(const boost::system::error_code& error, size_t bytes_transferred)
            {
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

                if (m_stop_flag.load() || !m_started_flag.load()) {
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

                if (m_stop_flag.load() || !m_started_flag.load()) {
                    return;
                }

                if (m_handler && m_current_send_msg) {
                    m_handler->on_write_cbk(m_session_id, m_current_send_msg->data(), m_current_send_msg->size());
                }

                m_current_send_msg.reset();

                m_write_mtx.lock();
                if (m_write_buf.size() == 0) {
                    m_sending_flag.exchange(false);
                    m_write_mtx.unlock();
                    return;
                }

                write();
                m_write_mtx.unlock();
            }

        private:
            std::string             m_host;
            std::string             m_hand_addr;

            // TCP解析器
            boost::asio::ip::tcp::resolver m_resolver;
            // asio的socket封装
            WebsocketSocketStream   m_socket;
            // 连接ID
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
            NetCallBack*            m_handler;

            // 是否已启动
            std::atomic<bool>	    m_started_flag;
            // 是否终止状态
            std::atomic<bool>	    m_stop_flag;

            // 是否正在发送中
            std::atomic<bool>	    m_sending_flag;

            // 连接者IP
            std::string             m_connect_ip;
            // 连接者Port
            unsigned short          m_connect_port;
        };
    }
}