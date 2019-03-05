/*************************************************
File name:  serial_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供具有相同属性任务串行有序执行的线程池
1, 每个属性都可以同时添加多个任务;
2, 有很多的属性和很多的任务;
3, 每个属性添加的任务必须有序串行执行,即在同一时刻不能有同时执行一个用户的两个任务;
4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
5. 提供可扩展或缩容线程池数量功能。
*************************************************/
#pragma once

#include <list>
#include <vector>
#include <set>
#include <condition_variable>
#include <boost/noncopyable.hpp>
#include <boost/concept_check.hpp>
#include "rwmutex.hpp"
#include "atomic_switch.hpp"
#include "safe_thread.hpp"

namespace BTool
{
    //////////////////////////////////////////////////////////////////////////
    // 串行有序线程池
    template<typename PropType>
    class SerialTaskPool : private boost::noncopyable
    {
#pragma region 任务队列

        //////////////////////////////////////////////////////////////////////////
        // 串行有序线程池待执行任务队列
        // PropType: 任务属性
        class SerialTaskList
        {
#pragma region 任务相关操作
            class Task
            {
            public:
#if (defined __linux)
#else
                Task(PropType&& prop) :m_prop(std::forward<PropType>(prop)) {}
#endif
                Task(const PropType& prop) :m_prop(prop) {}

                // 获取任务属性
                const PropType& get_prop_type() const {
                    return m_prop;
                }

                // 执行调用函数
                virtual void invoke() = 0;

            private:
                PropType m_prop;
            };
            typedef std::shared_ptr<Task> TaskPtr;

            template<typename Function, typename TTuple>
            class TaskItem : public Task
            {
            public:
#if (defined __linux)
                TaskItem(const PropType& prop, const Function& func, const TTuple& t)
                    : Task(prop)
                    , fun_cbk_(func)
                    , tuple_(t)
                {
                }
#else
                TaskItem(PropType&& prop, Function&& func, TTuple&& t)
                    : Task(std::forward<PropType>(prop))
                    , fun_cbk_(std::forward<Function>(func))
                    , tuple_(std::forward<TTuple>(t))
                {
                }

                TaskItem(const PropType& prop, Function&& func, TTuple&& t)
                    : Task(prop)
                    , fun_cbk_(std::forward<Function>(func))
                    , tuple_(std::forward<TTuple>(t))
                {
                }
#endif

                // 执行调用函数
                void invoke()
                {
                    constexpr auto size = std::tuple_size<typename std::decay<TTuple>::type>::value;
                    invoke_impl(std::make_index_sequence<size>{});
                }

            private:
                // 执行调用函数具体实现
                template<std::size_t... Index>
                decltype(auto) invoke_impl(std::index_sequence<Index...>)
                {
                    return fun_cbk_(std::get<Index>(tuple_)...);
                }

            private:
                TTuple      tuple_;
                Function    fun_cbk_;
            };
#pragma endregion

        public:
            // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
            SerialTaskList(size_t max_task_count = 0)
                : m_max_task_count(max_task_count)
                , m_bstop(false)
            {
            }
            ~SerialTaskList() {
                writeLock locker(m_tasks_mtx);
                m_wait_tasks.clear();
            }

#if (defined __linux)
            template<typename Function, typename... Args>
            bool add_task(const PropType& prop, const Function& func, Args&... args)
            {
                wait_for_can_add();
                if (m_bstop.load())
                    return false;
                typedef std::tuple<typename std::__strip_reference_wrapper<Args>::__type...> _Ttype;
                return add_task_tolist(std::make_shared<TaskItem<Function, _Ttype>>(prop, func, std::make_tuple(args...)));
            }
#else
            template<typename Function, typename... Args>
            bool add_task(PropType&& prop, Function&& func, Args&&... args)
            {
                wait_for_can_add();

                if (m_bstop.load())
                    return false;

#  if (!defined _MSC_VER) || (_MSC_VER < 1900)
#    error "Low Compiler."
#  elif (_MSC_VER >= 1900 && _MSC_VER < 1915) // vs2015
                typedef std::tuple<typename std::_Unrefwrap<Args>::type...> _Ttype;
#  else
                typedef std::tuple<typename std::_Unrefwrap_t<Args>...> _Ttype;
#  endif

                return add_task_tolist(std::make_shared<TaskItem<Function, _Ttype>>(std::forward<PropType>(prop), std::forward<Function>(func), std::make_tuple(std::forward<Args>(args)...)));
            }

            // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
            template<typename Function, typename... Args>
            bool add_task(const PropType& prop, Function&& func, Args&&... args)
            {
                wait_for_can_add();

                if (m_bstop.load())
                    return false;

#  if (!defined _MSC_VER) || (_MSC_VER < 1900)
#    error "Low Compiler."
#  elif (_MSC_VER >= 1900 && _MSC_VER < 1915) // vs2015
                typedef std::tuple<typename std::_Unrefwrap<Args>::type...> _Ttype;
#  else
                typedef std::tuple<typename std::_Unrefwrap_t<Args>...> _Ttype;
#  endif

                return add_task_tolist(std::make_shared<TaskItem<Function, _Ttype>>(prop, std::forward<Function>(func), std::make_tuple(std::forward<Args>(args)...)));
            }
#endif

            // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
            void remove_prop(const PropType& prop)
            {
                writeLock locker(m_tasks_mtx);
                auto iter = m_wait_tasks.begin();
                while (iter != m_wait_tasks.end())
                {
                    if (prop != (*iter)->get_prop_type())
                    {
                        iter++;
                        continue;
                    }
                    m_wait_tasks.erase(iter++);
                }
                m_cv_not_full.notify_one();
            }

            // 移除一个顶层非当前执行属性任务,队列为空时存在阻塞
            void pop_task()
            {
                {
                    std::unique_lock<std::mutex> locker(m_cv_mtx);
                    m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
                }

                if (m_bstop.load())
                    return;

                TaskPtr next_task(nullptr);
                PropType prop_type;

                {
                    writeLock locker(m_tasks_mtx);
                    auto iter = m_wait_tasks.begin();
                    while (iter != m_wait_tasks.end())
                    {
                        prop_type = (*iter)->get_prop_type();

                        if (!add_cur_prop(prop_type))
                        {
                            iter++;
                            continue;
                        }
                        next_task = *iter;
                        m_wait_tasks.erase(iter);
                        m_cv_not_full.notify_one();
                        break;
                    }
                }

                if (next_task)
                {
                    next_task->invoke();
                    remove_cur_prop(prop_type);
                }
            }

            void stop()
            {
                // 是否已终止判断
                bool target(false);
                if (!m_bstop.compare_exchange_strong(target, true))
                    return;

                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
            }

            bool empty() const
            {
                readLock locker(m_tasks_mtx);
                return m_wait_tasks.empty();
            }

            bool full() const
            {
                readLock locker(m_tasks_mtx);
                return m_max_task_count != 0 && m_wait_tasks.size() >= m_max_task_count;
            }

            size_t size() const
            {
                readLock locker(m_tasks_mtx);
                return m_wait_tasks.size();
            }

        private:
            // 等待直到可加入新的任务至队列
            void wait_for_can_add()
            {
                std::unique_lock<std::mutex> locker(m_cv_mtx);
                m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
            }

            // 新增任务至队列
            bool add_task_tolist(TaskPtr&& new_task_item)
            {
                if (!new_task_item)
                    return false;

                {
                    writeLock locker(m_tasks_mtx);
                    m_wait_tasks.push_back(std::forward<TaskPtr>(new_task_item));
                }

                m_cv_not_empty.notify_one();
                return true;
            }

            // 新增当前运行属性
            // 已存在返回false，正常插入返回true
            bool add_cur_prop(PropType prop_type)
            {
                std::lock_guard<std::mutex> locker(m_props_mtx);
                if (m_cur_props.find(prop_type) != m_cur_props.end())
                    return false;
                m_cur_props.insert(prop_type);
                return true;
            }

            // 删除当前运行属性
            void remove_cur_prop(PropType prop_type)
            {
                std::lock_guard<std::mutex> locker(m_props_mtx);

                auto prop_iter = m_cur_props.find(prop_type);
                if (prop_iter != m_cur_props.end())
                    m_cur_props.erase(prop_iter);
            }

