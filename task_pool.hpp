/*************************************************
File name:  task_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供各类任务线程池基类,避免外界重复创建
*************************************************/
#pragma once
#include <mutex>
#include <vector>
#include <boost/noncopyable.hpp>
#include "safe_thread.hpp"
#include "task_queue.hpp"

namespace BTool
{
    /*************************************************
                   任务线程池基类
    *************************************************/
    class TaskPoolBase
    {
        enum {
            TP_MAX_THREAD = 2000,   // 最大线程数
        };

    public:
        TaskPoolBase() : m_cur_thread_ver(0), m_task_queue(nullptr) {}
        virtual ~TaskPoolBase() { }

    protected:
        void set_queue_ptr(TaskQueueBaseVirtual* task_queue) {
            m_task_queue = task_queue;
        }

        virtual void pop_task_inner() = 0;

    public:
        // 开启线程池
        // thread_num: 开启线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        void start(size_t thread_num = std::thread::hardware_concurrency()) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            m_task_queue->start();
            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

        // 终止线程池
        // 注意此处可能阻塞等待task的回调线程结束,故在task的回调线程中不可调用该函数
        // bwait: 是否强制等待当前所有队列执行完毕后才结束
        // 完全停止后方可重新开启
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            m_task_queue->stop(bwait);

            std::vector<SafeThread*> tmp_threads;
            {
                std::lock_guard<std::mutex> lck(m_threads_mtx);
                tmp_threads.swap(m_cur_thread);
            }

            for (auto& thread : tmp_threads) {
                delete thread;
                thread = nullptr;
            }

            tmp_threads.clear();
            m_atomic_switch.reset();
        }

        // 清空任务队列,不会阻塞
        void clear() {
            m_task_queue->clear();
        }

        // 重置线程池个数,每缩容一个线程时会存在一个指针的内存冗余(线程资源会自动释放),执行stop函数或析构函数可消除该冗余
        // thread_num: 重置线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        // 注意:必须开启线程池后方可生效
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency()) {
            if (!m_atomic_switch.has_started())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

    private:
        // 创建线程
        void create_thread(size_t thread_num) {
            if (thread_num == 0) {
                thread_num = std::thread::hardware_concurrency();
            }
            size_t cur_thread_ver = ++m_cur_thread_ver;
            thread_num = thread_num < TP_MAX_THREAD ? thread_num : TP_MAX_THREAD;
            for (size_t i = 0; i < thread_num; i++) {
                m_cur_thread.push_back(new SafeThread(std::bind(&TaskPoolBase::thread_fun, this, cur_thread_ver)));
            }
        }

        // 线程池线程
        void thread_fun(size_t thread_ver) {
            while (true) {
                if (m_atomic_switch.has_stoped() && m_task_queue->empty()) {
                    break;
                }

                if (thread_ver < m_cur_thread_ver.load())
                    break;

                pop_task_inner();
            }
        }

    protected:
        // 原子启停标志
        AtomicSwitch                m_atomic_switch;
        // 队列指针,不同线程池只需替换不同队列指针即可
        TaskQueueBaseVirtual*       m_task_queue;

        std::mutex                  m_threads_mtx;
        // 线程队列
        std::vector<SafeThread*>    m_cur_thread;
        // 当前设置线程版本号,每次重新设置线程数时,会递增该数值
        std::atomic<size_t>         m_cur_thread_ver;
    };

    /*************************************************
    Description:    提供并行有序执行的线程池
    1, 可同时添加多个任务;
    2, 所有任务有先后执行顺序,但可能会同时进行;
    4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
    5. 提供可扩展或缩容线程池数量功能。
    *************************************************/
    class ParallelTaskPool
        : public TaskPoolBase
        , private boost::noncopyable
    {
    public:
        // 根据新增任务顺序并行有序执行的线程池
        // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
        ParallelTaskPool(size_t max_task_count = 0) : m_task_queue(max_task_count) {
            set_queue_ptr(&m_task_queue);
        }

        virtual ~ParallelTaskPool() {
            stop();
        }

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (!m_atomic_switch.has_started())
                return false;
            return m_task_queue.add_task(std::forward<TFunction>(func));
        }

    protected:
        virtual void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    protected:
        TaskQueue               m_task_queue;
    };

    /*************************************************
    Description:    专用于CTP,提供并行有序执行的线程池
    1, 可同时添加多个任务;
    2, 所有任务有先后执行顺序,但可能会同时进行;
    4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
    5. 提供可扩展或缩容线程池数量功能。
    6. 每次POP时均延时1S
    *************************************************/
    class ParallelWaitTaskPool : public ParallelTaskPool
    {
    public:
        // 根据新增任务顺序并行有序执行的线程池
        // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
        ParallelWaitTaskPool(size_t max_task_count = 0)
            : ParallelTaskPool(max_task_count)
            , m_sleep_millseconds(1100)
        {}

        ~ParallelWaitTaskPool() {}

        // 设置间隔时间
        void set_sleep_milliseconds(long long millseconds) {
            m_sleep_millseconds = millseconds;
        }

    protected:
        void pop_task_inner() override {
            ParallelTaskPool::pop_task_inner();
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_millseconds));
        }

    private:
        long long m_sleep_millseconds;
    };

    /*************************************************
    Description:    提供具有相同属性任务执行最新状态的线程池
    1, 每个属性都可以同时添加多个任务;
    2, 有很多的属性和很多的任务;
    3, 每个属性添加的任务必须有序串行执行,即在同一时刻不能有同时执行一个用户的两个任务;
    4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
    5. 提供可扩展或缩容线程池数量功能。
    *************************************************/
    template<typename TPropType>
    class LastTaskPool
        : public TaskPoolBase
        , private boost::noncopyable
    {
    public:
        // 具有相同属性任务执行最新状态的线程池
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        LastTaskPool(size_t max_task_count = 0)
            : m_task_queue(max_task_count)
        {
            set_queue_ptr(&m_task_queue);
        }

        ~LastTaskPool() {
            stop();
        }

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (!m_atomic_switch.has_started())
                return false;
            return m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }

    protected:
        void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    private:
        // 待执行任务队列
        LastTaskQueue<TPropType>        m_task_queue;
    };

    /*************************************************
    Description:    提供具有相同属性任务串行有序执行的线程池
    1, 每个属性都可以同时添加多个任务;
    2, 有很多的属性和很多的任务;
    3, 每个属性添加的任务必须有序串行执行,即在同一时刻不能有同时执行一个用户的两个任务;
    4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
    5. 提供可扩展或缩容线程池数量功能。
    *************************************************/
    template<typename TPropType>
    class SerialTaskPool
        : public TaskPoolBase
        , private boost::noncopyable
    {
    public:
        // 具有相同属性任务串行有序执行的线程池
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        SerialTaskPool(size_t max_task_count = 0) : m_task_queue(max_task_count) {
            set_queue_ptr(&m_task_queue);
        }

        ~SerialTaskPool() {
            stop();
        }

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (!m_atomic_switch.has_started())
                return false;
            return m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }

    protected:
        void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    private:
        // 待执行任务队列
        SerialTaskQueue<TPropType>          m_task_queue;
    };

}