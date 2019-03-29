/*************************************************
File name:  serial_proplist_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供具有相同属性任务累计叠加执行的线程池
1, 每个属性都可以同时添加多个任务对象;
2, 对每个属性下的任务对象做统计,等到达时间后一次全部提取出来
3. 提供可扩展或缩容线程池数量功能。
*************************************************/
#pragma once


#include <list>
#include <vector>
#include <set>
#include <condition_variable>
#include <boost/bimap.hpp>
#include <boost/noncopyable.hpp>
#include <boost/concept_check.hpp>
#include "rwmutex.hpp"
#include "atomic_switch.hpp"
#include "safe_thread.hpp"

namespace BTool
{

    //////////////////////////////////////////////////////////////////////////
    // 串行有序按属性累计任务线程池
    template<typename PropType, typename JobType>
    class SerialPropListTaskPool : private boost::noncopyable
    {

#pragma region 任务相关操作
        class Task
        {
        public:
            Task() {}
            // 执行调用函数
            virtual void invoke(const PropType& prop, std::list<JobType>&& jobs) = 0;
        };
        typedef std::shared_ptr<Task> TaskPtr;

        template<typename Function, typename TTuple>
        class FuncTaskItem : public Task
        {
        public:
#if (defined __linux)
            FuncTaskItem(const Function& func, TTuple& t)
                : Task()
                , fun_cbk_(func)
                , tuple_(t)
            {
            }
#else
            FuncTaskItem(Function&& func, TTuple&& t)
                : Task()
                , fun_cbk_(std::forward<Function>(func))
                , tuple_(std::forward<TTuple>(t))
            {
            }
#endif

            // 执行调用函数
            void invoke(const PropType& prop, std::list<JobType>&& jobs)
            {
                constexpr auto size = std::tuple_size<typename std::decay<TTuple>::type>::value;
                invoke_impl(prop, std::forward<std::list<JobType>>(jobs), std::make_index_sequence<size>{});
            }

        private:
            template<std::size_t... Index>
            decltype(auto) invoke_impl(const PropType& prop, std::list<JobType>&& jobs, std::index_sequence<Index...>)
            {
                return fun_cbk_(prop, std::forward<std::list<JobType>>(jobs), std::get<Index>(std::forward<TTuple>(tuple_))...);
            }

        private:
            TTuple      tuple_;
            Function    fun_cbk_;
        };
#pragma endregion

#pragma region 任务队列

        //////////////////////////////////////////////////////////////////////////
        // 串行有序线程池待执行任务队列
        // PropType: 任务属性
        class SerialPropAccTaskList
        {
            typedef std::list<JobType> JobList;
        public:
            // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
            SerialPropAccTaskList(size_t max_task_count = 0)
                : m_max_task_count(max_task_count)
                , m_bstop(false)
            {
            }

            bool add_task(const PropType& prop, const JobType& job)
            {
                wait_for_can_add();

                if (m_bstop.load())
                    return false;

                {
                    writeLock locker(m_tasks_mtx);
                    auto left_iter = m_cur_props.left.find(prop);
                    if (left_iter != m_cur_props.left.end())
                    {
                        left_iter->second->push_back(job);
                    }
                    else
                    {
                        std::list<JobType> jobs;
                        jobs.push_back(job);
                        m_wait_tasks.push_back(jobs);
                        auto list_iter = m_wait_tasks.end();
                        list_iter--;
                        typedef boost::bimap<PropType, JobList*>::value_type value_t;
                        m_cur_props.insert(value_t(prop, &(*(list_iter))));
                    }
                }

                m_cv_not_empty.notify_one();
                return true;
            }

            // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
            void remove_prop(const PropType& prop)
            {
                writeLock locker(m_tasks_mtx);
                auto left_iter = m_cur_props.left.find(prop);
                if (left_iter != m_cur_props.left.end())
                {
                    left_iter->second->clear();
                    m_cur_props.erase(left_iter);
                }
                m_cv_not_full.notify_one();
            }

