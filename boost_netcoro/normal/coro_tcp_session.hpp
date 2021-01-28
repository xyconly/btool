/*************************************************
File name:      coro_tcp_session.hpp
Author:			AChar
Version:
Date:
Purpose: 利用beast实现配合CoroServer的TCP连接对象
*************************************************/

#pragma once

#ifndef BOOST_COROUTINES_NO_DEPRECATION_WARNING
# define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#endif

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR,4)

namespace BTool
{
    namespace BeastCoro
    {
        // TCP连接对象
        class TCPSession : public std::enable_shared_from_this<TCPSession>
        {
            // 用以设置"tcp"连接中"Keepalive"选项的数据结构；
            // TCP默认并不开启Keepalive功能，因为开启Keepalive功能需要消耗额外的宽带和流量，尽管这微不足道，但在按流量计费的环境下增加了费用，
            // 另一方面，Keepalive设置不合理时可能会因为短暂的网络波动而断开健康的TCP连接。并且，默认的Keepalive超时需要7,200,000 milliseconds，即2小时，探测次数为5次。
            struct TcpKeepAliveST
            {
                unsigned long onoff;
                unsigned long keepalivetime;
                unsigned long keepaliveinterval;
            };

            enum {
                BufferLength = 8191,
                QUEUE_MAX_SIZE = 100000,
            };


            typedef std::function<void(const std::shared_ptr<TCPSession>& session_ptr, const char* msg, std::size_t)>		read_msg_func_t;
            typedef std::function<void(const std::shared_ptr<TCPSession>& session_ptr)>										disconn_func_t;

        public:
            TCPSession(boost::asio::ip::tcp::socket& dev, boost::asio::yield_context& yield)
                : m_socket(dev)
                , m_yield(yield)
                , m_read_cbk(nullptr)
                , m_disconnect_cbk(nullptr)
            {
                setkeepalive(10000);
            }

            ~TCPSession() {}

#pragma region 通用TCP连接信息
            boost::asio::ip::tcp::socket& get_socket() const {
                return const_cast<TCPSession*>(this)->m_socket;
            }
            // 工具函数：获取本地、远端的网络地址和端口号
            unsigned long getLocalAddress() const {
                return m_socket.local_endpoint().address().to_v4().to_ulong();
            }
            // 返回本地机器IP（V4）地址字符串
            std::string getLocalAddress_str() const {
                return m_socket.local_endpoint().address().to_v4().to_string();
            }
            unsigned short getLocalPort() const {
                return m_socket.local_endpoint().port();
            }
            unsigned long getPeerAddress() const {
                return m_socket.remote_endpoint().address().to_v4().to_ulong();
            }
            std::string getPeerAddress_str() const {
                return m_socket.remote_endpoint().address().to_v4().to_string();
            }
            unsigned short getPeerPort() const {
                return m_socket.remote_endpoint().port();
            }

            // 设置无延迟发送选项
            bool setnodelay(bool isnodelay, std::string& errmsg)
            {
                boost::asio::ip::tcp::no_delay nodelay(isnodelay);
                boost::system::error_code ec;
                m_socket.set_option(nodelay, ec);
                if (ec)
                {
                    errmsg = ec.message();
                    return false;
                }
                return true;
            }
            // 设置心跳检测选项；ultime: KeepAlive探测的时间间隔(毫秒)
            bool setkeepalive(unsigned long ultime)
            {
                // 开启 KeepAlive
                boost::asio::ip::tcp::socket::keep_alive option(true);
                m_socket.set_option(option);

#if defined(_WINDOWS_) // WINDOWS的API实现
                // KeepAlive实现心跳
                //SOCKET st = reinterpret_cast<SOCKET>(work_socket_.native().as_handle());
                SOCKET st = m_socket.native_handle();

                TcpKeepAliveST inKeepAlive = { 0 }; //输入参数
                unsigned long ulInLen = sizeof(TcpKeepAliveST);

                TcpKeepAliveST outKeepAlive = { 0 }; //输出参数
                unsigned long ulOutLen = sizeof(TcpKeepAliveST);
                unsigned long ulBytesReturn = 0;

                //设置socket的keep alive为3秒
                inKeepAlive.onoff = 1;
                inKeepAlive.keepaliveinterval = ultime;	//两次KeepAlive探测间的时间间隔
                inKeepAlive.keepalivetime = 3000;		//开始首次KeepAlive探测前的TCP空闲时间

                if (WSAIoctl(st, SIO_KEEPALIVE_VALS, (LPVOID)&inKeepAlive, ulInLen, (LPVOID)&outKeepAlive, ulOutLen, &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
                {
                    return false;
                }
#elif
                ////KeepAlive实现，单位秒
                int keepAlive = 1;//设定KeepAlive
                int keepIdle = 3;//开始首次KeepAlive探测前的TCP空闭时间
                int keepInterval = ultime / 1000;//两次KeepAlive探测间的时间间隔
                int keepCount = 3;//判定断开前的KeepAlive探测次数
                if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, sizeof(keepAlive)) == -1) {
                    return false;
                }
                if (setsockopt(s, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle)) == -1) {
                    return false;
                }
                if (setsockopt(s, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) == -1) {
                    return false;
                }
                if (setsockopt(s, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)) == -1) {
                    return false;
                }
#endif
                return true;
            }
#pragma endregion

#pragma region 数据解析与回应
            // 设置读取消息回调,协程动力
            void setReadMsgCbk(const read_msg_func_t& cbk) {
                m_read_cbk = cbk;
            }
            // 设置断开连接回调,协程动力
            void setDisConnectCbk(const disconn_func_t& cbk) {
                m_disconnect_cbk = cbk;
            }

            void coro_start()
            {
                for (;;)
                {
                    // Read a message
                    if (!coro_read())
                        return;
                }
            }

            bool coro_write(const char* msg, std::size_t sz)
            {
                boost::system::error_code ec;
                // Echo the message back
                m_socket.async_write_some(boost::asio::buffer(msg, sz), m_yield[ec]);
                if (ec)
                {
                    fail();
                    return false;
                }

                return true;
            }

#pragma endregion

#pragma region 内部函数
        private:
            bool coro_read()
            {
                boost::system::error_code ec;
                m_socket.async_read_some(boost::asio::buffer(m_buffer.data(), BufferLength), m_yield[ec]);

                // This indicates that the session was closed
                if (ec)
                {
                    fail();
                    return false;
                }

                if (m_read_cbk)
                    m_read_cbk(shared_from_this(), m_buffer.data(), strlen(m_buffer.data()));

                return true;
            }

            void fail()
            {
                if (m_disconnect_cbk)
                    m_disconnect_cbk(shared_from_this());
            }
#pragma endregion

        private:
            boost::asio::ip::tcp::socket&	m_socket;
            boost::asio::yield_context&		m_yield;
            std::array<char, BufferLength + 1> m_buffer = { 0 }; // \0位


                                                               // 读取消息回调
            read_msg_func_t					m_read_cbk;
            // 断开连接回调
            disconn_func_t					m_disconnect_cbk;

        };
    }
}