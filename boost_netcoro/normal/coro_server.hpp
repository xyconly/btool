/*************************************************
File name:      coro_server.hpp
Author:			AChar
Version:
Date:
Purpose: 利用beast实现的协程server对象, 由外部传入session决定连接方式
*************************************************/

#pragma once

#include <memory>
#include <vector>
#include <boost/asio/spawn.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

namespace BTool
{
    namespace BeastCoro
    {
        // TSession: 通讯连接类,用于存储新建连接的相关参数
        // 必须含有TSession(boost::asio::ip::tcp::socket& dev, boost::asio::yield_context& yield)构造函数
        // 以及coro_start()函数
        template<typename TSession>
        class CoroServer : public std::enable_shared_from_this<CoroServer<TSession>>
        {
            typedef std::shared_ptr<TSession>				TSessionPtr;

        public:

            enum class ServerErrCode : int {
                Err_Success = 0,
                Err_Open,		// 开启监听错误
                Err_Bind,		// 绑定监听错误
                Err_Listen,		// 监听错误
                Err_Accept,		// socket监听错误
                Err_Close,		// 关闭错误
                Err_Other,		// 未知错误
            };

#pragma region 回调信号
        public:
            typedef std::function<void(const TSessionPtr& session_ptr)>		session_func_t;
            typedef std::function<void(std::string&& err_msg)>				error_func_t;

            // 设置错误回调,协程动力
            void setErrorCbk(const error_func_t& cbk) {
                m_error_cbk = cbk;
            }
            // 设置有新的连接回调,协程动力
            void setConnectCbk(const session_func_t& cbk) {
                m_connect_cbk = cbk;
            }
        protected:
            // 错误回调,启动监听失败
            error_func_t		m_error_cbk;
            // 有新的连接回调
            session_func_t		m_connect_cbk;

#pragma endregion

        public:
            CoroServer()
                : m_bstart(false)
                , m_ios(nullptr)
                , m_error_cbk(nullptr)
                , m_connect_cbk(nullptr)
            {
            }

            ~CoroServer()
            {
                close();
            }

            // 启动连接侦听；采用协程模式
            // "port"：连接侦听用端口号，必需指定一个有效值，注意避开常用的系统服务端口
            // "thread_num"：开启线程数,0为系统线程数
            // "addr"：连接侦听本地网络地址，适用于多网卡情况，传入"0.0.0.0"/"127.0.0.1"亦使用默认地址；
            long acceptor(unsigned short port, std::size_t thread_num = 0, const std::string& addr = "")
            {
                bool expected = false;
                if (!m_bstart.compare_exchange_strong(expected, true))	// 是否已启动
                    return 1;

//                 bool useSpecAddr(true);
//                 if (addr.empty() || addr == "127.0.0.1" || addr == "0.0.0.0")
//                     useSpecAddr = false;

                auto const address = addr.empty() ? boost::asio::ip::address_v4::any() : boost::asio::ip::address::from_string(addr);
                auto stp = address.to_string();

                if (thread_num == 0)
                    thread_num = boost::thread::hardware_concurrency();

                m_thrd = boost::thread(boost::bind(&CoroServer<TSession>::on_acceptor, CoroServer<TSession>::shared_from_this(), std::move(address), port, thread_num));

                return 0;
            }

            bool isRunning() const
            {
                return m_bstart;
            }

            void close()
            {
                bool expected = true;
                if (!m_bstart.compare_exchange_strong(expected, false))  // 未启动则退出
                    return;

                if (m_ios)
                    m_ios->stop();

                m_thrd.join();
            }

        private:
            void on_acceptor(boost::asio::ip::address& address, unsigned short port, int threads_num)
            {
                boost::asio::io_service ios{ threads_num };
                m_ios = &ios;

                boost::asio::spawn(ios,
                    boost::bind(
                        &CoroServer<TSession>::do_listen, CoroServer<TSession>::shared_from_this(),
                        boost::ref(ios),
                        boost::asio::ip::tcp::endpoint{ address, port },
                        boost::placeholders::_1));

                std::vector<boost::thread> v;
                v.reserve(threads_num - 1);
                for (auto i = threads_num - 1; i > 0; --i)
                    v.emplace_back(
                        [&ios]
                {
                    ios.run();
                });
                ios.run();
            }

            void do_listen(boost::asio::io_service& ios,
                boost::asio::ip::tcp::endpoint endpoint,
                boost::asio::yield_context yield)
            {
                boost::system::error_code ec;

                boost::asio::ip::tcp::acceptor acceptor(ios);
                acceptor.open(endpoint.protocol(), ec);
                if (ec)
                    return fail(ec, ServerErrCode::Err_Open);

                acceptor.bind(endpoint, ec);
                if (ec)
                    return fail(ec, ServerErrCode::Err_Bind);

                acceptor.listen(boost::asio::socket_base::max_connections, ec);
                if (ec)
                    return fail(ec, ServerErrCode::Err_Listen);

                for (;;)
                {
                    boost::asio::ip::tcp::socket socket(ios);
                    acceptor.async_accept(socket, yield[ec]);
                    if (ec)
                        fail(ec, ServerErrCode::Err_Accept);
                    else
                        boost::asio::spawn(
                            acceptor.get_io_service(),
                            std::bind(
                                &CoroServer<TSession>::do_session, CoroServer<TSession>::shared_from_this(),
                                std::move(socket),
                                std::placeholders::_1));
                }
            }

            void do_session(boost::asio::ip::tcp::socket& socket, boost::asio::yield_context yield)
            {
                TSessionPtr sess_ptr = std::make_shared<TSession>(socket, yield);
                if (m_connect_cbk)
                    m_connect_cbk(sess_ptr);

                sess_ptr->coro_start();
            }

            void fail(boost::system::error_code ec, ServerErrCode what)
            {
                std::string tmp = std::to_string(ec.value()) + ": ";
                tmp += std::to_string((int)what);
                tmp += ": ";
                tmp += ec.message();
                if (m_error_cbk)
                    m_error_cbk(std::move(tmp));
            }

        private:
            // 运行状态
            std::atomic<bool>					m_bstart;
            boost::asio::io_service*			m_ios;
            boost::thread						m_thrd;
        };
    }
}