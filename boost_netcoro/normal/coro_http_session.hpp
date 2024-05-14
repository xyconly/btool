/*************************************************
File name:      coro_tcp_session.hpp
Author:			AChar
Version:
Date:
Purpose: 利用beast实现配合CoroServer的Http连接对象
Note:    http功能暂不齐全,
         测试时需将btool/boost_netserver_coro/example下copy至运行目录下,
         该indel.html是测试websocket使用的^^,
         源自QT的http://doc.qt.io/qt-5/qtwebsockets-echoserver-echoclient-html.html
*************************************************/

#pragma once

#ifndef BOOST_COROUTINES_NO_DEPRECATION_WARNING
# define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#endif

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

namespace BTool
{
    namespace BeastCoro
    {
        // Http连接对象
        class HttpSession : public std::enable_shared_from_this<HttpSession>
        {
            // This is the C++11 equivalent of a generic lambda.
            // The function object is used to send an HTTP message.
            template<class Stream>
            struct send_lambda
            {
                Stream& stream_;
                boost::system::error_code& ec_;
                boost::asio::yield_context yield_;

                explicit
                    send_lambda(
                        Stream& stream,
                        boost::system::error_code& ec,
                        boost::asio::yield_context yield)
                    : stream_(stream)
                    , ec_(ec)
                    , yield_(yield)
                {
                }

                template<bool isRequest, class Body, class Fields>
                void
                    operator()(boost::beast::http::message<isRequest, Body, Fields>&& msg) const
                {
                    // We need the serializer here because the serializer requires
                    // a non-const file_body, and the message oriented version of
                    // http::write only works with const messages.
                    boost::beast::http::serializer<isRequest, Body, Fields> sr{ msg };
                    boost::beast::http::async_write(stream_, sr, yield_[ec_]);
                }
            };

            typedef std::function<void(const std::shared_ptr<HttpSession>& session_ptr, const char* body, std::size_t body_len, const char* target, std::size_t target_len)>		read_msg_func_t;
            typedef std::function<void(const std::shared_ptr<HttpSession>& session_ptr)>								    disconn_func_t;

        public:
            HttpSession(boost::asio::ip::tcp::socket& dev, boost::asio::yield_context& yield)
                : m_socket(dev)
                , m_yield(yield)
                , m_doc_root("")
                , m_read_cbk(nullptr)
                , m_disconnect_cbk(nullptr)
            {
            }

            ~HttpSession() {}

/**************   通用连接信息  ******************/
            boost::asio::ip::tcp::socket& get_socket() const {
                return const_cast<HttpSession*>(this)->m_socket;
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

/**************   数据解析与回应  ******************/
            // 设置读取消息回调,协程动力
            void setReadMsgCbk(const read_msg_func_t& cbk) {
                m_read_cbk = cbk;
            }
            // 设置断开连接回调,协程动力
            void setDisConnectCbk(const disconn_func_t& cbk) {
                m_disconnect_cbk = cbk;
            }

            // 设置http根路径
            // 注1: 当连接为"http://127.0.0.1:12345/home/user/index.html"时, doc_root为运行路径
            // 注2: 当连接为"http://127.0.0.1:12345"时, doc_root为http://127.0.0.1:12345/home/user
            // 前者需要带上运行路径下的,额外的路径,以及对应文件名;  后者直接默认读取index.html文件
            void set_doc_root(const std::string& doc_root)
            {
                m_doc_root = doc_root;
            }

            void coro_start()
            {
                boost::system::error_code ec;

                // This buffer is required to persist across reads
                boost::beast::flat_buffer buffer;

                // This lambda is used to send messages
                send_lambda<boost::asio::ip::tcp::socket> lambda{ m_socket, ec, m_yield };

                for (;;)
                {
                    // Read a request
                    boost::beast::http::request<boost::beast::http::string_body> req;
                    boost::beast::http::async_read(m_socket, buffer, req, m_yield[ec]);
                    if (ec == boost::beast::http::error::end_of_stream)
                        break;

                    if (ec)
                        return fail(/*ec, "read"*/);

//                     std::stringstream read_msg;
//                     read_msg << boost::beast::buffers(buffer.data());

                    if (m_read_cbk)
                        m_read_cbk(shared_from_this(), req.body().data(), req.body().length(), req.target().data(), req.target().length());

                    // Send the response
                    handle_request(m_doc_root, std::move(req), lambda);
                    if (ec == boost::beast::http::error::end_of_stream)
                    {
                        // This means we should close the connection, usually because
                        // the response indicated the "Connection: close" semantic.
                        break;
                    }
                    if (ec)
                        return fail(/*ec, "write"*/);
                }

                // Send a TCP shutdown
                m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            }

            void fail()
            {
                if (m_disconnect_cbk)
                    m_disconnect_cbk(shared_from_this());
            }


/**************   内部函数  ******************/
        private:
            // Return a reasonable mime type based on the extension of a file.
            boost::beast::string_view
                mime_type(boost::beast::string_view path)
            {
                using boost::beast::iequals;
                auto const ext = [&path]
                {
                    auto const pos = path.rfind(".");
                    if (pos == boost::beast::string_view::npos)
                        return boost::beast::string_view{};
                    return path.substr(pos);
                }();
                if (iequals(ext, ".htm"))  return "text/html";
                if (iequals(ext, ".html")) return "text/html";
                if (iequals(ext, ".php"))  return "text/html";
                if (iequals(ext, ".css"))  return "text/css";
                if (iequals(ext, ".txt"))  return "text/plain";
                if (iequals(ext, ".js"))   return "application/javascript";
                if (iequals(ext, ".json")) return "application/json";
                if (iequals(ext, ".xml"))  return "application/xml";
                if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
                if (iequals(ext, ".flv"))  return "video/x-flv";
                if (iequals(ext, ".png"))  return "image/png";
                if (iequals(ext, ".jpe"))  return "image/jpeg";
                if (iequals(ext, ".jpeg")) return "image/jpeg";
                if (iequals(ext, ".jpg"))  return "image/jpeg";
                if (iequals(ext, ".gif"))  return "image/gif";
                if (iequals(ext, ".bmp"))  return "image/bmp";
                if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
                if (iequals(ext, ".tiff")) return "image/tiff";
                if (iequals(ext, ".tif"))  return "image/tiff";
                if (iequals(ext, ".svg"))  return "image/svg+xml";
                if (iequals(ext, ".svgz")) return "image/svg+xml";
                return "application/text";
            }

            // Append an HTTP rel-path to a local filesystem path.
            // The returned path is normalized for the platform.
            std::string
                path_cat(
                    boost::beast::string_view base,
                    boost::beast::string_view path)
            {
                if (base.empty())
                    return path.to_string();
                std::string result = base.to_string();
#if BOOST_MSVC
                char constexpr path_separator = '\\';
                if (result.back() == path_separator)
                    result.resize(result.size() - 1);
                result.append(path.data(), path.size());
                for (auto& c : result)
                    if (c == '/')
                        c = path_separator;
#else
                char constexpr path_separator = '/';
                if (result.back() == path_separator)
                    result.resize(result.size() - 1);
                result.append(path.data(), path.size());
#endif
                return result;
            }

            // This function produces an HTTP response for the given
            // request. The type of the response object depends on the
            // contents of the request, so the interface requires the
            // caller to pass a generic lambda for receiving the response.
            template<
                class Body, class Allocator,
                class Send>
                void
                handle_request(
                    boost::beast::string_view doc_root,
                    boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>&& req,
                    Send&& send)
            {
                // Returns a bad request response
                auto const bad_request =
                    [&req](boost::beast::string_view why)
                {
                    boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::bad_request, req.version() };
                    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(boost::beast::http::field::content_type, "text/html");
                    res.keep_alive(req.keep_alive());
                    res.body() = why.to_string();
                    res.prepare_payload();
                    return res;
                };

                // Returns a not found response
                auto const not_found =
                    [&req](boost::beast::string_view target)
                {
                    boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::not_found, req.version() };
                    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(boost::beast::http::field::content_type, "text/html");
                    res.keep_alive(req.keep_alive());
                    res.body() = "The resource '" + target.to_string() + "' was not found.";
                    res.prepare_payload();
                    return res;
                };

