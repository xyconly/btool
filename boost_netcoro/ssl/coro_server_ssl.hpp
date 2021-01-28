/*************************************************
File name:      coro_server_ssl.hpp
Author:			AChar
Version:
Date:
Purpose: ����beastʵ�ֵ�Э��server����, ���ⲿ����session�������ӷ�ʽ
Note:    ���������Լ�����openssl·��,����ʹ�õ���openssl-1.0.2l
*************************************************/

#pragma once

#include <boost/asio/ssl.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#include "example/server_certificate.hpp"

namespace BTool
{
    namespace BeastCoro
    {
        // TSessionSsl: ͨѶ������,���ڴ洢�½����ӵ���ز���
        // ���뺬��TSessionSsl(boost::asio::ip::tcp::socket& dev, boost::asio::yield_context& yield)���캯��
        // �Լ�coro_start()����
        template<typename TSessionSsl>
        class CoroServerSsl : public std::enable_shared_from_this<CoroServerSsl<TSessionSsl>>
        {
            typedef std::shared_ptr<TSessionSsl>				TSessionPtr;

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
            typedef std::function<void(std::string& err_msg)>				error_func_t;

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
            CoroServerSsl()
                : m_bstart(false)
                , m_ios(nullptr)
                , m_error_cbk(nullptr)
                , m_connect_cbk(nullptr)
            {
            }

            ~CoroServerSsl()
            {
                close();
            }

            // ������������������Э��ģʽ
            // "port"�����������ö˿ںţ�����ָ��һ����Чֵ��ע��ܿ����õ�ϵͳ����˿�
            // "thread_num"�������߳���
            // "addr"�������������������ַ�������ڶ��������������"0.0.0.0"/"127.0.0.1"��ʹ��Ĭ�ϵ�ַ��
            long acceptor(unsigned short port, std::size_t thread_num = 1, const std::string& addr = "")
            {
                bool expected = false;
                if (!m_bstart.compare_exchange_strong(expected, true))	// �Ƿ�������
                    return 1;

                bool useSpecAddr(true);
                if (addr.empty() || addr == "127.0.0.1" || addr == "0.0.0.0")
                    useSpecAddr = false;

                auto const address = useSpecAddr ? boost::asio::ip::address::from_string(addr) : boost::asio::ip::address_v4::any();
                auto stp = address.to_string();
                auto const threads = std::max<std::size_t>(1, thread_num);

                m_thrd = boost::thread(boost::bind(&CoroServerSsl<TSessionSsl>::on_acceptor, shared_from_this(), std::move(address), port, threads));
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
            void on_acceptor(boost::asio::ip::address& address, unsigned short port, const std::size_t threads_num)
            {
                // The io_service is required for all I/O
                boost::asio::io_service ios{ threads_num };
                m_ios = &ios;
                // The SSL context is required, and holds certificates
                boost::asio::ssl::context ctx{ boost::asio::ssl::context::sslv23 };

                // This holds the self-signed certificate used by the server
                ::load_server_certificate(ctx);
                ctx.set_password_callback(boost::bind(&CoroServerSsl<TSessionSsl>::get_password, shared_from_this()));
//                 ctx.use_certificate_chain_file("server.pem");
//                 ctx.use_private_key_file("server.pem", boost::asio::ssl::context::pem);
//                 ctx.use_tmp_dh_file("dh512.pem");

                // Spawn a listening port
                boost::asio::spawn(ios,
                    boost::bind(
                        &CoroServerSsl<TSessionSsl>::do_listen, shared_from_this(),
                        std::ref(ios),
                        std::ref(ctx),
                        boost::asio::ip::tcp::endpoint{ address, port },
                        boost::placeholders::_1));

                // Run the I/O service on the requested number of threads
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
                boost::asio::ssl::context& ctx,
                boost::asio::ip::tcp::endpoint endpoint,
                boost::asio::yield_context yield)
            {
                boost::system::error_code ec;

                // Open the acceptor
                boost::asio::ip::tcp::acceptor acceptor(ios);
                acceptor.open(endpoint.protocol(), ec);
                if (ec)
                    return fail(ec, ServerErrCode::Err_Open);

                // Bind to the server address
                acceptor.bind(endpoint, ec);
                if (ec)
                    return fail(ec, ServerErrCode::Err_Bind);

                // Start listening for connections
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
                                &CoroServerSsl<TSessionSsl>::do_session, shared_from_this(),
                                std::move(socket),
                                std::ref(ctx),
                                std::placeholders::_1));
                }
            }

            // Echoes back all received network messages
            void do_session(boost::asio::ip::tcp::socket& socket, boost::asio::ssl::context& ctx, boost::asio::yield_context yield)
            {
                TSessionPtr sess_ptr = std::make_shared<TSessionSsl>(socket, ctx, yield);

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

            std::string get_password() const
            {
                return "test";
            }

        private:
            // ����״̬
            std::atomic<bool>					m_bstart;
            boost::asio::io_service*			m_ios;
            boost::thread						m_thrd;
        };
    }
}