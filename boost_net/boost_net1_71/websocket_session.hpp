/******************************************************************************
File name:  websocket_session.hpp
Author:	    AChar
Purpose:    websocket������
Note:       Ϊ���ⲿ�����ܵ��޻���,�ⲿ������ȡ���ݺ���Ҫ��������consume_read_buf,
            �Դ���ɾ��������

Special Note: ���캯����ios_type& iosΪ�ⲿ����,��Ҫ�����ͷŸö���֮������ͷ�ios����
            ��͵����ⲿ����ʹ��ʹ����Ҫ������ios����,Ȼ�������ö���,����:
                class WebsocketClient{
                    ...
                private:
                    ioc_type                m_ioc;
                    WebsocketSession    m_session;
                };
            ��Ȼ����ⲿ�����������Ⱥ�˳������,����:
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

// �����Զ���beast�е�websocketĿ¼��implĿ¼�µ�accept.hpp�ļ�
#define USE_SELF_BEAST_WEBSOCKET_ACCEPT_HPP

namespace BTool
{
    namespace BoostNet1_71
    {
        // Websocket���Ӷ���
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
                NOLIMIT_WRITE_BUFFER_SIZE = 0, // ������
                MAX_WRITE_BUFFER_SIZE = 30000,
                MAX_READSINGLE_BUFFER_SIZE = 20000,
            };

        public:
            // Websocket���Ӷ���
            // ioc: io��д��������, Ϊ�ⲿ����, ��Ҫ�����ͷŸö���֮������ͷ�ioc����
            // max_wbuffer_size: ���д��������С
            // max_rbuffer_size: ���ζ�ȡ��󻺳�����С
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

            // ���ûص�,���ø���ʽ�ɻص�����ͬ���зֿ�����
            WebsocketSession& register_cbk(const BoostNet::NetCallBack& handler) {
                m_handler = handler;
                return *this;
            }
            // ���ÿ������ӻص�
            WebsocketSession& register_open_cbk(const BoostNet::NetCallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // ���ùر����ӻص�
            WebsocketSession& register_close_cbk(const BoostNet::NetCallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // ���ö�ȡ��Ϣ�ص�
            WebsocketSession& register_read_cbk(const BoostNet::NetCallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // �����ѷ�����Ϣ�ص�
            WebsocketSession& register_write_cbk(const BoostNet::NetCallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
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
            void connect(const char* host, unsigned short port, char const* addr = "/") {
                if (!m_atomic_switch.init())
                    return;

                m_connect_ip = host;
                m_connect_port = port;
                m_hand_addr = addr;

                m_resolver.async_resolve(host, std::to_string(port).c_str(),
                    boost::beast::bind_front_handler(&WebsocketSession::handle_resolve, shared_from_this()));
            }

            // �ͻ�������
            void reconnect() {
                connect(m_connect_ip.c_str(), m_connect_port, m_hand_addr.c_str());
            }

            // ����˿�������,ͬʱ������ȡ
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
                        // ������slb����ssl��ת���޸�ip��ַ,ԭ��ַ��head��X-Forwarded-For�ַ���ʾ
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
                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE&& m_write_buf.size() + size > m_max_wbuffer_size) {
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
                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE&& m_write_buf.size() + size > m_max_wbuffer_size) {
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
                static std::atomic<SessionID> next_session_id(BoostNet::NetCallBack::InvalidSessionID);
                return ++next_session_id;
            }

            socket_type& get_socket() {
                return boost::beast::get_lowest_layer(m_socket).socket();
            }

        private:
            // �첽��
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

            // �첽д
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

            // �������ӻص�
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

            // ����ʼ
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

            // ������ص�
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

            // ����д�ص�
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
            // TCP������
            boost::asio::ip::tcp::resolver m_resolver;
            // asio��socket��װ
            websocket_stream_type   m_socket;
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
            BoostNet::NetCallBack   m_handler;

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