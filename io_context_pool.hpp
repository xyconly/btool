/*************************************************
File name:  io_context_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供io_context的对象池,提供异步start及同步stop接口
*************************************************/
#pragma once
#include <boost/version.hpp>

#if (BOOST_VERSION >= 107000 && BOOST_VERSION < 108000)
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <boost/functional/factory.hpp>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>
#include <atomic>
#include <vector>
#include "comm_function_os.hpp"

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
        AsioContextPool(int pool_size = 0, bool auto_start = true, const std::vector<int>& bind_cores = {})
            : m_pool_size(pool_size)
            , m_next_ioc_index(0)
            , m_bstart(false)
            , m_bind_cores(bind_cores)
        {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            if (auto_start)
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

        void start(int pool_size, const std::vector<int>& bind_cores) {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            if (pool_size <= 0)
                pool_size = boost::thread::hardware_concurrency();

            assert (pool_size <= (int)bind_cores.size());

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
        }

        bool restart(const std::vector<int>& bind_cores, int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
            return true;
        }

        bool restart(int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (!m_bind_cores.empty() && pool_size > (int)m_bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            
            init(m_pool_size);
            return true;
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
        void run_io_context(ioc_type& ioc, int core_id) {
            CommonOS::BindCore(core_id);
            ioc.run();
        }

        void init(int pool_size) {
            reset_sync();

            for (int i = 0; i < pool_size; i++) {
                auto new_ioc = std::make_shared<ioc_type>();
                m_io_contexts.emplace_back(new_ioc);
                m_io_works.emplace_back(std::make_shared<work_type>(*new_ioc));
                
                if ((int)m_bind_cores.size() > i) {
                    m_threads.create_thread(boost::bind(&AsioContextPool::run_io_context, this, boost::ref(*new_ioc), m_bind_cores[i]));
                }
                else {
                    m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*new_ioc)));
                }
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
        // 绑核
        std::vector<int>            m_bind_cores;
    };


    // 单一io_context的线程池
	// 当连接数较少时, 比多个ioc性能更优, 适合内网情况
    class AsioSingleContextPool : private boost::noncopyable
    {
    public:
        typedef boost::asio::io_context         ioc_type;
        typedef boost::asio::io_context::work   work_type;

        // io_context的对象池
        // pool_size 缺省时,服务对象数是机器里cpu数
        AsioSingleContextPool(int pool_size = 0, bool auto_start = true, const std::vector<int>& bind_cores = {})
            : m_pool_size(pool_size)
            , m_io_context(nullptr)
            , m_io_work(nullptr)
            , m_bstart(false)
            , m_bind_cores(bind_cores)
        {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            if (auto_start)
                start();
        }

        ~AsioSingleContextPool() {
            stop();
        }

        // 非阻塞式启动服务
        void start() {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            init(m_pool_size);
        }

        void start(int pool_size, const std::vector<int>& bind_cores) {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            if (pool_size <= 0)
                pool_size = boost::thread::hardware_concurrency();

            assert (pool_size <= (int)bind_cores.size());

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
        }

        bool restart(const std::vector<int>& bind_cores, int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
            return true;
        }

        bool restart(int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (!m_bind_cores.empty() && pool_size > (int)m_bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;

            init(m_pool_size);
            return true;
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

            if (m_io_context) {
                m_io_context->stop();
            }
            m_threads.join_all();
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
            return *m_io_context;
        }

    private:
        void run_io_context(ioc_type& ioc, int core_id) {
            CommonOS::BindCore(core_id);
            ioc.run();
        }

        void init(int pool_size) {
            reset_sync();

            m_io_context = new ioc_type(pool_size);
            m_io_work = new work_type(*m_io_context);
            for (int i = 0; i < pool_size; i++) {
                if ((int)m_bind_cores.size() > i) {
                    m_threads.create_thread(boost::bind(&AsioSingleContextPool::run_io_context, this, boost::ref(*m_io_context), m_bind_cores[i]));
                }
                else {
                    m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*m_io_context)));
                }
            }
        }

        void reset_sync() {
            if (m_io_context) {
                m_io_context->stop();
            }
            m_threads.join_all();

            if (m_io_context) {
                delete m_io_work;
                m_io_work = nullptr;

                delete m_io_context;
                m_io_context = nullptr;
            }
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
        // 绑核
        std::vector<int>        m_bind_cores;
    };
}
#elif BOOST_VERSION >= 108000 
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <atomic>
#include <vector>
#include "comm_function_os.hpp"

namespace BTool {
    // io_context的对象池
    class AsioContextPool : private boost::noncopyable {
    public:
        typedef boost::asio::io_context ioc_type;
        typedef boost::asio::executor_work_guard<ioc_type::executor_type> work_guard_type;
        typedef std::shared_ptr<ioc_type> ioc_ptr_type;
        typedef std::shared_ptr<work_guard_type> work_guard_ptr_type;

