/******************************************************************************
File name:  http_server.hpp
Author:	    AChar
Purpose:    http服务类
Note:       可直接使用HttpService,调用HttpServiceNetCallBack回调

示例代码:
        class TestHttpService : public BTool::BoostNet::HttpServiceNetCallBack
        {
            typedef BTool::BoostNet1_71::HttpService    service_type;
            typedef std::shared_ptr<service_type>       service_ptr_type;
        public:
            TestHttpService()
            {
                m_service = std::make_shared<service_type>(get_io_context());
                m_service->register_cbk(this);
                bool rslt = m_service->start(ip);
            }

        protected:
            // 开启连接回调
            virtual void on_open_cbk(SessionID session_id) override;

            // 关闭连接回调
            virtual void on_close_cbk(SessionID session_id) override;

            // 读取消息回调,此时read_msg_type为boost::beast::http::request<boost::beast::http::string_body>
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) override;

            // 写入消息回调,此时send_msg_type为boost::beast::http::response<boost::beast::http::string_body>
            virtual void on_write_cbk(SessionID session_id, const send_msg_type& send_msg) override;

        private:
            service_ptr_type            m_service;
        }

备注:
        也可直接自定义发送及返回消息类型, 如
            using SelfHttpServiceNetCallBack = HttpNetCallBack<true, boost::beast::http::string_body, boost::beast::http::file_body>;
            using SelfHttpServer = HttpServer<true, boost::beast::http::string_body, boost::beast::http::file_body>
*****************************************************************************/

#pragma once

#include <mutex>
#include <map>
#include <set>
#include "../../io_context_pool.hpp"
#include "http_session.hpp"

namespace BTool
{
    namespace BoostNet1_71
    {
        // Http服务
        template<bool isRequest, class ReadType, class WriteType = ReadType, class Fields = boost::beast::http::fields>
        class HttpServer 
            : public BoostNet::HttpNetCallBack<isRequest, ReadType, WriteType, Fields>
            , public std::enable_shared_from_this<HttpServer<isRequest, ReadType, WriteType, Fields>>
        {
            typedef boost::asio::io_context                         io_context_type;
            typedef boost::asio::ip::tcp::acceptor                  accept_type;

            // 自身命名
            typedef HttpServer<isRequest, ReadType, WriteType, Fields>          ServerType;

            // 回调相关命名
            typedef BoostNet::HttpNetCallBack<isRequest, ReadType, WriteType, Fields>     callback_type;
            typedef typename callback_type::read_msg_type                       read_msg_type;
            typedef typename callback_type::send_msg_type                       send_msg_type;
            typedef typename callback_type::SessionID                           SessionID;
            typedef typename callback_type::method_type                         method_type;

            // Session命名
            typedef HttpSession<isRequest, ReadType, WriteType, Fields>         session_type;
            typedef std::shared_ptr<session_type>                               session_ptr_type;
            typedef std::map<SessionID, session_ptr_type>                       session_map_type;

        public:
            // Http服务
            // handler: session返回回调
            HttpServer(AsioContextPool& ioc)
                : m_ioc_pool(ioc)
                , m_acceptor(boost::asio::make_strand(ioc.get_io_context()))
                , m_handler(nullptr)
            {
            }

            ~HttpServer() {
                m_handler = nullptr;
                stop();
                boost::system::error_code ec;
                m_acceptor.close(ec);
            }

            // 设置回调,采用该形式可回调至不同类中分开处理
            void register_cbk(callback_type* handler) {
                m_handler = handler;
            }

            // 非阻塞式启动服务,
            // ip: 监听IP,默认本地IPV4地址
            // port: 监听端口
            // reuse_address: 是否启用端口复用
            bool start(const char* ip = nullptr, unsigned short port = 80, bool reuse_address = true)
            {
                m_ioc_pool.start();
                if (!start_listen(ip, port, reuse_address)) {
                    return false;
                }
                return true;
            }

            // 阻塞式启动服务,使用join_all等待
            // ip: 监听IP,默认本地IPV4地址
            // port: 监听端口
            // reuse_address: 是否启用端口复用
            void run(const char* ip = nullptr, unsigned short port = 80, bool reuse_address = true)
            {
                if (!start_listen(ip, port, reuse_address)) {
                    return;
                }
                m_ioc_pool.run();
            }

            // 终止当前服务
            void stop() {
                clear();
                m_ioc_pool.stop();
            }

            // 清空当前所有连接
            // 注意,该函数不会终止当前服务,仅终止并清空当前所有连接,服务的终止在stop()中操作
            void clear() {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.clear();
            }

            // 异步写入
            bool write(SessionID session_id, send_msg_type&& msg)
            {
                auto sess_ptr = find_session(session_id);
                if (!sess_ptr) {
                    return false;
                }
                return sess_ptr->async_write(std::forward<send_msg_type>(msg));
            }

            // 同步关闭连接,注意此时的close_cbk依旧在当前线程下
            void close(SessionID session_id)
            {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    sess_ptr->shutdown();
                }
            }

