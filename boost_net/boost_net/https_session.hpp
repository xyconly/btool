/******************************************************************************
File name:  https_session.hpp
Author:	    AChar
Purpose:    https连接类, http的ssl实现
Note:       客户端可直接使用HttpsClientSession,调用HttpClientNetCallBack回调

示例代码:
        class TestHttpClient : public BTool::BoostNet::HttpClientNetCallBack
        {
            BTool::BoostNet::HttpsClientSession     session_type;
            typedef std::shared_ptr<session_type>       session_ptr_type;
        public:
            TestHttpClient()
                : m_context({ boost::asio::ssl::context::tlsv12_client })
            {
                boost::beast::error_code ignore_ec;
                load_root_certificates(m_context, ignore_ec);
                m_session = std::make_shared<session_type>(get_io_context(), m_context);
                m_session->register_cbk(this);
                m_session->connect(ip, port);
            }

        protected:
            // 开启连接回调
            virtual void on_open_cbk(SessionID session_id) override;

            // 关闭连接回调
            virtual void on_close_cbk(SessionID session_id) override;

            // 读取消息回调,此时read_msg_type为boost::beast::http::response<boost::beast::http::string_body>
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) override;

            // 写入消息回调,此时send_msg_type为boost::beast::http::request<boost::beast::http::string_body>
            virtual void on_write_cbk(SessionID session_id, const send_msg_type& send_msg) override;

        private:
            boost::asio::ssl::context   m_context;
            session_ptr_type            m_session;
        }

备注:
        也可直接自定义发送及返回消息类型, 如
            using SelfHttpClientNetCallBack = HttpNetCallBack<false, boost::beast::http::file_body, boost::beast::http::string_body>;
            using SelfHttpsClientSession = HttpsSession<false, boost::beast::http::file_body, boost::beast::http::string_body>
*****************************************************************************/

#pragma once

#include <tuple>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include "../http_net_callback.hpp"
#include "../../atomic_switch.hpp"

namespace BTool
{
    namespace BoostNet
    {


#define DeclareGetRequestFunc(Name, Type) \
    static send_msg_type GetSend##Name##Request(const std::string& target, const std::string& content_type = "application/json", int version = 11) { \
        return GetSendRequest(boost::beast::http::verb::Type, target, content_type, version); \
    }

#define DeclareSyncFunc(Name) \
    template<size_t MaxReadSize = 0, typename TRESTFul = void> \
    static std::tuple<bool, read_msg_type> Sync##Name(const char* host, unsigned short port, std::string target, TRESTFul&& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& body) { \
        GetRestFulTarget(target, restfuls); \
        auto send_msg = GetSend##Name##Request(target); \
        for (auto& [key, val] : heads) { \
            SetSendMsgHead(send_msg, key, val); \
        } \
        SetSendMsgBody(send_msg, body); \
        return SyncWrite<MaxReadSize>(host, port, std::move(send_msg), GetDefaultContext()); \
    } \
    template<size_t MaxReadSize = 0, typename TRESTFul = void> \
    std::tuple<bool, read_msg_type> sync##Name(const std::string& target, TRESTFul&& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& body) { \
        read_msg_type read_msg = {}; \
        if (!m_atomic_switch.has_started()) { \
            read_msg.body() = "unready finish"; \
            return std::forward_as_tuple(false, std::move(read_msg)); \
        } \
        std::string real_target = target; \
        GetRestFulTarget(real_target, restfuls); \
        auto send_msg = GetSend##Name##Request(real_target); \
        for (auto& [key, val] : heads) SetSendMsgHead(send_msg, key, val); \
        SetSendMsgBody(send_msg, body); \
        return sync_write<MaxReadSize>(std::move(send_msg)); \
    }
    
