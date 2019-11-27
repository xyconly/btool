/*************************************************
File name:  io_context_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供io_context的对象池,提供异步start及同步stop接口
*************************************************/
#pragma once

#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread.hpp>
#include <boost/functional/factory.hpp>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>

namespace BTool
{
    // io_context的对象池
    class AsioContextPool : private boost::noncopyable
    {
    public:
        typedef boost::asio::io_context         ioc_type;
        typedef boost::asio::io_context::work   work_type;

        // io_context的对象池
        // pool_size 缺省时,服务对象数是机器里cpu数
        AsioContextPool(int pool_size = 0)
            : m_bstart(false)
            , m_pool_size(pool_size)
            , m_io_context(nullptr)
            , m_io_work(nullptr)
        {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            start();
        }

        ~AsioContextPool() {
            stop();
        }

        // 非阻塞式启动服务
        void start() {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            init(m_pool_size);
        }

        void restart(int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            m_pool_size = pool_size;
            init(m_pool_size);
        }

        // 阻塞式启动服务,使用join_all等待
        void run() {
            start();
            m_threads.join_all();
        }

        // 停止服务,可能产生阻塞
        void stop() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // 未启动则退出
                return;

            stop_sync();
        }

        // 循环获取io_context
        ioc_type& get_io_context() {
            return *m_io_context;
        }

    private:
        void init(int pool_size) {
            stop_sync();

            m_io_context = new ioc_type(pool_size);
            m_io_work = new work_type(*m_io_context);
            for (size_t i = 0; i < pool_size; i++) {
                m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*m_io_context)));
            }
        }

        void stop_sync() {
            if (m_io_context) {
                m_io_context->stop();
                delete m_io_context;
                m_io_context = nullptr;

                delete m_io_work;
                m_io_work = nullptr;
            }
            m_threads.join_all();
        }

    private:
        // 线程池个数
        int                     m_pool_size;
        // io_context的对象池
        ioc_type*               m_io_context;
        // io_context在无任何任务的情况下便会退出,为确保无任务下不会退出,增加work确保有任务
        work_type*              m_io_work;
        // 线程池
        boost::thread_group     m_threads;
        // 是否已开启
        std::atomic<bool>       m_bstart;
    };
}