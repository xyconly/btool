/*************************************************
File name:      coro_websocket_session.hpp
Author:			AChar
Version:
Date:
Purpose: ����beastʵ�����CoroServer��websocket���Ӷ���
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
        // ���Ӷ���
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

   /**************   ͨ��������Ϣ  ******************/
        private:
            boost::beast::websocket::stream<boost::asio::ip::tcp::socket>::lowest_layer_type& socket() const {
                return const_cast<WebsocketSession*>(this)->m_socket.lowest_layer();
            }
        public:
            // ���ߺ�������ȡ���ء�Զ�˵������ַ�Ͷ˿ں�
            unsigned long getLocalAddress() const {
                return m_local_addr_u;
            }
            // ���ر��ػ���IP��V4����ַ�ַ���
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
            // �������ӳٷ���ѡ��
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

   /**************   ���ݽ������Ӧ  ******************/
            // ���ö�ȡ��Ϣ�ص�,Э�̶���
            void setReadMsgCbk(const read_msg_func_t& cbk) {
                m_read_cbk = cbk;
            }
            // ���öϿ����ӻص�,Э�̶���
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


   /**************   �ڲ�����  ******************/
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
            // ����������Ϣ
            unsigned long   m_local_addr_u;
            std::string     m_local_addr_str;
            unsigned short  m_local_port;
            // ���Ӷ�����Ϣ
            unsigned long   m_peer_addr_u;
            std::string     m_peer_addr_str;
            unsigned short  m_peer_port;

        private:
            // ��ȡ��Ϣ�ص�
            read_msg_func_t     m_read_cbk;
            // �Ͽ����ӻص�
            disconn_func_t      m_disconnect_cbk;
            // �Ƿ��ѿ���
            std::atomic<bool>   m_bstart;
        };
    }
}