                // Returns a server error response
                auto const server_error =
                    [&req](boost::beast::string_view what)
                {
                    boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::internal_server_error, req.version() };
                    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(boost::beast::http::field::content_type, "text/html");
                    res.keep_alive(req.keep_alive());
                    res.body() = "An error occurred: '" + what.to_string() + "'";
                    res.prepare_payload();
                    return res;
                };

                // Make sure we can handle the method
                if (req.method() != boost::beast::http::verb::get &&
                    req.method() != boost::beast::http::verb::head)
                    return send(bad_request("Unknown HTTP-method"));

                // Request path must be absolute and not contain "..".
                if (req.target().empty() ||
                    req.target()[0] != '/' ||
                    req.target().find("..") != boost::beast::string_view::npos)
                    return send(bad_request("Illegal request-target"));

                // Build the path to the requested file
                std::string path = path_cat(doc_root, req.target());
                if (req.target().back() == '/')
                    path.append("index.html");

                // Attempt to open the file
                boost::beast::error_code ec;
                boost::beast::http::file_body::value_type body;
                body.open(path.c_str(), boost::beast::file_mode::scan, ec);

                // Handle the case where the file doesn't exist
                if (ec == boost::system::errc::no_such_file_or_directory)
                    return send(not_found(req.target()));

                // Handle an unknown error
                if (ec)
                    return send(server_error(ec.message()));

                // Respond to HEAD request
                if (req.method() == boost::beast::http::verb::head)
                {
                    boost::beast::http::response<boost::beast::http::empty_body> res{ boost::beast::http::status::ok, req.version() };
                    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(boost::beast::http::field::content_type, mime_type(path));
                    res.content_length(body.size());
                    res.keep_alive(req.keep_alive());
                    return send(std::move(res));
                }

                // Respond to GET request
                boost::beast::http::response<boost::beast::http::file_body> res{
                    std::piecewise_construct,
                    std::make_tuple(std::move(body)),
                    std::make_tuple(boost::beast::http::status::ok, req.version()) };
                res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(boost::beast::http::field::content_type, mime_type(path));
                res.content_length(body.size());
                res.keep_alive(req.keep_alive());
                return send(std::move(res));
            }


        private:
            boost::asio::ip::tcp::socket&	m_socket;
            boost::asio::yield_context&		m_yield;
            std::string						m_doc_root;
            // 读取消息回调
            read_msg_func_t					m_read_cbk;
            // 断开连接回调
            disconn_func_t					m_disconnect_cbk;
        };
    }
}