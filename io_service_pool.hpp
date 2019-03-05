/*************************************************
File name:  io_service_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供io_service的对象池,提供异步start及同步stop接口
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
    // io_service的对象池
    class AsioServicePool : private boost::noncopyable
    {
    public:
        typedef boost::asio::io_service         ios_type;
        typedef boost::asio::io_service::work   work_type;
        typedef boost::ptr_vector<ios_type>     io_services_type;
        typedef boost::ptr_vector<work_type>    works_type;

        // io_service的对象池
        // pool_size 缺省时,服务对象数是机器里cpu数
        AsioServicePool(size_t pool_size = 0)
            : m_next_io_service(0)
            , m_bstart(false)
            , m_pool_size(pool_size)
        {
            if (m_pool_size == 0)
                m_pool_size = boost::thread::hardware_concurrency();

            start();
        }

        ~AsioServicePool()
        {
            stop();
        }

        // 非阻塞式启动服务
        void start()
        {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            init(m_pool_size);
        }

        void restart(size_t pool_size = 0)
        {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            m_pool_size = pool_size;
            init(m_pool_size);
        }

        // 阻塞式启动服务,使用join_all等待
        void run()
        {
            start();
            m_threads.join_all();
        }

        // 停止服务,可能产生阻塞
        void stop()
        {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // 未启动则退出
                return;

            stop_sync();
        }

        // 循环获取io_service
        ios_type& get_io_service()
        {
            if (m_next_io_service >= m_io_services.size())
            {
                m_next_io_service = 0;
            }

            return m_io_services[m_next_io_service++];
        }

    private:
        void init(size_t pool_size)
        {
            stop_sync();

            for (size_t i = 0; i < pool_size; i++)
            {
                m_io_services.push_back(boost::factory<ios_type*>()());
                m_works.push_back((boost::factory<work_type*>()(m_io_services.back())));
            }

            BOOST_FOREACH(ios_type& ios, m_io_services)
            {
                m_threads.create_thread(boost::bind(&ios_type::run, boost::ref(ios)));
            }
        }

        void stop_sync()
        {
            m_works.clear();
            std::for_each(m_io_services.begin(), m_io_services.end(), boost::bind(&ios_type::stop, _1));
            m_threads.join_all();
        }

    private:
        // 线程池个数
        size_t                  m_pool_size;
        // io_service的对象池
        io_services_type        m_io_services;
        // 保持io_services对象池运行
        works_type              m_works;
        // 线程池
        boost::thread_group     m_threads;
        // 连接的下一个io_service
        size_t                  m_next_io_service;
        // 是否已开启
        std::atomic<bool>       m_bstart;
    };
}