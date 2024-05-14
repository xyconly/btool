/*************************************************
File name:      coro_tcp_session.hpp
Author:			AChar
Version:
Date:
Purpose: ����beastʵ�����CoroServer��TCP���Ӷ���
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
        // TCP���Ӷ���
        class TCPSession : public std::enable_shared_from_this<TCPSession>
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

/**************   ͨ��TCP������Ϣ  ******************/
            boost::asio::ip::tcp::socket& get_socket() const {
                return const_cast<TCPSession*>(this)->m_socket;
            }
            // ���ߺ�������ȡ���ء�Զ�˵������ַ�Ͷ˿ں�
            unsigned long getLocalAddress() const {
                return m_socket.local_endpoint().address().to_v4().to_ulong();
            }
            // ���ر��ػ���IP��V4����ַ�ַ���
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

            // �������ӳٷ���ѡ��
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
            // �����������ѡ�ultime: KeepAlive̽���ʱ����(����)
            bool setkeepalive(unsigned long ultime)
            {
                // ���� KeepAlive
                boost::asio::ip::tcp::socket::keep_alive option(true);
                m_socket.set_option(option);

#if defined(_WINDOWS_) // WINDOWS��APIʵ��
                // KeepAliveʵ������
                //SOCKET st = reinterpret_cast<SOCKET>(work_socket_.native().as_handle());
                SOCKET st = m_socket.native_handle();

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


/**************   �ڲ�����  ******************/
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

        private:
            boost::asio::ip::tcp::socket&	m_socket;
            boost::asio::yield_context&		m_yield;
            std::array<char, BufferLength + 1> m_buffer = { 0 }; // \0λ


                                                               // ��ȡ��Ϣ�ص�
            read_msg_func_t					m_read_cbk;
            // �Ͽ����ӻص�
            disconn_func_t					m_disconnect_cbk;

        };
    }
}