        // io_context的对象池
        // pool_size 缺省时,服务对象数是机器里cpu数
        AsioContextPool(int pool_size = 0, bool auto_start = true, const std::vector<int>& bind_cores = {})
            : m_pool_size(pool_size), m_next_ioc_index(0), m_bstart(false), m_bind_cores(bind_cores) {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            if (auto_start)
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

        void start(int pool_size, const std::vector<int>& bind_cores) {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            if (pool_size <= 0)
                pool_size = boost::thread::hardware_concurrency();

            assert(pool_size <= (int)bind_cores.size());

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
        }

        bool restart(const std::vector<int>& bind_cores, int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
            return true;
        }

        bool restart(int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (!m_bind_cores.empty() && pool_size > (int)m_bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;

            init(m_pool_size);
            return true;
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
            if (++m_next_ioc_index == m_pool_size)
                m_next_ioc_index = 0;
            return result;
        }

    private:
        void run_io_context(ioc_type& ioc, int core_id) {
            CommonOS::BindCore(core_id);
            ioc.run();
        }

        void init(int pool_size) {
            reset_sync();

            for (int i = 0; i < pool_size; i++) {
                auto new_ioc = std::make_shared<ioc_type>();
                m_io_contexts.emplace_back(new_ioc);
                m_work_guards.emplace_back(std::make_shared<work_guard_type>(new_ioc->get_executor()));

                if ((int)m_bind_cores.size() > i) {
                    m_threads.create_thread(boost::bind(&AsioContextPool::run_io_context, this, boost::ref(*new_ioc), m_bind_cores[i]));
                } else {
                    m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*new_ioc)));
                }
            }
        }

        void reset_sync() {
            for (auto& ioc_ptr : m_io_contexts)
                ioc_ptr->stop();

            m_threads.join_all();
            m_work_guards.clear();
            m_io_contexts.clear();
            m_next_ioc_index = 0;
        }

    private:
        int m_pool_size;
        int m_next_ioc_index;
        std::vector<ioc_ptr_type> m_io_contexts;
        std::vector<work_guard_ptr_type> m_work_guards;
        boost::thread_group m_threads;
        std::atomic<bool> m_bstart;
        std::vector<int> m_bind_cores;
    };

    // 单一 io_context 的线程池
    class AsioSingleContextPool : private boost::noncopyable {
    public:
        typedef boost::asio::io_context ioc_type;
        typedef boost::asio::executor_work_guard<ioc_type::executor_type> work_guard_type;

        // 构造函数
        AsioSingleContextPool(int pool_size = 0, bool auto_start = true, const std::vector<int>& bind_cores = {})
            : m_pool_size(pool_size), m_bstart(false), m_bind_cores(bind_cores) {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            if (auto_start)
                start();
        }

        ~AsioSingleContextPool() {
            stop();
        }

        // 非阻塞式启动服务
        void start() {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            init(m_pool_size);
        }

        void start(int pool_size, const std::vector<int>& bind_cores) {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // 已启动则退出
                return;

            if (pool_size <= 0)
                pool_size = boost::thread::hardware_concurrency();

            assert(pool_size <= (int)bind_cores.size());

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
        }

        bool restart(const std::vector<int>& bind_cores, int pool_size = 0) {
            stop();
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            start();
            return true;
        }

        bool restart(int pool_size = 0) {
            stop();
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (!m_bind_cores.empty() && pool_size > (int)m_bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;

            start();
            return true;
        }

        // 阻塞式启动服务，使用 join_all 等待
        void run() {
            start();
            m_threads.join_all();
        }

        // 停止服务，可能产生阻塞
        void stop() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // 已终止则退出
                return;

            if (m_io_context) {
                m_io_context->stop();
            }
            m_threads.join_all();
            m_work_guard.reset();
            m_io_context.reset();
        }

        // 获取 io_context
        ioc_type& get_io_context() {
            return *m_io_context;
        }

    private:
        void run_io_context(ioc_type& ioc, int core_id) {
            CommonOS::BindCore(core_id);
            ioc.run();
        }

        void init(int pool_size) {
            m_io_context = std::make_shared<ioc_type>();
            m_work_guard = std::make_shared<work_guard_type>(m_io_context->get_executor());

            for (int i = 0; i < pool_size; i++) {
                if ((int)m_bind_cores.size() > i) {
                    m_threads.create_thread(boost::bind(&AsioSingleContextPool::run_io_context, this, boost::ref(*m_io_context), m_bind_cores[i]));
                } else {
                    m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*m_io_context)));
                }
            }
        }

    private:
        // 线程池个数
        int m_pool_size;
        // io_context 对象
        std::shared_ptr<ioc_type> m_io_context;
        // 防止 io_context 提前退出
        std::shared_ptr<work_guard_type> m_work_guard;
        // 线程池
        boost::thread_group m_threads;
        // 是否已开启
        std::atomic<bool> m_bstart;
        // 绑核
        std::vector<int> m_bind_cores;
    };
}
#else
#error boost version not supported!
#endif