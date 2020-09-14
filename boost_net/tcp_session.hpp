/******************************************************************************
File name:  tcp_session.hpp
Author:	    AChar
Purpose:    tcp������
Note:       Ϊ���ⲿ�����ܵ��޻���,�ⲿ������ȡ���ݺ���Ҫ��������consume_read_buf,
            �Դ���ɾ��������

Special Note: ���캯����ioc_type& iosΪ�ⲿ����,��Ҫ�����ͷŸö���֮������ͷ�ios����
            ��͵����ⲿ����ʹ��ʹ����Ҫ������ios����,Ȼ�������ö���,����:
                class TcpClient{
                    ...
                private:
                    ioc_type                    m_ios;
                    std::shared_ptr<TcpSession> m_session;
                };
            ��Ȼ����ⲿ�����������Ⱥ�˳������,����:
                class TcpClient {
                public:
                    TcpClient(ioc_type& ios) {
                        m_session = std::make_shared<TcpSession>(ios);
                    }
                    ~TcpClient() {
                        m_session.reset();
                    }
                private:
                    std::shared_ptr<TcpSession> m_session;
                };
*****************************************************************************/

#pragma once

#include <mutex>
#include <string>
#include <boost/asio.hpp>
#include "net_callback.hpp"
#include "net_buffer.hpp"
#include "../atomic_switch.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // TCP���Ӷ���
        class TcpSession : public std::enable_shared_from_this<TcpSession>
        {
        public:
            typedef boost::asio::ip::tcp::socket        socket_type;
            typedef boost::asio::io_context             ioc_type;
            typedef ReadBuffer                          ReadBufferType;
            typedef WriteBuffer                         WriteBufferType;
            typedef WriteBuffer::WriteMemoryStreamPtr   WriteMemoryStreamPtr;
            typedef NetCallBack::SessionID              SessionID;

            enum {
                NOLIMIT_WRITE_BUFFER_SIZE = 0, // ������
                MAX_WRITE_BUFFER_SIZE = 30000,
                MAX_READSINGLE_BUFFER_SIZE = 2000,
            };

        public:
            // TCP���Ӷ���,Ĭ�϶��з���ģʽ,��ͨ��set_only_one_mode����Ϊ��������ģʽ
            // ios: io��д��������, Ϊ�ⲿ����, ��Ҫ�����ͷŸö���֮������ͷ�ios����
            // max_buffer_size: ���д��������С
            // max_rbuffer_size: ���ζ�ȡ��󻺳�����С
            TcpSession(ioc_type& ios, size_t max_wbuffer_size = NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = MAX_READSINGLE_BUFFER_SIZE)
                : m_io_context(ios)
                , m_socket(ios)
                , m_overtime_timer(ios)
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
                , m_current_send_msg(nullptr)
            {
            }

            ~TcpSession() {
                m_handler = nullptr;
                m_overtime_timer.cancel();
                close();
            }

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(NetCallBack* handler) {
                m_handler = handler;
            }

            // ���socket
            socket_type& get_socket() {
                return m_socket;
            }

            // ���io_context
            ioc_type& get_io_context() {
                return m_io_context;
            }

            // �Ƿ��ѿ���
            bool is_open() const {
                return  m_atomic_switch.has_started() && m_socket.is_open();
            }

            // ��ȡ����ID
            SessionID get_session_id() const {
                return m_session_id;
            }

            // ��ȡ������IP
            const std::string& get_ip() const {
                return m_connect_ip;
            }

            // ��ȡ������port
            unsigned short get_port() const {
                return m_connect_port;
            }

            // �ͻ��˿�������,ͬʱ������ȡ
            void connect(const char* ip, unsigned short port) {
                if (!m_atomic_switch.init())
                    return;

                m_connect_ip = ip;
                m_connect_port = port;

                m_overtime_timer.cancel();
                m_overtime_timer.expires_from_now(boost::posix_time::milliseconds(2000));

                m_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(ip), port)
                                    ,std::bind(&TcpSession::handle_connect, shared_from_this(), std::placeholders::_1));

                m_overtime_timer.async_wait(std::bind(&TcpSession::handle_overtimer, shared_from_this(), std::placeholders::_1));
            }

            // �ͻ��˿�������,ͬʱ������ȡ
            void reconnect() {
                connect(m_connect_ip.c_str(), m_connect_port);
            }

            // ����˿�������,ͬʱ������ȡ
            void start()  {
                if (!m_atomic_switch.init())
                    return;

                handle_start();
            }

            // ͬ���ر�
            void shutdown()
            {
                if (!m_atomic_switch.stop())
                    return;
                close();
            }

            // д��
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
                // �Ƿ��ڷ���״̬��
                if (m_current_send_msg) {
                    return true;
                }

                write();
                return true;
            }

            // �ڵ�ǰ��Ϣβ׷��
            // max_package_size: ������Ϣ������
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
                // �Ƿ��ڷ���״̬��
                if (m_current_send_msg) {
                    return true;
                }

                write();
                return true;
            }

            // ���ѵ�ָ�����ȵĶ�����
            void consume_read_buf(size_t bytes_transferred) {
                if (m_atomic_switch.has_stoped()) {
                    return;
                }

                m_read_buf.consume(bytes_transferred);
            }

        protected:
            static SessionID GetNextSessionID() {
                static std::atomic<SessionID> next_session_id(NetCallBack::InvalidSessionID);
                return ++next_session_id;
            }

        private:
            // �첽��
            bool read() {
                try {
                    m_socket.async_read_some(m_read_buf.prepare(m_max_rbuffer_size),
                        std::bind(&TcpSession::handle_read, shared_from_this(),
                            std::placeholders::_1, std::placeholders::_2));
                    return true;
                }
                catch (...) {
                    shutdown();
                    return false;
                }
            }

            // �첽д
            void write() {
                m_current_send_msg = m_write_buf.pop_front();
                boost::asio::async_write(m_socket, boost::asio::buffer(m_current_send_msg->data(), m_current_send_msg->size())
                    , std::bind(&TcpSession::handle_write, shared_from_this()
                        , std::placeholders::_1
                        , std::placeholders::_2));
            }

            // �������ӻص�
            void handle_connect(const boost::system::error_code& error) {
                m_overtime_timer.cancel();

                if (error) {
                    close();
                    return;
                }

                handle_start();
            }

            // ����ʼ
            void handle_start() {
                boost::system::error_code ec;
                if (m_connect_ip.empty())
					m_connect_ip = m_socket.remote_endpoint(ec).address().to_string(ec);
                if(m_connect_port == 0)
                    m_connect_port = m_socket.remote_endpoint(ec).port();

                if (m_atomic_switch.start() && read() && m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }
            }

            // ������ص�
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

            // ����д�ص�
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

            void handle_overtimer(boost::system::error_code ec) {
                if (!ec) {
                    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    m_socket.close(ec);
                }
            }

            void close() {
                boost::system::error_code ignored_ec;
                m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                m_socket.close(ignored_ec);

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
            // asio��socket��װ
            socket_type             m_socket;
            ioc_type&               m_io_context;
            SessionID               m_session_id;

            boost::asio::deadline_timer m_overtime_timer;

            // ������
            ReadBufferType          m_read_buf;
            // ������������С
            size_t                  m_max_rbuffer_size;

            // д�������ݱ�����
            std::recursive_mutex    m_write_mtx;
            // д����
            WriteBufferType         m_write_buf;
            // ��ǰ���ڷ��͵Ļ���
            WriteMemoryStreamPtr    m_current_send_msg;
            // ���д��������С
            size_t                  m_max_wbuffer_size;

            // �ص�����
            NetCallBack*            m_handler;

            // ԭ����ͣ��־
            AtomicSwitch            m_atomic_switch;

            // ������IP
            std::string             m_connect_ip;
            // ������Port
            unsigned short          m_connect_port;
        };
    }
}