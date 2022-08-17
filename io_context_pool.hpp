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
        typedef std::shared_ptr<ioc_type>       ioc_ptr_type;
        typedef std::shared_ptr<work_type>      work_ptr_type;

        // io_context的对象池
        // pool_size 缺省时,服务对象数是机器里cpu数
        AsioContextPool(int pool_size = 0)
            : m_pool_size(pool_size)
            , m_next_ioc_index(0)
            , m_bstart(false)
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
            if (!m_bstart.compare_exchange_strong(expected, false))  // 已终止则退出
                return;

            reset_sync();
        }

        // 重置
        void reset() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // 未启动则退出
                return;

            reset_sync();
        }

        // 循环获取io_context
        ioc_type& get_io_context() {
            auto& result = *m_io_contexts[m_next_ioc_index];
            if(++m_next_ioc_index == m_pool_size) 
                m_next_ioc_index = 0;
            return result;
        }

    private:
        void init(int pool_size) {
            reset_sync();

            for (int i = 0; i < pool_size; i++) {
                auto new_ioc = std::make_shared<ioc_type>();
                m_io_contexts.emplace_back(new_ioc);
                m_io_works.emplace_back(std::make_shared<work_type>(*new_ioc));
                m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*new_ioc)));
            }
        }

        void reset_sync() {
            for (auto& ioc_ptr : m_io_contexts)
                ioc_ptr->stop();
			
            m_threads.join_all();
            m_io_works.clear();
            m_io_contexts.clear();
            m_next_ioc_index = 0;
        }

    private:
        // 线程池个数
        int                         m_pool_size;
        // 下一io_context的下标
        int                         m_next_ioc_index;
        // io_context的对象池
        std::vector<ioc_ptr_type>   m_io_contexts;
        // io_context在无任何任务的情况下便会退出,为确保无任务下不会退出,增加work确保有任务
        std::vector<work_ptr_type>  m_io_works;
        // 线程池
        boost::thread_group         m_threads;
        // 是否已开启
        std::atomic<bool>           m_bstart;
    };
}