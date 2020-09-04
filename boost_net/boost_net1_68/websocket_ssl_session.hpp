/******************************************************************************
File name:  websocket_ssl_session.hpp
Author:	    AChar
Purpose:    websocket������
Note:       Ϊ���ⲿ�����ܵ��޻���,�ⲿ������ȡ���ݺ���Ҫ��������consume_read_buf,
            �Դ���ɾ��������

Special Note: ���캯����ios_type& iosΪ�ⲿ����,��Ҫ�����ͷŸö���֮������ͷ�ios����
            ��͵����ⲿ����ʹ��ʹ����Ҫ������ios����,Ȼ�������ö���,����:
                class WebsocketClient{
                    ...
                private:
                    ios_type            m_ios;
                    WebsocketSslSession    m_session;
                };
            ��Ȼ����ⲿ�����������Ⱥ�˳������,����:
                class WebsocketClient {
                public:
                    WebsocketClient(ios_type& ios) {
                        m_session = std::make_shared<WebsocketSslSession>(ios);
                    }
                    ~WebsocketClient() {
                        m_session.reset();
                    }
                private:
                    std::shared_ptr<WebsocketSslSession> m_session;
                };
*****************************************************************************/

#pragma once

#include <mutex>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/cast.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "../../atomic_switch.hpp"
#include "../net_callback.hpp"
#include "../net_buffer.hpp"

namespace BTool
{
    namespace BoostNet1_68
    {
        // WebsocketSsl���Ӷ���
        class WebsocketSslSession : public std::enable_shared_from_this<WebsocketSslSession>
        {
        public:
            typedef boost::beast::websocket::stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>   websocket_ssl_stream_type;
            typedef websocket_ssl_stream_type::lowest_layer_type                    socket_type;
            typedef boost::asio::ssl::context                                       ssl_context_type;
            typedef boost::asio::io_service                                         ios_type;
            typedef boost::asio::ssl::stream_base::handshake_type                   ssl_handshake_type;
            typedef BoostNet::ReadBuffer                                            ReadBufferType;
            typedef BoostNet::WriteBuffer                                           WriteBufferType;
            typedef WriteBufferType::WriteMemoryStreamPtr                           WriteMemoryStreamPtr;
            typedef BoostNet::NetCallBack::SessionID                                SessionID;

            enum {
                NOLIMIT_WRITE_BUFFER_SIZE = 0, // ������
                MAX_WRITE_BUFFER_SIZE = 30000,
                MAX_READSINGLE_BUFFER_SIZE = 20000,
            };

        public:
            // Websocket���Ӷ���
            // ios: io��д��������, Ϊ�ⲿ����, ��Ҫ�����ͷŸö���֮������ͷ�ios����
            // max_wbuffer_size: ���д��������С
            // max_rbuffer_size: ���ζ�ȡ��󻺳�����С
            WebsocketSslSession(ios_type& ios, ssl_context_type& ctx, ssl_handshake_type hand_shake_type = ssl_handshake_type::client, size_t max_wbuffer_size = NOLIMIT_WRITE_BUFFER_SIZE, size_t max_rbuffer_size = MAX_READSINGLE_BUFFER_SIZE)
                : m_io_service(ios)
                , m_socket(ios, ctx)
				, m_resolver(ios)
                , m_max_wbuffer_size(max_wbuffer_size)
                , m_max_rbuffer_size(max_rbuffer_size)
                , m_handler(nullptr)
                , m_connect_port(0)
                , m_session_id(GetNextSessionID())
                , m_current_send_msg(nullptr)
                ,m_hand_shake_type(hand_shake_type)
            {
                m_socket.binary(true);
            }

            ~WebsocketSslSession() {
                m_handler = nullptr;
                shutdown();
            }

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            void register_cbk(BoostNet::NetCallBack* handler) {
                m_handler = handler;
            }

            // ���socket
            socket_type& get_socket() {
                return m_socket.lowest_layer();
            }

            // ���io_service
            ios_type& get_io_service() {
                return m_io_service;
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
            void connect(const char* ip, unsigned short port, char const* addr = "/") {
                if (!m_atomic_switch.init())
                    return;

                m_connect_ip = ip;
                m_connect_port = port;
                m_hand_addr = addr;

                m_resolver.async_resolve({ boost::asio::ip::address::from_string(ip), port },
                    std::bind(
                        &WebsocketSslSession::handle_resolve,
                        shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2));
            }

            // �ͻ�������
            void reconnect() {
                connect(m_connect_ip.c_str(), m_connect_port, m_hand_addr.c_str());
            }

            // ����˿�������,ͬʱ������ȡ
            void start() {
                if (!m_atomic_switch.init())
                    return;

                m_socket.async_accept(std::bind(&WebsocketSslSession::handle_start
                    , shared_from_this()
                    , std::placeholders::_1));
            }

            // ͬ���ر�
            void shutdown() {
                if (!m_atomic_switch.stop())
                    return;

                close();
            }

            // ��˳��д��
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
                        std::bind(&WebsocketSslSession::handle_read, shared_from_this(),
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
                m_socket.async_write(boost::asio::buffer(m_current_send_msg->data(), m_current_send_msg->size())
                    , std::bind(&WebsocketSslSession::handle_write, shared_from_this()
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
                    std::bind(&WebsocketSslSession::handle_connect
                        , shared_from_this()
                        , std::placeholders::_1));
            }

            // �������ӻص�
            void handle_connect(const boost::system::error_code& error)
            {
                if (error) {
                    close();
                    return;
                }

                m_socket.next_layer().async_handshake(
                    boost::asio::ssl::stream_base::client,
                    std::bind(&WebsocketSslSession::handle_ssl_handshake
                        , shared_from_this()
                        , std::placeholders::_1));
//                 m_socket.async_handshake(m_connect_ip, m_hand_addr,
//                     std::bind(&WebsocketSslSession::handle_handshake
//                         , shared_from_this()
//                         , std::placeholders::_1));
            }

            void handle_ssl_handshake(const boost::system::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }
                
                if (m_hand_addr.empty())
                    m_hand_addr = "/";

                m_socket.async_handshake(m_connect_ip, m_hand_addr,
                    std::bind(
                        &WebsocketSslSession::handle_handshake,
                        shared_from_this(),
                        std::placeholders::_1));
            }
            void handle_handshake(const boost::system::error_code& ec)
            {
                if (ec) {
                    close();
                    return;
                }

                handle_start(ec);
            }

            // ����ʼ
            void handle_start(const boost::system::error_code& error) {
                boost::system::error_code ec;
                m_connect_ip = get_socket().remote_endpoint(ec).address().to_v4().to_string();
                m_connect_port = get_socket().remote_endpoint(ec).port();

                if (m_atomic_switch.start() && read() && m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }
            }

            // ������ص�
            void handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
                if (error) {
                    auto tmp = error.message();
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
            ssl_handshake_type              m_hand_shake_type;

            // TCP������
            boost::asio::ip::tcp::resolver  m_resolver;
            // asio��socket��װ
            websocket_ssl_stream_type       m_socket;
            ios_type&               m_io_service;
            SessionID               m_session_id;

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
            BoostNet::NetCallBack*  m_handler;

            // ԭ����ͣ��־
            AtomicSwitch            m_atomic_switch;

            // ������IP
            std::string             m_connect_ip;
            // ������Port
            unsigned short          m_connect_port;
			// ��ַ
            std::string             m_hand_addr;
        };
    }
}