#define DeclareAsyncFunc(Name) \
    template <typename TCallBackFunc, typename TRESTFul = void> \
    static void Async##Name(const char* host, unsigned short port, std::string target, TRESTFul&& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& body, TCallBackFunc&& callback) { \
        GetRestFulTarget(target, restfuls); \
        auto send_msg = GetSend##Name##Request(target); \
        for (auto& [key, val] : heads) { \
            SetSendMsgHead(send_msg, key, val); \
        } \
        SetSendMsgBody(send_msg, body); \
        return AsyncWrite(host, port, std::move(send_msg), GetDefaultContext(), std::forward<TCallBackFunc>(callback)); \
    } \
    template <typename TCallBackFunc, typename TRESTFul = void> \
    void async##Name(std::string target, TRESTFul&& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& body, TCallBackFunc&& callback) { \
        GetRestFulTarget(target, restfuls); \
        auto send_msg = GetSend##Name##Request(target); \
        send_msg.set(boost::beast::http::field::host, m_connect_ip); \
        send_msg.set(boost::beast::http::field::connection, "keep-alive"); \
        for (auto& [key, val] : heads) SetSendMsgHead(send_msg, key, val); \
        SetSendMsgBody(send_msg, body); \
    }

        // Https连接对象
        template<bool isRequest, typename ReadType, typename WriteType = ReadType, typename Fields = boost::beast::http::fields>
        class HttpsSession : public std::enable_shared_from_this<HttpsSession<isRequest, ReadType, WriteType, Fields>>
        {
        public:
            typedef boost::asio::ip::tcp::resolver                              resolver_type;
            typedef boost::asio::ip::tcp::socket                                socket_type;
            typedef boost::asio::ssl::context                                   ssl_context_type;
            typedef boost::asio::ssl::stream<boost::beast::tcp_stream>          stream_type;
            typedef boost::beast::flat_buffer                                   read_buffer_type;

            typedef HttpsSession<isRequest, ReadType, WriteType, Fields>                SessionType;
            typedef BoostNet::HttpNetCallBack<isRequest, ReadType, WriteType, Fields>   callback_type;
            typedef typename callback_type::read_msg_type                               read_msg_type;
            typedef typename callback_type::send_msg_type                               send_msg_type;
            typedef typename callback_type::SessionID                                   SessionID;

            // 异步会话结构体
            template<typename TCallBackFunc>
            struct AsyncSession : public std::enable_shared_from_this<AsyncSession<TCallBackFunc>>
            {
                boost::asio::io_context             ioc_;
                ssl_context_type&                   ctx_;
                resolver_type                       resolver_;
                stream_type                         stream_;
                read_buffer_type                    read_buf_;
                read_msg_type                       read_msg_;
                std::string                         host_;
                unsigned short                      port_;
                send_msg_type                       send_msg_;
                TCallBackFunc                       callback_;

                AsyncSession(ssl_context_type& ssl_ctx, const std::string& host, unsigned short port, send_msg_type&& send_msg, TCallBackFunc&& callback)
                    : ctx_(ssl_ctx)
                    , resolver_(ioc_)
                    , stream_(ioc_, ctx_)
                    , host_(host)
                    , port_(port)
                    , send_msg_(std::move(send_msg))
                    , callback_(std::forward<TCallBackFunc>(callback))
                {
                }
            };

            // 异步会话结构体
            template<typename TCallBackFunc>
            struct asyncSession : public std::enable_shared_from_this<asyncSession<TCallBackFunc>>
            {
                read_buffer_type                    read_buf_;
                read_msg_type                       read_msg_;
                send_msg_type                       send_msg_;
                TCallBackFunc                       callback_;

                asyncSession(send_msg_type&& send_msg, TCallBackFunc&& callback)
                    : send_msg_(std::move(send_msg))
                    , callback_(std::forward<TCallBackFunc>(callback))
                {
                }
            };

        public:
            // Http连接对象
            // ios: io读写动力服务
            // max_rbuffer_size:单次读取最大缓冲区大小
            HttpsSession(socket_type&& socket, ssl_context_type& ctx)
                : m_resolver(socket.get_executor())
                , m_stream(std::move(socket), ctx)
                , m_session_id(GetNextSessionID())
                , m_handler(nullptr)
                , m_connect_port(0)
            {
            }

            HttpsSession(boost::asio::io_context& ioc, ssl_context_type& ctx)
                : m_resolver(boost::asio::make_strand(ioc))
                , m_stream(boost::asio::make_strand(ioc), ctx)
                , m_session_id(GetNextSessionID())
                , m_handler(nullptr)
                , m_connect_port(0)
            {
            }

            HttpsSession(boost::asio::io_context& ioc)
                : m_resolver(boost::asio::make_strand(ioc))
                , m_stream(boost::asio::make_strand(ioc), GetDefaultContext())
                , m_session_id(GetNextSessionID())
                , m_handler(nullptr)
                , m_connect_port(0)
            {
            }

            ~HttpsSession() {
                m_handler = nullptr;
                close(boost::beast::error_code{});
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            inline boost::asio::io_context& get_io_context() {
                return static_cast<boost::asio::io_context&>(m_stream.get_executor().context());
            }

            // 是否已开启
            bool is_open() const {
                return  m_atomic_switch.has_started();
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
            void async_connect(const char* host, unsigned short port)
            {
                if (!m_atomic_switch.init())
                    return;
                
                if (!SSL_set_tlsext_host_name(m_stream.native_handle(), host))
                {
                    boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                    close(ec);
                    m_atomic_switch.reset();
                    return;
                }

                m_connect_ip = host;
                m_connect_port = port;

                m_resolver.async_resolve(
                    host,
                    std::to_string(port),
                    boost::beast::bind_front_handler(&SessionType::handle_resolve, SessionType::shared_from_this()));
            }

            // 客户端开启连接,同时开启读取
            void async_reconnect()
            {
                async_connect(m_connect_ip.c_str(), m_connect_port);
            }

            // 客户端开启连接,同时开启读取
            bool connect(const char* host, unsigned short port)
            {
                if (!m_atomic_switch.init())
                    return true;
                
                m_connect_ip = host;
                m_connect_port = port;

                if (!SSL_set_tlsext_host_name(m_stream.native_handle(), host)) {
                    boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                    callback_close(ec);
                    return false;
                }
                boost::beast::error_code ec;
                // 连接
                boost::asio::ip::tcp::resolver::query query(host, GetPort(port));
                auto const results = m_resolver.resolve(query);
                boost::beast::get_lowest_layer(m_stream).connect(results, ec);
                if (ec) {
                    callback_close(ec);
                    return false;
                }
                // 握手
                m_stream.handshake(boost::asio::ssl::stream_base::client, ec);
                if (ec) {
                    callback_close(ec);
                    return false;
                }
                m_atomic_switch.start();
                callback_open();
                return true;
            }
            
            bool reconnect()
            {
                return connect(m_connect_ip.c_str(), m_connect_port);
            }

            // 服务端开启连接,同时开启读取
            void start(boost::asio::ssl::stream_base::handshake_type handshake = boost::asio::ssl::stream_base::server)
            {
                if (!m_atomic_switch.init())
                    return;

                boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                m_stream.async_handshake(handshake,
                    boost::beast::bind_front_handler(&SessionType::handle_handshake, SessionType::shared_from_this()));
            }

            template<size_t MaxReadSize = 0>
            std::tuple<bool, read_msg_type> sync_write(send_msg_type&& send_msg, size_t retry_count = 1) {
                read_msg_type read_msg = {};
                if (!m_atomic_switch.has_started()) {
                    read_msg.body() = "unready finish";
                    return std::forward_as_tuple(false, std::move(read_msg));
                }
                send_msg.set(boost::beast::http::field::host, m_connect_ip);
                send_msg.set(boost::beast::http::field::connection, "keep-alive");
                if constexpr (MaxReadSize > 0) read_msg.body().reserve(MaxReadSize);
                boost::beast::error_code ec;
                boost::beast::http::write(m_stream, std::forward<send_msg_type>(send_msg), ec);
                if (ec) {
                    if (retry_count > 0) {
                        close(ec, false);
                        if (reconnect()) {
                            return sync_write(std::move(send_msg), retry_count - 1);
                        }
                    }
                    callback_shutdown(ec);
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }
                read_buffer_type read_buf;
                boost::beast::http::read(m_stream, read_buf, read_msg, ec);
                if (ec) {
                    if (retry_count > 0) {
                        close(ec, false);
                        if (reconnect()) {
                            return sync_write(std::move(send_msg), retry_count - 1);
                        }
                    }
                    callback_shutdown(ec);
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }
                return std::forward_as_tuple(true, std::move(read_msg));
            }
            
            // 异步写,开启异步写之前先确保开启异步连接
            template<typename TCallBackFunc>
            bool async_write(send_msg_type&& send_msg, TCallBackFunc&& callback, size_t retry_count = 1)
            {
                if (!m_atomic_switch.has_started()) {
                    return false;
                }
                auto session = std::make_shared<asyncSession<TCallBackFunc>>(std::move(send_msg), std::forward<TCallBackFunc>(callback));
                boost::beast::http::async_write(m_stream, session->send_msg_
                    , [this, session, retry_count](boost::beast::error_code ec, std::size_t) {
                        if (ec) {
                            if (retry_count > 0) {
                                close(ec, false);
                                if (reconnect()) {
                                    return async_write(std::move(session->send_msg_), std::move(session->callback_), retry_count - 1);
                                }
                            }
                            session->read_msg_.body() = ec.message();
                            session->callback_(false, std::move(session->read_msg_));
                            callback_shutdown(ec);
                            return;
                        }

                        boost::beast::http::read(m_stream, session->read_buf_, session->read_msg_, ec);
                        if (ec) {
                            if (retry_count > 0) {
                                close(ec, false);
                                if (reconnect()) {
                                    return async_write(std::move(session->send_msg_), std::move(session->callback_), retry_count - 1);
                                }
                            }
                            session->read_msg_.body() = ec.message();
                            session->callback_(false, std::move(session->read_msg_));
                            callback_shutdown(ec);
                            return;
                        }
                        session->callback_(true, std::move(session->read_msg_));
                    });
                return true;
            }
            // 异步写,开启异步写之前先确保开启异步连接
            bool async_write(send_msg_type&& msg)
            {
                m_send_msg = std::forward<send_msg_type>(msg);
                m_send_msg.set(boost::beast::http::field::host, m_connect_ip);

                boost::beast::http::async_write(
                    m_stream, m_send_msg
                    , boost::beast::bind_front_handler(&SessionType::handle_write
                        , SessionType::shared_from_this()
                        , m_send_msg.need_eof()));

                return true;
            }

            // 同步关闭
            void shutdown(const boost::beast::error_code& ec = {}) {
                if (!m_atomic_switch.stop())
                    return;

                close(ec);
            }

            static send_msg_type GetSendRequest(const boost::beast::http::verb& verb, const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                send_msg_type send_req;
                send_req.version(version);
                send_req.method(verb);
                send_req.target(target);
                send_req.keep_alive(false);
                send_req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
                send_req.set(boost::beast::http::field::content_type, content_type);
                return send_req;
            }

            static void SetSendMsgHead(send_msg_type& send_msg, const std::string& name, const std::string& val) {
                send_msg.set(name, val);
            }
            static void SetSendMsgBody(send_msg_type& send_msg, const std::string& body) {
                send_msg.body() = body;
                send_msg.set(boost::beast::http::field::content_length, std::to_string(body.length()));
            }
            static ssl_context_type& GetDefaultContext() {
                static ssl_context_type s_ctx = InitDefaultContext();
                return s_ctx;
            }

            // 获取待发送put请求信息
            // target: 路径,包含Query
            // version: https协议版本
            DeclareGetRequestFunc(Get, get)
            /* static send_msg_type GetSendGetRequest(const std::string& target, const std::string& content_type = "application/json", int version = 11) {
                    return GetSendRequest(boost::beast::http::verb::get, target, content_type, version);
            }*/
            // 获取待发送post请求信息
            // target: 路径,包含Query
            // version: https协议版本
            DeclareGetRequestFunc(Post, post)
            // 获取待发送get请求信息
            // target: 路径,包含Query
            // version: https协议版本
            DeclareGetRequestFunc(Put, put)
            // 获取待发送delete请求信息
            // target: 路径,包含Query
            // version: https协议版本
            DeclareGetRequestFunc(Delete, delete_)

            DeclareSyncFunc(Get)
            /* template<size_t MaxReadSize = 0, typename TRESTFul = void>
            static std::tuple<bool, read_msg_type> SyncGet(const char* host, unsigned short port, std::string target, TRESTFul&& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& body) {
                GetRestFulTarget(target, restfuls);
                auto send_msg = GetSendGetRequest(target);
                for (auto& [key, val] : heads) SetSendMsgHead(send_msg, key, val);
                SetSendMsgBody(send_msg, body);
                return SyncWrite<MaxReadSize>(host, port, std::move(send_msg), GetDefaultContext());
            }
            // 长链接获取
            template<size_t MaxReadSize = 0, typename TRESTFul = void>
            std::tuple<bool, read_msg_type> syncGet(std::string target, TRESTFul&& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& body) {
                GetRestFulTarget(target, restfuls);
                auto send_msg = GetSendGetRequest(target);
                send_msg.set(boost::beast::http::field::host, m_connect_ip);
                send_msg.set(boost::beast::http::field::connection, "keep-alive");
                for (auto& [key, val] : heads) SetSendMsgHead(send_msg, key, val);
                SetSendMsgBody(send_msg, body);
                read_msg_type read_msg = {};
                if constexpr (MaxReadSize > 0) read_msg.body().reserve(MaxReadSize);
                boost::beast::error_code ec;
                boost::beast::http::write(m_stream, std::forward<send_msg_type>(send_msg), ec);
                if (ec) {
                    callback_close(ec);
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }
                read_buffer_type read_buf;
                boost::beast::http::read(m_stream, read_buf, read_msg, ec);
                if (ec) {
                    callback_close(ec);
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }
                return std::forward_as_tuple(true, std::move(read_msg));
            } */
            DeclareSyncFunc(Post)
            DeclareSyncFunc(Put)
            DeclareSyncFunc(Delete)

            // 使用ip+port(port为0则为host解析)同步发送,仅用于客户端,非线程安全
            template<size_t MaxReadSize, typename TSendMsg>
            static std::tuple<bool, read_msg_type> SyncWrite(const char* host, TSendMsg&& send_msg, ssl_context_type& ctx) {
                return SyncWrite<MaxReadSize, TSendMsg>(host, 0, std::forward<TSendMsg>(send_msg), ctx);
            }
            template<size_t MaxReadSize, typename TSendMsg>
            static std::tuple<bool, read_msg_type> SyncWrite(const char* host, unsigned short port, TSendMsg&& send_msg, ssl_context_type& ctx)
            {
                read_msg_type read_msg = {};
                if constexpr (MaxReadSize > 0)
                    read_msg.body().reserve(MaxReadSize);

                boost::asio::io_context ioc;
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::asio::ip::tcp::resolver resolver(ioc);
                stream_type stream(ioc, ctx);

                // 证书
                if (!SSL_set_tlsext_host_name(stream.native_handle(), host)) {
                    boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                    read_msg.body() = ec.message();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                boost::beast::error_code ec;
                // 连接
                boost::asio::ip::tcp::resolver::query query(host, GetPort(port));
                auto const results = resolver.resolve(query);
                boost::beast::get_lowest_layer(stream).connect(results, ec);
                
                if (ec) {
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                // 握手
                stream.handshake(boost::asio::ssl::stream_base::client, ec);
                // 检查握手是否成功
                if (ec) {
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                // 发送消息
                send_msg.set(boost::beast::http::field::host, host);
                boost::beast::http::write(stream, std::forward<send_msg_type>(send_msg), ec);
                if (ec) {
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                // 读取应答
                read_buffer_type read_buf;
                boost::beast::http::read(stream, read_buf, read_msg, ec);
                if (ec) {
                    read_msg.body() = ec.what();
                    return std::forward_as_tuple(false, std::move(read_msg));
                }

                stream.shutdown(ec);
                return std::forward_as_tuple(true, std::move(read_msg));
            }
            
            // 使用ip+port(port为0则为host解析)同步发送,仅用于客户端
            template<typename TSendMsg>
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, TSendMsg&& send_msg, ssl_context_type& ctx) {
                return SyncWriteEndOfStream(host, 0, std::forward<TSendMsg>(send_msg), ctx);
            }
            template<typename TSendMsg>
            static std::tuple<bool, std::vector<read_msg_type>> SyncWriteEndOfStream(const char* host, unsigned short port, TSendMsg&& send_msg, ssl_context_type& ctx)
            {
                std::vector<read_msg_type> rslt;
                boost::asio::io_context ioc;
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::asio::ip::tcp::resolver resolver(ioc);
                stream_type stream(ioc, ctx);
                
                try {
                    // 证书
                    if (!::SSL_set_tlsext_host_name(stream.native_handle(), host)) {
                        boost::beast::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                        return std::forward_as_tuple(false, std::move(rslt));
                    }

                    // 连接
                    boost::asio::ip::tcp::resolver::query query(host, GetPort(port));
                    auto const results = resolver.resolve(query);
                    boost::beast::get_lowest_layer(stream).connect(results);

                    // 握手
                    stream.handshake(boost::asio::ssl::stream_base::client);

                    // 发送消息
                    send_msg.set(boost::beast::http::field::host, host);
                    boost::beast::http::write(stream, std::forward<TSendMsg>(send_msg));

                    // 读取应答
                    boost::beast::error_code ec;
                    for (;;)
                    {
                        read_msg_type read_msg = {};
                        read_buffer_type read_buf;
                        /*auto read_len =*/ boost::beast::http::read(stream, read_buf, read_msg, ec);
                        if (ec == boost::beast::http::error::end_of_stream)
                            break;
                        rslt.push_back(std::move(read_msg));
                        if (ec)
                            return std::forward_as_tuple(false, std::move(rslt));
                    }

                    stream.shutdown(ec);
                }
                catch (std::exception const&)
                {
                    return std::forward_as_tuple(false, std::move(rslt));
                }

                return std::forward_as_tuple(true, std::move(rslt));
            }

            /************************* 异步 ******************************/
            DeclareAsyncFunc(Get)
            /* template <typename TCallBackFunc>
            static void AsyncGet(const char* host, unsigned short port, std::string target, const std::vector<std::string>& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& msg, TCallBackFunc&& callback) {
                if (!restfuls.empty()) {
                    target += "?";
                    for (auto& restful : restfuls) target += restful + "&";
                    target.pop_back();
                }
                auto send_msg = GetSendGetRequest(target);
                for (auto& [key, val] : heads) SetSendMsgHead(send_msg, key, val);
                SetSendMsgBody(send_msg, msg);
                return AsyncWrite(host, port, std::move(send_msg), GetDefaultContext(), std::forward<TCallBackFunc>(callback));
            }
            template <typename TCallBackFunc>
            static void AsyncGet(const char* host, unsigned short port, std::string target, const std::map<std::string, std::string>& restfuls, const std::unordered_map<std::string, std::string>& heads, const std::string& msg, TCallBackFunc&& callback) {
                if (!restfuls.empty()) {
                    target += "?";
                    for (auto& [key, value] : restfuls) target += key + "=" + value + "&";
                    target.pop_back();
                }
                auto send_msg = GetSendGetRequest(target);
                for (auto& [key, val] : heads) SetSendMsgHead(send_msg, key, val);
                SetSendMsgBody(send_msg, msg);
                return AsyncWrite(host, port, std::move(send_msg), GetDefaultContext(), std::forward<TCallBackFunc>(callback));
            } */
            DeclareAsyncFunc(Post)
            DeclareAsyncFunc(Put)
            DeclareAsyncFunc(Delete)

            template <typename TCallBackFunc>
            static void AsyncWrite(const char* host, unsigned short port, send_msg_type&& send_msg, ssl_context_type& ctx, TCallBackFunc&& callback)
            {
                auto session = std::make_shared<AsyncSession<TCallBackFunc>>(ctx, host, port, std::move(send_msg), std::forward<TCallBackFunc>(callback));
                if (!SSL_set_tlsext_host_name(session->stream_.native_handle(), session->host_.c_str())) {
                    boost::beast::error_code ssl_ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
                    session->read_msg_.body() = ssl_ec.message();
                    session->callback_(false, std::move(session->read_msg_));
                    return;
                }

                auto query = boost::asio::ip::tcp::resolver::query(host, std::to_string(port));
                // auto const results = session->resolver_.resolve(query);
                // boost::beast::get_lowest_layer(session->stream_).connect(results);
                // // 握手
                // session->stream_.handshake(boost::asio::ssl::stream_base::client);
                // // 发送消息
                // session->send_msg_.set(boost::beast::http::field::host, host);

                session->resolver_.async_resolve(
                    query,
                    [session](boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
                        if (ec) {
                            session->read_msg_.body() = ec.message();
                            session->callback_(false, std::move(session->read_msg_));
                            return;
                        }
                        //session->stream_.lowest_layer().expires_after(std::chrono::seconds(30));
                        boost::beast::get_lowest_layer(session->stream_).expires_after(std::chrono::seconds(30));
                        boost::beast::get_lowest_layer(session->stream_).async_connect(
                            results, [session](boost::beast::error_code ec, boost::asio::ip::tcp::endpoint) {
                                if (ec) {
                                    session->read_msg_.body() = ec.message();
                                    session->callback_(false, std::move(session->read_msg_));
                                    return;
                                }
                                boost::beast::get_lowest_layer(session->stream_).expires_after(std::chrono::seconds(30));
                                //session->stream_.lowest_layer().expires_after(std::chrono::seconds(30));
                                session->stream_.async_handshake(
                                    boost::asio::ssl::stream_base::client,
                                    [session](boost::beast::error_code ec) {
                                        if (ec) {
                                            session->read_msg_.body() = ec.message();
                                            session->callback_(false, std::move(session->read_msg_));
                                            return;
                                        }
                                        session->send_msg_.set(boost::beast::http::field::host, session->stream_.lowest_layer().remote_endpoint(ec).address().to_string(ec));
                                        boost::beast::http::async_write(
                                            session->stream_, session->send_msg_,
                                            [session](boost::beast::error_code ec, std::size_t) {
                                                if (ec) {
                                                    session->read_msg_.body() = ec.message();
                                                    session->callback_(false, std::move(session->read_msg_));
                                                    return;
                                                }

                                                boost::beast::http::async_read(
                                                    session->stream_, session->read_buf_, session->read_msg_,
                                                    [session](boost::beast::error_code ec, std::size_t) {
                                                        if (ec) {
                                                            session->read_msg_.body() = ec.message();
                                                            session->callback_(false, std::move(session->read_msg_));
                                                            return;
                                                        }

                                                        boost::beast::error_code shutdown_ec;
                                                        session->stream_.shutdown(shutdown_ec);
                                                        session->callback_(true, std::move(session->read_msg_));
                                                    });
                                            });
                                    });
                            });
                    });
            }

       protected:
            static SessionID GetNextSessionID()
            {
                static std::atomic<SessionID> next_session_id(callback_type::InvalidSessionID);
                return ++next_session_id;
            }

        private:

            void close(const boost::beast::error_code& ec, bool callback = true)
            {
                boost::beast::error_code ignored_ec;
                auto& lowest_layer = m_stream.lowest_layer();
                lowest_layer.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
                if(m_atomic_switch.has_init()) {
                    auto ssl_handle = m_stream.native_handle();
                    if (ssl_handle != nullptr) {
                        SSL_clear(ssl_handle);
                    }
                    lowest_layer.cancel(ignored_ec);
                    if (lowest_layer.is_open()) {
                        lowest_layer.close(ignored_ec);
                        boost::beast::get_lowest_layer(m_stream).socket().close(ignored_ec);
                    }
                }

                m_read_buf.consume(m_read_buf.size());
                m_atomic_switch.reset();

                if (m_handler && callback) {
                    m_handler->on_close_cbk(m_session_id, ec.message());
                }
            }

            // 异步读
            void read(bool close)
            {
                //try {
                    m_read_msg = {};
                    boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                    boost::beast::http::async_read(m_stream, m_read_buf, m_read_msg,
                        boost::beast::bind_front_handler(&SessionType::handle_read
                            , SessionType::shared_from_this()
                            , close));
                // }
                // catch (...) {
                //     shutdown();
                // }
            }

            // 解析IP回调
            void handle_resolve(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& results)
            {
                if (ec) {
                    close(ec);
                    m_atomic_switch.reset();
                    return;
                }

                boost::beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));
                boost::beast::get_lowest_layer(m_stream).async_connect(results
                    , boost::beast::bind_front_handler(&SessionType::handle_connect, SessionType::shared_from_this()));
            }

            // 处理连接回调
            void handle_connect(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type::endpoint_type& endpoint)
            {
                if (ec) {
                    close(ec);
                    m_atomic_switch.reset();
                    return;
                }

                start(boost::asio::ssl::stream_base::client);
            }

            // 处理连接回调
            void handle_handshake(boost::beast::error_code ec)
            {
                if (ec || !m_atomic_switch.start())
                    return close(ec);

                if (m_connect_ip.empty())
					m_connect_ip = m_stream.lowest_layer().remote_endpoint(ec).address().to_string(ec);
                if (m_connect_port == 0)
                    m_connect_port = m_stream.lowest_layer().remote_endpoint(ec).port();

                if (m_handler) {
                    m_handler->on_open_cbk(m_session_id);
                }

                if (isRequest)
                    read(false);
            }

            // 处理读回调
            void handle_read(bool close, const boost::beast::error_code& ec, size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                if (ec) {
                    shutdown(ec);
                    return;
                }

                if (m_handler)
                    m_handler->on_read_cbk(m_session_id, m_read_msg);

                if (close)
                    shutdown();
            }

            // 处理写回调
            void handle_write(bool close, const boost::beast::error_code& ec, size_t /*bytes_transferred*/)
            {
                if (ec) {
                    return shutdown(ec);
                }

                if (m_handler)
                    m_handler->on_write_cbk(m_session_id, m_send_msg);

                if (!isRequest || !close)
                    return read(close);

                if (close)
                    return shutdown();
            }

        private:
            static ssl_context_type InitDefaultContext() {
                ssl_context_type s_ctx{ boost::asio::ssl::context::tlsv12_client };
                s_ctx.set_default_verify_paths();
                s_ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                return s_ctx;
            }

            inline static std::string GetPort(unsigned short port) {
                std::string port_str;
                if (port == 0)
                    port_str = "https";
                else
                    port_str = std::to_string(port);
                return port_str;
            }

            inline static void GetRestFulTarget(std::string& target, const std::vector<std::string>& restfuls) {
                if (!restfuls.empty()) {
                    target += "?";
                    for (auto& key : restfuls) target += key + "&";
                    target.pop_back();
                }
            }

            inline static void GetRestFulTarget(std::string& target, const std::map<std::string, std::string>& restfuls) {
                if (!restfuls.empty()) {
                    target += "?";
                    for (auto& [key, value] : restfuls) target += key + "=" + value + "&";
                    target.pop_back();
                }
            }

            void callback_open() {
                auto& io_context = get_io_context();
                io_context.post([this]{
                    if (m_handler) {
                        m_handler->on_open_cbk(m_session_id);
                    }
                });
            }

            void callback_close(const boost::beast::error_code& ec) {
                auto& io_context = get_io_context();
                io_context.post([this, ec]{
                    close(ec);
                });
            }
            void callback_shutdown(const boost::beast::error_code& ec) {
                auto& io_context = get_io_context();
                io_context.post([this, ec]{
                    close(ec);
                });
            }

        private:
            // asio的socket封装
            resolver_type           m_resolver;
            stream_type             m_stream;
            SessionID               m_session_id;

            // 读缓冲
            read_buffer_type        m_read_buf;

            // 回调操作
            callback_type*          m_handler;

            // 原子启停标志
            AtomicSwitch            m_atomic_switch;

            // 连接者IP
            std::string             m_connect_ip;
            // 连接者Port
            unsigned short          m_connect_port;

            read_msg_type           m_read_msg;
            send_msg_type           m_send_msg;
        };

        // 默认的客户端, 发送请求,读取应答
        using HttpsClientSession = HttpsSession<false, boost::beast::http::string_body>;

    }
}