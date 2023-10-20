/*************************************************
File name:      coro_websocket_session.hpp
Author:			AChar
Version:
Date:
Purpose: 利用beast实现配合CoroServer的websocket连接对象
*************************************************/

#pragma once

#ifndef BOOST_COROUTINES_NO_DEPRECATION_WARNING
# define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#endif

#include <sstream>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

namespace BTool
{
    namespace BeastCoro
    {
        // 连接对象
        class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
        {
            enum {
                BufferLength = 8191,
                QUEUE_MAX_SIZE = 100000,
            };

            typedef std::function<void(const std::shared_ptr<WebsocketSession>& session_ptr, const char* msg, std::size_t)>		read_msg_func_t;
            typedef std::function<void(const std::shared_ptr<WebsocketSession>& session_ptr)>										disconn_func_t;

        public:
            WebsocketSession(boost::asio::ip::tcp::socket& dev, boost::asio::yield_context& yield)
                : m_socket(std::move(dev))
                , m_yield(yield)
                , m_read_cbk(nullptr)
                , m_disconnect_cbk(nullptr)
                , m_bstart(false)
            {
                m_local_addr_u = socket().local_endpoint().address().to_v4().to_ulong();
                m_local_addr_str = socket().local_endpoint().address().to_v4().to_string();
                m_local_port = socket().local_endpoint().port();

                m_peer_addr_u = socket().remote_endpoint().address().to_v4().to_ulong();
                m_peer_addr_str = socket().remote_endpoint().address().to_v4().to_string();
                m_peer_port = socket().remote_endpoint().port();
            }

            ~WebsocketSession() {}

   /**************   通用连接信息  ******************/
        private:
            boost::beast::websocket::stream<boost::asio::ip::tcp::socket>::lowest_layer_type& socket() const {
                return const_cast<WebsocketSession*>(this)->m_socket.lowest_layer();
            }
        public:
            // 工具函数：获取本地、远端的网络地址和端口号
            unsigned long getLocalAddress() const {
                return m_local_addr_u;
            }
            // 返回本地机器IP（V4）地址字符串
            std::string getLocalAddress_str() const {
                return m_local_addr_str;
            }
            unsigned short getLocalPort() const {
                return m_local_port;
            }
            unsigned long getPeerAddress() const {
                return m_peer_addr_u;
            }
            std::string getPeerAddress_str() const {
                return m_peer_addr_str;
            }
            unsigned short getPeerPort() const {
                return m_peer_port;
            }
            // 设置无延迟发送选项
            bool setnodelay(bool isnodelay, std::string& errmsg)
            {
                boost::asio::ip::tcp::no_delay nodelay(isnodelay);
                boost::system::error_code ec;
                socket().set_option(nodelay, ec);
                if (ec)
                {
                    errmsg = ec.message();
                    return false;
                }
                return true;
            }

   /**************   数据解析与回应  ******************/
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
                boost::system::error_code ec;
                m_socket.async_accept(m_yield[ec]);
                if (ec)
                    return fail();

                m_bstart = true;
                for (;;)
                {
                    // Read a message
                    if (!coro_read())
                        return;
                }

            }

            bool write(const char* msg, std::size_t sz)
            {
                if (!m_bstart)
                    return true;

                try
                {
                    m_socket.write(boost::asio::buffer(msg, sz));
                }
                catch (...)
                {
                    fail();
                    return false;
                }
                return true;
            }

            bool coro_write(const char* msg, std::size_t sz)
            {
                boost::system::error_code ec;
                // Echo the message back
                m_socket.async_write(boost::asio::buffer(msg, sz), m_yield[ec]);

                if (ec)
                {
                    fail();
                    return false;
                }

                return true;
            }


   /**************   内部函数  ******************/
        private:
            bool coro_read()
            {
                boost::system::error_code ec;
                boost::beast::multi_buffer buffer;
                m_socket.async_read(buffer, m_yield[ec]);

                // This indicates that the session was closed
                if (ec/* == boost::beast::websocket::error::closed*/)
                {
                    fail();
                    return false;
                }

                std::stringstream read_msg;
                read_msg << boost::beast::buffers(buffer.data());

                if (m_read_cbk)
                    m_read_cbk(shared_from_this(), read_msg.str().c_str(), buffer.size());

                return true;
            }

            void fail()
            {
                boost::system::error_code ec;
                if (m_disconnect_cbk)
                    m_disconnect_cbk(shared_from_this());
            }

        private:
            boost::beast::websocket::stream<boost::asio::ip::tcp::socket>	m_socket;

            boost::asio::yield_context&         m_yield;

        private:
            // 本地连接信息
            unsigned long   m_local_addr_u;
            std::string     m_local_addr_str;
            unsigned short  m_local_port;
            // 连接对象信息
            unsigned long   m_peer_addr_u;
            std::string     m_peer_addr_str;
            unsigned short  m_peer_port;

        private:
            // 读取消息回调
            read_msg_func_t     m_read_cbk;
            // 断开连接回调
            disconn_func_t      m_disconnect_cbk;
            // 是否已开启
            std::atomic<bool>   m_bstart;
        };
    }
}