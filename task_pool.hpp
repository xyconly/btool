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
#include "submodule/oneTBB/include/tbb/concurrent_hash_map.h"
#include "safe_thread.hpp"
#include "task_queue.hpp"

namespace BTool {
    /*************************************************
                   任务线程池基类
    *************************************************/
    template<typename TCrtpImpl, typename TQueueType>
    class TaskPoolBase
    {
        enum {
            TP_MAX_THREAD = 2000,   // 最大线程数
        };
        // noncopyable
        TaskPoolBase(const TaskPoolBase&) = delete;
        TaskPoolBase& operator=(const TaskPoolBase&) = delete;

    protected:
        TaskPoolBase(size_t max_task_count = 0) : m_task_queue(max_task_count), m_cur_thread_ver(0) {}
        virtual ~TaskPoolBase() { stop(); }

        static bool BindToCore(int coreId) {
            cpu_set_t cpuSet;
            CPU_ZERO(&cpuSet);
            CPU_SET(coreId, &cpuSet);

            if (sched_setaffinity(0,sizeof(cpuSet), &cpuSet) == -1) {
                return false;
            }
            else {
                return true;
            }
        }

    public:
        // 开启线程池
        // thread_num: 开启线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        void start(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            m_task_queue.start();
            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num, is_bind_core, start_core_index);
        }

        // 终止线程池
        // 注意此处可能阻塞等待task的回调线程结束,故在task的回调线程中不可调用该函数
        // bwait: 是否强制等待当前所有队列执行完毕后才结束
        // 完全停止后方可重新开启
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            m_task_queue.stop(bwait);

            std::vector<SafeThread*> tmp_threads;
            {
                std::lock_guard<std::mutex> lck(m_threads_mtx);
                tmp_threads.swap(m_cur_threads);
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
            m_task_queue.clear();
        }

        // 等待所有任务执行完毕
        void wait() {
            m_task_queue.wait();
        }

        // 重置线程池个数,每缩容一个线程时会存在一个指针的内存冗余(线程资源会自动释放),执行stop函数或析构函数可消除该冗余
        // thread_num: 重置线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        // 注意:必须开启线程池后方可生效
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 0) {
            if (!m_atomic_switch.has_started())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num, is_bind_core, start_core_index);
        }

    protected:
        // crtp实现
        void pop_task_inner() {
            static_cast<TCrtpImpl*>(this)->pop_task_inner_impl();
        }
        void pop_task_inner_impl() {
            m_task_queue.pop_task();
        }

    private:
        // 创建线程
        void create_thread(size_t thread_num, bool is_bind_core, int start_core_index) {
            int core_num = std::thread::hardware_concurrency();
            if (thread_num == 0) {
                thread_num = core_num;
            }
            size_t cur_thread_ver = ++m_cur_thread_ver;
            thread_num = thread_num < TP_MAX_THREAD ? thread_num : TP_MAX_THREAD;

            for (size_t i = 0; i < thread_num; i++) {
                if (is_bind_core) {
                    if (start_core_index < core_num) {
                        m_cur_threads.push_back(new SafeThread(std::bind(&TaskPoolBase::thread_fun, this, cur_thread_ver, true, start_core_index++)));
                        continue;
                    }
                }
                
                m_cur_threads.push_back(new SafeThread(std::bind(&TaskPoolBase::thread_fun, this, cur_thread_ver, false,  0)));
            }
        }

        // 线程池线程
        void thread_fun(size_t thread_ver, bool is_bind_core, int core_index) {
            if (is_bind_core)
                BindToCore(core_index);
 
            while (true) {
                if (m_atomic_switch.has_stoped() && m_task_queue.empty()) {
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
        TQueueType                  m_task_queue;

        std::mutex                  m_threads_mtx;
        // 线程队列
        std::vector<SafeThread*>    m_cur_threads;
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
        : public TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>
    {
        friend class TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>;
    public:
        // 根据新增任务顺序并行有序执行的线程池
        // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
        ParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>(max_task_count)
        { }

        virtual ~ParallelTaskPool() {}

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };

    class NoBlockingParallelTaskPool
        : public TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>
    {
        friend class TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>;
    public:
        // 根据新增任务顺序并行有序执行的线程池
        // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
        NoBlockingParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>(max_task_count)
        { }

        virtual ~NoBlockingParallelTaskPool() {}

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };

    // 基于生产者/消费者模式
    class SingleThreadParallelTaskPool
        : public TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>
    {
        friend class TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>;
    public:
        // 根据新增任务顺序并行有序执行的线程池
        // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
        SingleThreadParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>(max_task_count)
        {
        }

        virtual ~SingleThreadParallelTaskPool() {}

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };


    /*************************************************
    Description:    专用于CTP,提供并行有序执行的线程池
    1, 可同时添加多个任务;
    2, 所有任务有先后执行顺序,但可能会同时进行;
    4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
    5. 提供可扩展或缩容线程池数量功能。
    6. 每次POP时均延时1S
    *************************************************/
    class ParallelWaitTaskPool
        : public TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>
    {
        friend class TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>;
    public:
        // 根据新增任务顺序并行有序执行的线程池
        // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
        ParallelWaitTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>(max_task_count)
            , m_sleep_millseconds(1100)
        {}

        ~ParallelWaitTaskPool() {}

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }

        // 设置间隔时间
        void set_sleep_milliseconds(long long millseconds) {
            m_sleep_millseconds = millseconds;
        }

    protected:
        void pop_task_inner_impl() {
            TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>::pop_task_inner_impl();
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
        : public TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>
    {
        friend class TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>;
    public:
        // 具有相同属性任务执行最新状态的线程池
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        LastTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>(max_task_count)
        {
        }

        ~LastTaskPool() {}

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            this->m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }
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
        : public TaskPoolBase<SerialTaskPool<TPropType>, SerialTaskQueue<TPropType>>
    {
        friend class TaskPoolBase<SerialTaskPool<TPropType>, SerialTaskQueue<TPropType>>;
    public:
        // 具有相同属性任务串行有序执行的线程池
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        SerialTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<SerialTaskPool<TPropType>, SerialTaskQueue<TPropType>>(max_task_count)
        {
        }

        ~SerialTaskPool() {}

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            this->m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }
    };

    /*************************************************
    Description:    提供具有相同属性任务串行有序执行的线程池
                    但是是将任务按属性简单均匀分配至内部单线程线程池
                    这样做可能带来的后果是, 导致某个线程超负荷而其余线程空闲
                    且一旦add_task后不可删除属性, 属性一旦确定线程号后不得更改
                    适用于类似行情这种任务相对均匀的场景
    1, 每个属性都可以同时添加多个任务;
    2, 有很多的属性和很多的任务;
    3, 每个属性添加的任务必须有序串行执行,即在同一时刻不能有同时执行一个用户的两个任务;
    4, 实时性:该线程池无法确保不同属性间的实时性及均衡性, 仅仅只是按任务属性流转
    5. 提供可扩展或缩容线程池数量功能。
    *************************************************/
    template<typename TPropType>
    class RotateSerialTaskPool
    {
        enum {
            TP_MAX_THREAD = 2000,   // 最大线程数
        };

        using hash_type = typename tbb::concurrent_hash_map<TPropType, size_t>;

    public:
        // 注意: max_task_count:表示单线程池内的最大任务量, 和其余线程池不同
        RotateSerialTaskPool(size_t max_task_count = 0) : m_max_task_count(max_task_count), m_next_task_pool_index(-1) {}
        ~RotateSerialTaskPool() { stop(); }

    public:
        // 开启线程池
        // thread_num: 开启线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        void start(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;
            writeLock locker(m_mtx);
            create_thread(thread_num);
            for (auto& item : m_task_pools) {
                item->start(1, is_bind_core, start_core_index++);
            }
        }

        // 终止线程池
        // 注意此处可能阻塞等待task的回调线程结束,故在task的回调线程中不可调用该函数
        // bwait: 是否强制等待当前所有队列执行完毕后才结束
        // 完全停止后方可重新开启
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            writeLock locker(m_mtx);
            if (bwait) {
                std::vector<SafeThread> tmp_threads(m_task_pools.size());
                for (size_t i = 0;i < m_task_pools.size(); ++i) {
                    tmp_threads[i].start([this, i] {
                        m_task_pools[i]->stop(true);
                    });
                }
                for (auto& item : tmp_threads) {
                    if (item.joinable()) item.join();
                }
            }

            // 删除所有
            for (auto& item : m_task_pools) {
                delete item;
                item = nullptr;
            }

            m_task_pools.clear();
            m_next_task_pool_index.store(-1);
            m_atomic_switch.reset();
        }

        // 注意此处可能阻塞, 为了与stop做同步
        void clear() {
            readLock locker(m_mtx);
            for (auto& item : m_task_pools) {
                item->clear();
            }
        }
        
        // 等待所有任务执行完毕, 为了与stop做同步
        void wait() {
            typename hash_type::accessor ac;
            readLock locker(m_mtx);
            for (auto& item : m_task_pools) {
                item->wait();
            }
        }

        // 重置线程池个数,每缩容一个线程时会存在一个指针的内存冗余(线程资源会自动释放),执行stop函数或析构函数可消除该冗余
        // thread_num: 重置线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        // 注意:必须开启线程池后方可生效
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency()) {
            stop(true);
            start(thread_num);
        }

        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            readLock locker(m_mtx); // 为了与stop做同步
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;

            size_t index = 0;
            {
                typename hash_type::accessor ac;
                bool ok = m_prop_index.insert(ac, prop);
                if (ok) {
                    index = ac->second = ++m_next_task_pool_index % m_task_pools.size();
                }
                else {
                    index = ac->second;
                }
            }
            return m_task_pools[index]->add_task(std::forward<TFunction>(func));
        }

    private:
        // 创建线程
        void create_thread(size_t thread_num, bool is_bind_core = false, int start_core_index = 0) {
            if (thread_num == 0) {
                thread_num = std::thread::hardware_concurrency();
            }
            thread_num = thread_num < TP_MAX_THREAD ? thread_num : TP_MAX_THREAD;
            for (size_t i = 0; i < thread_num; i++) {
                m_task_pools.emplace_back(new ParallelTaskPool(m_max_task_count));
            }
        }

    protected:
        // 单线程池内的最大任务量
        size_t                              m_max_task_count;
        // 原子启停标志
        AtomicSwitch                        m_atomic_switch;
        // 数据安全锁
        rwMutex                             m_mtx;
        // 线程队列
        std::vector<ParallelTaskPool*>      m_task_pools;
        // 下一个新增属性的任务队列下标
        std::atomic<size_t>                 m_next_task_pool_index;
        // 属性对应队列下标
        hash_type                           m_prop_index;
    };

}