            // 移除一个顶层任务,队列为空时存在阻塞
            JobList pop_jobs(PropType& prop)
            {
                {
                    std::unique_lock<std::mutex> locker(m_cv_mtx);
                    m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
                }

                JobList jobs;
                if (m_bstop.load())
                    return jobs;

                TaskPtr next_task(nullptr);
                PropType prop_type;
                {
                    writeLock locker(m_tasks_mtx);
                    if(m_wait_tasks.empty())
                        return jobs;

                    JobList& tmp = m_wait_tasks.front();
                    auto right_iter = m_cur_props.right.find(&tmp);
                    prop = right_iter->second;
                    m_cur_props.right.erase(right_iter);

                    jobs.swap(tmp);
                    m_wait_tasks.pop_front();
                    m_cv_not_full.notify_one();
                }
                return jobs;
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
                    m_wait_tasks.push_back(new_task_item);
                }

                m_cv_not_empty.notify_one();
                return true;
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
            std::atomic<bool>                   m_bstop;

            // 任务队列锁
            mutable rwMutex                     m_tasks_mtx;
            // 总待执行任务队列,包含所有的待执行任务
            std::list<JobList>                  m_wait_tasks;
            boost::bimap<PropType, JobList*>    m_cur_props;
            // 最大任务个数,当为0时表示无限制
            size_t                              m_max_task_count;

            // 条件阻塞锁
            std::mutex                  m_cv_mtx;
            // 不为空的条件变量
            std::condition_variable     m_cv_not_empty;
            // 没有满的条件变量
            std::condition_variable     m_cv_not_full;
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
        SerialPropListTaskPool(size_t max_task_count = 0)
            : m_cur_thread_ver(0)
            , m_task_list(max_task_count)
        {
        }

        ~SerialPropListTaskPool() {
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
#if (defined __linux)
        // 开启线程池
        // thread_num: 开启线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        template<typename Function, typename... Args>
        void start(size_t thread_num, const Function& func, Args&... args)
        {
            if (!m_atomic_switch.start(false))
                return;

            typedef std::tuple<typename std::__strip_reference_wrapper<Args>::__type...> _Ttype;

            m_func_task = std::make_shared<FuncTaskItem<Function, _Ttype>>(func, std::make_tuple(args...));

            m_atomic_switch.store_stop_flag(false);

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }
#else
        // 开启线程池
        // thread_num: 开启线程数,最大为STP_MAX_THREAD个线程,0表示系统CPU核数
        template<typename Function, typename... Args>
        void start(size_t thread_num, Function&& func, Args&&... args)
        {
            if (!m_atomic_switch.start(false))
                return;

#  if (!defined _MSC_VER) || (_MSC_VER < 1900)
#    error "Low Compiler."
#  elif (_MSC_VER >= 1900 && _MSC_VER < 1915) // vs2015
            typedef std::tuple<typename std::_Unrefwrap<Args>::type...> _Ttype;
#  else
            typedef std::tuple<typename std::_Unrefwrap_t<Args>...> _Ttype;
#  endif

            m_func_task = std::make_shared<FuncTaskItem<Function, _Ttype>>(std::forward<Function>(func), std::make_tuple(std::forward<Args>(args)...));

            m_atomic_switch.store_stop_flag(false);

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }
#endif

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

        // 新增任务队列,超出最大任务数时存在阻塞
        bool add_task(const PropType& prop, const JobType& job)
        {
            if (!m_atomic_switch.has_started())
                return;

            return m_task_list.add_task(prop, job);
        }

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
                SafeThread* thd = new SafeThread(std::bind(&SerialPropListTaskPool<PropType, JobType>::thread_fun, this, m_cur_thread_ver));
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

                PropType cur_prop;
                m_func_task->invoke(cur_prop, std::move(m_task_list.pop_jobs(cur_prop)));
            }
        }

    private:
        // 原子启停标志
        AtomicSwitch                m_atomic_switch;

        std::shared_ptr<Task>       m_func_task;

        // 线程队列锁,使用递归锁,便于使用
        std::mutex                  m_threads_mtx;
        // 线程队列
        std::vector<SafeThread*>    m_cur_thread;
        // 当前设置线程版本号,每次重新设置线程数时,会递增该数值
        size_t                      m_cur_thread_ver;

        // 待执行任务队列
        SerialPropAccTaskList       m_task_list;
    };

}