            // 设置支持的请求方法
            void set_allow_method(const std::set<method_type>& methods) {
                m_allow_methods = methods;
            }

            // 新增支持的请求方法
            void add_allow_method(method_type method) {
                m_allow_methods.emplace(method);
            }

            // 获取是否支持该请求方法
            bool is_allow_method(method_type method) {
                return m_allow_methods.find(method) != m_allow_methods.end();
            }

            // 获取连接者IP
            bool get_ip(SessionID session_id, std::string& ip) const {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    ip = sess_ptr->get_ip();
                    return true;
                }
                return false;
            }

            // 获取连接者port
            bool get_port(SessionID session_id, unsigned short& port) const {
                auto sess_ptr = find_session(session_id);
                if (sess_ptr) {
                    port = sess_ptr->get_port();
                    return true;
                }
                return false;
            }

        private:
            // 启动监听端口
            bool start_listen(const char* ip, unsigned short port, bool reuse_address)
            {
                boost::system::error_code ec;
                boost::asio::ip::tcp::endpoint endpoint;
                if (!ip) {
                    endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
                }
                else {
                    boost::asio::ip::tcp::resolver rslv(m_ioc_pool.get_io_context());
                    boost::asio::ip::tcp::resolver::query qry(ip, std::to_string(port));
                    auto iter = rslv.resolve(qry, ec);

                    if (iter != boost::asio::ip::tcp::resolver::iterator())
                        endpoint = iter->endpoint();

                    if (ec)
                        return false;
                }

                try {
                    m_acceptor.open(endpoint.protocol());
                    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(reuse_address));
                    m_acceptor.bind(endpoint);
                    m_acceptor.listen(boost::asio::socket_base::max_listen_connections);
                }
                catch (boost::system::system_error&) {
                    return false;
                }
//                 catch (...) {
//                     return false;
//                 }

                if (!m_acceptor.is_open())
                    return false;

                start_accept();
                return true;
            }

            // 开始监听
            void start_accept()
            {
                try {
                    m_acceptor.async_accept(
                        boost::asio::make_strand(m_ioc_pool.get_io_context()),
                        boost::beast::bind_front_handler(&ServerType::handle_accept, ServerType::shared_from_this()));
                }
                catch (std::exception&) {
                    stop();
                }
            }

            // 处理接听回调
            void handle_accept(const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket)
            {
                start_accept();

                if (ec) {
                    return;
                }

                auto session_ptr = std::make_shared<session_type>(std::move(socket));
                session_ptr->register_cbk(this);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_sessions.emplace(session_ptr->get_session_id(), session_ptr);
                }

                session_ptr->start();
            }

            // 查找连接对象
            session_ptr_type find_session(SessionID session_id) const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto iter = m_sessions.find(session_id);
                if (iter == m_sessions.end()) {
                    return session_ptr_type();
                }
                return iter->second;
            }

            // 删除连接对象
            void remove_session(SessionID session_id) 
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions.erase(session_id);
            }

        private:
            // 开启连接回调
            virtual void on_open_cbk(SessionID session_id) override
            {
                if(m_handler)
                    m_handler->on_open_cbk(session_id);
            }
            // 关闭连接回调
            virtual void on_close_cbk(SessionID session_id) override
            {
                remove_session(session_id);
                if (m_handler)
                    m_handler->on_close_cbk(session_id);
            }
            // 读取消息回调
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) override
            {
                if (m_handler)
                    m_handler->on_read_cbk(session_id, read_msg);
            }

        private:
            AsioContextPool&                        m_ioc_pool;
            accept_type                             m_acceptor;
            callback_type*                          m_handler;
            // 服务支持的方法类型
            std::set<method_type>             m_allow_methods;

            mutable std::mutex                      m_mutex;
            // 所有连接对象，后期改为内存块，节省开辟/释放内存时间
            session_map_type                        m_sessions;
        };

        // 默认的服务端, 读取请求,发送应答
        using HttpService = HttpServer<true, boost::beast::http::string_body>;
    }
}
