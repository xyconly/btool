/*************************************************
File name:  parallel_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供并行有序执行的线程池
1, 可同时添加多个任务;
2, 所有任务有先后执行顺序,但可能会同时进行;
4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
5. 提供可扩展或缩容线程池数量功能。
*************************************************/
#pragma once

#include <atomic>
#include <vector>
#include <set>
#include <boost/noncopyable.hpp>
#include "job_queue.hpp"
#include "atomic_switch.hpp"
#include "safe_thread.hpp"

namespace BTool
{
    //////////////////////////////////////////////////////////////////////////
    // 并行有序任务池
    class ParallelTaskPool : private boost::noncopyable
    {
        enum {
            STP_MAX_THREAD = 2000,   // 最大线程数
        };

#pragma region 任务队列
        class ParallelTaskQueue
        {
#pragma region 任务相关操作
            class VirtualTask
            {
            public:
                VirtualTask() {}
                virtual ~VirtualTask() {}

                // 执行调用函数
                virtual void invoke() = 0;
            };
            typedef std::shared_ptr<VirtualTask> TaskPtr;

            template<typename Function, typename TTuple>
            class TaskItem : public VirtualTask
            {
            public:
#if (defined __linux)
                TaskItem(const Function& func, const TTuple& t)
                    : fun_cbk_(func)
                    , tuple_(t)
                {
                }
#else
                TaskItem(Function&& func, TTuple&& t)
                    : fun_cbk_(std::forward<Function>(func))
                    , tuple_(std::forward<TTuple>(t))
                {
                }
#endif

                ~TaskItem() {
                }

                // 执行调用函数
                void invoke() {
                    constexpr auto size = std::tuple_size<typename std::decay<TTuple>::type>::value;
                    invoke_impl(std::make_index_sequence<size>{});
                }

            private:
                template<std::size_t... Index>
                decltype(auto) invoke_impl(std::index_sequence<Index...>) {
                    return fun_cbk_(std::get<Index>(tuple_)...);
                }

            private:
                TTuple      tuple_;
                Function    fun_cbk_;
            };
#pragma endregion

        public:
            // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
            ParallelTaskQueue(size_t max_task_count = 0)
                : m_queue(max_task_count)
            {
            }

            ~ParallelTaskQueue() {
                m_queue.clear();
                m_queue.stop();
            }

#if (defined __linux)
            template<typename Function, typename... Args>
            bool add_task(const Function& func, Args&... args)
            {
                typedef std::tuple<typename std::__strip_reference_wrapper<Args>::__type...> _Ttype;
                auto new_task_item = std::make_shared<TaskItem<Function, _Ttype>>(func, std::make_tuple(args...));
                if (!new_task_item)
                    return false;
                return m_queue.push(new_task_item);
            }
#else
            template<typename Function, typename... Args>
            bool add_task(Function&& func, Args&&... args)
            {
#  if (!defined _MSC_VER) || (_MSC_VER < 1900)
#    error "Low Compiler."
#  elif (_MSC_VER >= 1900 && _MSC_VER < 1915) // vs2015
                typedef std::tuple<typename std::_Unrefwrap<Args>::type...> _Ttype;
#  else
                typedef std::tuple<typename std::_Unrefwrap_t<Args>...> _Ttype;
#  endif
                auto new_task_item = std::make_shared<TaskItem<Function, _Ttype>>(std::forward<Function>(func), std::make_tuple(std::forward<Args>(args)...));
                if (!new_task_item)
                    return false;
                return m_queue.push(new_task_item);
            }
#endif

            // 移除一个顶层非当前执行属性任务,队列为空时存在阻塞
            void pop_task() {
                TaskPtr next_task(nullptr);
                if (m_queue.pop(next_task)) {
                    next_task->invoke();
                }
            }

            void stop() {
                m_queue.stop();
            }

            bool empty() const {
                return m_queue.empty();
            }

            size_t size() const {
                return m_queue.size();
            }
        private:
            JobQueue<TaskPtr>           m_queue;
        };
#pragma endregion

    public:
        // 根据新增任务顺序并行有序执行的线程池
        // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
        ParallelTaskPool(size_t max_task_count = 0)
            : m_cur_thread_ver(0)
            , m_task_queue(max_task_count)
        {
        }

        ~ParallelTaskPool() {
            stop();
        }

        // 是否已启动
        bool has_start() const {
            return m_atomic_switch.has_started();
        }

        // 是否已终止
        bool has_stop() const {
            return m_atomic_switch.has_stoped();
        }

        // 开启线程池
        // thread_num: 开启线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        void start(size_t thread_num = std::thread::hardware_concurrency())
        {
            if (!m_atomic_switch.start())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

        // 重置线程池个数,每缩容一个线程时会存在一个指针的内存冗余(线程资源会自动释放),执行stop函数或析构函数可消除该冗余
        // thread_num: 重置线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        // 注意:必须开启线程池后方可生效
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency())
        {
            if (!m_atomic_switch.has_started())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

        // 终止线程池
        // 注意此处可能阻塞,完全停止后方可重新开启
        void stop()
        {
            if (!m_atomic_switch.stop())
                return;

            m_task_queue.stop();

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            for (auto& thread : m_cur_thread)
            {
                delete thread;
                thread = nullptr;
            }
            m_cur_thread.clear();

            m_atomic_switch.store_start_flag(false);
        }
		
#if (defined __linux)
        // 新增任务队列,超出最大任务数时存在阻塞
        template<typename Function, typename... Args>
        bool add_task(const Function& func, Args... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_queue.add_task(func, args...);
        }
#else
        // 新增任务队列,超出最大任务数时存在阻塞
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        template<typename Function, typename... Args>
        bool add_task(Function&& func, Args&&... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_queue.add_task(std::forward<Function>(func), std::forward<Args>(args)...);
        }
#endif

    private:
        // 创建线程
        void create_thread(size_t thread_num)
        {
            if (thread_num == 0) {
                thread_num = std::thread::hardware_concurrency();
            }
            m_cur_thread_ver++;
            thread_num = thread_num < STP_MAX_THREAD ? thread_num : STP_MAX_THREAD;
            for (size_t i = 0; i < thread_num; i++) {
                SafeThread* thd = new SafeThread(std::bind(&ParallelTaskPool::thread_fun, this, m_cur_thread_ver));
                m_cur_thread.push_back(thd);
            }
        }

        // 线程池线程
        void thread_fun(size_t thread_ver)
        {
            while (true)
            {
                if (m_atomic_switch.has_stoped()) {
                    break;
                }

                {
                    std::lock_guard<std::mutex> lck(m_threads_mtx);
                    if (thread_ver < m_cur_thread_ver)
                        break;
                }

                m_task_queue.pop_task();
            }
        }

    private:
        // 原子启停标志
        AtomicSwitch                m_atomic_switch;

        // 线程队列锁
        std::mutex                  m_threads_mtx;
        // 线程队列
        std::vector<SafeThread*>    m_cur_thread;
        // 当前设置线程版本号,每次重新设置线程数时,会递增该数值
        size_t                      m_cur_thread_ver;

        // 待执行任务队列
        ParallelTaskQueue           m_task_queue;
    };
}