            // 是否处于未满状态
            bool not_full() const
            {
                return m_max_task_count == 0 || m_wait_tasks.size() < m_max_task_count;
            }

            // 是否处于空状态
            bool not_empty() const
            {
                return !m_wait_tasks.empty();
            }

        private:
            // 是否已终止标识符
            std::atomic<bool>           m_bstop;

            // 任务队列锁
            mutable rwMutex             m_tasks_mtx;
            // 总待执行任务队列,包含所有的待执行任务
            std::list<TaskPtr>          m_wait_tasks;
            // 最大任务个数,当为0时表示无限制
            size_t                      m_max_task_count;

            // 条件阻塞锁
            std::mutex                  m_cv_mtx;
            // 不为空的条件变量
            std::condition_variable     m_cv_not_empty;
            // 没有满的条件变量
            std::condition_variable     m_cv_not_full;

            // 当前执行任务属性队列锁
            std::mutex                  m_props_mtx;
            // 当前正在执行中的任务属性
            std::set<PropType>          m_cur_props;

        };

#pragma endregion

        enum {
            STP_MAX_THREAD = 2000,   // 最大线程数
        };

        BOOST_CONCEPT_ASSERT((boost::SGIAssignable<PropType>));         // 拷贝构造函数检查
        BOOST_CONCEPT_ASSERT((boost::DefaultConstructible<PropType>));  // 默认构造函数检查

    public:
        // 具有相同属性任务串行有序执行的线程池
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        SerialTaskPool(size_t max_task_count = 0)
            : m_cur_thread_ver(0)
            , m_task_list(max_task_count)
        {
        }

        ~SerialTaskPool()
        {
            stop();
        }

        // 是否已启动
        bool has_start() const
        {
            return m_atomic_switch.has_started();
        }

        // 是否已终止
        bool has_stop() const
        {
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

            m_task_list.stop();

            for (auto& thread : m_cur_thread) {
                delete thread;
                thread = nullptr;
            }
            m_cur_thread.clear();

            m_atomic_switch.store_start_flag(false);
        }

#if (defined __linux)
        template<typename Function, typename... Args>
        bool add_task(const PropType& prop, const Function& func, Args&... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_list.add_task(prop, func, args...);
        }
#else
        // 新增任务队列,超出最大任务数时存在阻塞
        template<typename Function, typename... Args>
        bool add_task(PropType&& prop, Function&& func, Args&&... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_list.add_task(std::forward<PropType>(prop), std::forward<Function>(func), std::forward<Args>(args)...);
        }

        template<typename Function, typename... Args>
        bool add_task(const PropType& prop, Function&& func, Args&&... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_list.add_task(prop, std::forward<Function>(func), std::forward<Args>(args)...);
        }
#endif

        void remove_prop(const PropType& prop)
        {
            m_task_list.remove_prop(prop);
        }

    private:
        // 创建线程
        void create_thread(size_t thread_num)
        {
            ++m_cur_thread_ver;
            thread_num = thread_num < STP_MAX_THREAD ? thread_num : STP_MAX_THREAD;
            for (size_t i = 0; i < thread_num; i++)
            {
                SafeThread* thd = new SafeThread(std::bind(&SerialTaskPool<PropType>::thread_fun, this, m_cur_thread_ver));
                m_cur_thread.push_back(thd);
            }
        }

        // 线程池线程
        void thread_fun(size_t thread_ver)
        {
            while (true)
            {
                if (m_atomic_switch.has_stoped())
                    break;

                {
                    std::lock_guard<std::mutex> lck(m_threads_mtx);
                    if (thread_ver < m_cur_thread_ver)
                        break;
                }

                m_task_list.pop_task();
            }
        }

    private:
        // 原子启停标志
        AtomicSwitch                m_atomic_switch;

        // 线程队列锁,使用递归锁,便于使用
        std::mutex                  m_threads_mtx;
        // 线程队列
        std::vector<SafeThread*>    m_cur_thread;
        // 当前设置线程版本号,每次重新设置线程数时,会递增该数值
        size_t                      m_cur_thread_ver;

        // 待执行任务队列
        SerialTaskList              m_task_list;
    };
}