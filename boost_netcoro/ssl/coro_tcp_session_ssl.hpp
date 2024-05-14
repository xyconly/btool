/*************************************************
File name:      coro_tcp_session_ssl.hpp
Author:			AChar
Version:
Date:
Purpose: ����beastʵ�����CoroServerSsl��TCP���Ӷ���
*************************************************/

#pragma once

#ifndef BOOST_COROUTINES_NO_DEPRECATION_WARNING
# define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#endif

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>

#define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR,4)

namespace BTool
{
    namespace BeastCoro
    {
        // TCP���Ӷ���
        class TCPSessionSsl : public std::enable_shared_from_this<TCPSessionSsl>
        {
            // ��������"tcp"������"Keepalive"ѡ������ݽṹ��
            // TCPĬ�ϲ�������Keepalive���ܣ���Ϊ����Keepalive������Ҫ���Ķ���Ŀ����������������΢����������ڰ������ƷѵĻ����������˷��ã�
            // ��һ���棬Keepalive���ò�����ʱ���ܻ���Ϊ���ݵ����粨�����Ͽ�������TCP���ӡ����ң�Ĭ�ϵ�Keepalive��ʱ��Ҫ7,200,000 milliseconds����2Сʱ��̽�����Ϊ5�Ρ�
            struct TcpKeepAliveST
            {
                unsigned long onoff;
                unsigned long keepalivetime;
                unsigned long keepaliveinterval;
            };

            enum {
                BufferLength = 1023,
                QUEUE_MAX_SIZE = 100000,
            };

            typedef std::function<void(const std::shared_ptr<TCPSessionSsl>& session_ptr, const char* msg, std::size_t)>		read_msg_func_t;
            typedef std::function<void(const std::shared_ptr<TCPSessionSsl>& session_ptr)>										disconn_func_t;

        public:
            TCPSessionSsl(boost::asio::ip::tcp::socket& sc, boost::asio::ssl::context& context, boost::asio::yield_context& yield)
                : m_tcp_ssl_socket(sc, context)
                , m_yield(yield)
                , m_read_cbk(nullptr)
                , m_disconnect_cbk(nullptr)
            {
                m_tcp_ssl_socket.set_verify_mode(boost::asio::ssl::verify_none);

                m_local_addr_u = socket().local_endpoint().address().to_v4().to_ulong();
                m_local_addr_str = socket().local_endpoint().address().to_v4().to_string();
                m_local_port = socket().local_endpoint().port();

                m_peer_addr_u = socket().remote_endpoint().address().to_v4().to_ulong();
                m_peer_addr_str = socket().remote_endpoint().address().to_v4().to_string();
                m_peer_port = socket().remote_endpoint().port();
            }

            ~TCPSessionSsl() {}

/**************   ͨ��TCP������Ϣ  ******************/
        public:
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>::lowest_layer_type& socket() {
                return m_tcp_ssl_socket.lowest_layer();
            }

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
            // �����������ѡ�ultime: KeepAlive̽���ʱ����(����)
            bool setkeepalive(unsigned long ultime)
            {
                // ���� KeepAlive
                boost::asio::ip::tcp::socket::keep_alive option(true);
                socket().set_option(option);

#if defined(_WINDOWS_) // WINDOWS��APIʵ��
                // KeepAliveʵ������
                //SOCKET st = reinterpret_cast<SOCKET>(work_socket_.native().as_handle());
                SOCKET st = socket().native_handle();

                TcpKeepAliveST inKeepAlive = { 0 }; //�������
                unsigned long ulInLen = sizeof(TcpKeepAliveST);

                TcpKeepAliveST outKeepAlive = { 0 }; //�������
                unsigned long ulOutLen = sizeof(TcpKeepAliveST);
                unsigned long ulBytesReturn = 0;

                //����socket��keep aliveΪ3��
                inKeepAlive.onoff = 1;
                inKeepAlive.keepaliveinterval = ultime;	//����KeepAlive̽����ʱ����
                inKeepAlive.keepalivetime = 3000;		//��ʼ�״�KeepAlive̽��ǰ��TCP����ʱ��

                if (WSAIoctl(st, SIO_KEEPALIVE_VALS, (LPVOID)&inKeepAlive, ulInLen, (LPVOID)&outKeepAlive, ulOutLen, &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
                {
                    return false;
                }
#elif
                ////KeepAliveʵ�֣���λ��
                int keepAlive = 1;//�趨KeepAlive
                int keepIdle = 3;//��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��
                int keepInterval = ultime / 1000;//����KeepAlive̽����ʱ����
                int keepCount = 3;//�ж��Ͽ�ǰ��KeepAlive̽�����
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

/**************   ���ݽ������Ӧ  ******************/

            void coro_start()
            {
                boost::system::error_code ec;
                m_tcp_ssl_socket.async_handshake(boost::asio::ssl::stream_base::server, m_yield[ec]);

                if (ec == boost::asio::error::operation_aborted)
                    return fail();

                for (;;)
                    if (!coro_read())
                        return;
            }

            // ���ö�ȡ��Ϣ�ص�,Э�̶���
            void setReadMsgCbk(const read_msg_func_t& cbk) {
                m_read_cbk = cbk;
            }
            // ���öϿ����ӻص�,Э�̶���
            void setDisConnectCbk(const disconn_func_t& cbk) {
                m_disconnect_cbk = cbk;
            }

            bool coro_read()
            {
                boost::system::error_code ec;
                m_tcp_ssl_socket.async_read_some(boost::asio::buffer(buffer.data(), BufferLength), m_yield[ec]);

                // This indicates that the session was closed
                if (ec)
                {
                    fail();
                    return false;
                }

                if (m_read_cbk)
                    m_read_cbk(shared_from_this(), buffer.data(), strlen(buffer.data()));

                return true;
            }

            bool coro_write(const char* msg, std::size_t sz)
            {
                boost::system::error_code ec;
                // Echo the message back
                m_tcp_ssl_socket.async_write_some(boost::asio::buffer(msg, sz), m_yield[ec]);

                if (ec)
                {
                    fail();
                    return false;
                }

                return true;
            }

            void fail()
            {
                if (m_disconnect_cbk)
                    m_disconnect_cbk(shared_from_this());
            }


        private:
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>	 m_tcp_ssl_socket;

            boost::asio::yield_context&			m_yield;
            std::array<char, BufferLength + 1>	buffer = { 0 }; // \0λ

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

        };
    }
}