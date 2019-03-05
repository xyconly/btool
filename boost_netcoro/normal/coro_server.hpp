/*************************************************
File name:      coro_server.hpp
Author:			AChar
Version:
Date:
Purpose: ����beastʵ�ֵ�Э��server����, ���ⲿ����session�������ӷ�ʽ
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
        // TSession: ͨѶ������,���ڴ洢�½����ӵ���ز���
        // ���뺬��TSession(boost::asio::ip::tcp::socket& dev, boost::asio::yield_context& yield)���캯��
        // �Լ�coro_start()����
        template<typename TSession>
        class CoroServer : public std::enable_shared_from_this<CoroServer<TSession>>
        {
            typedef std::shared_ptr<TSession>				TSessionPtr;

        public:

            enum class ServerErrCode : int {
                Err_Success = 0,
                Err_Open,		// ������������
                Err_Bind,		// �󶨼�������
                Err_Listen,		// ��������
                Err_Accept,		// socket��������
                Err_Close,		// �رմ���
                Err_Other,		// δ֪����
            };

#pragma region �ص��ź�
        public:
            typedef std::function<void(const TSessionPtr& session_ptr)>		session_func_t;
            typedef std::function<void(std::string&& err_msg)>				error_func_t;

            // ���ô���ص�,Э�̶���
            void setErrorCbk(const error_func_t& cbk) {
                m_error_cbk = cbk;
            }
            // �������µ����ӻص�,Э�̶���
            void setConnectCbk(const session_func_t& cbk) {
                m_connect_cbk = cbk;
            }
        protected:
            // ����ص�,��������ʧ��
            error_func_t		m_error_cbk;
            // ���µ����ӻص�
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

            // ������������������Э��ģʽ
            // "port"�����������ö˿ںţ�����ָ��һ����Чֵ��ע��ܿ����õ�ϵͳ����˿�
            // "thread_num"�������߳���,0Ϊϵͳ�߳���
            // "addr"�������������������ַ�������ڶ��������������"0.0.0.0"/"127.0.0.1"��ʹ��Ĭ�ϵ�ַ��
            long acceptor(unsigned short port, std::size_t thread_num = 0, const std::string& addr = "")
            {
                bool expected = false;
                if (!m_bstart.compare_exchange_strong(expected, true))	// �Ƿ�������
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
                if (!m_bstart.compare_exchange_strong(expected, false))  // δ�������˳�
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
            // ����״̬
            std::atomic<bool>					m_bstart;
            boost::asio::io_service*			m_ios;
            boost::thread						m_thrd;
        };
    }
}