/*************************************************
File name:  serial_proplist_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ������ͬ���������ۼƵ���ִ�е��̳߳�
1, ÿ�����Զ�����ͬʱ��Ӷ���������;
2, ��ÿ�������µ����������ͳ��,�ȵ���ʱ���һ��ȫ����ȡ����
3. �ṩ����չ�������̳߳��������ܡ�
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
    // �������������ۼ������̳߳�
    template<typename PropType, typename JobType>
    class SerialPropListTaskPool : private boost::noncopyable
    {

#pragma region ������ز���
        class Task
        {
        public:
            Task() {}
            // ִ�е��ú���
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

            // ִ�е��ú���
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

#pragma region �������

        //////////////////////////////////////////////////////////////////////////
        // ���������̳߳ش�ִ���������
        // PropType: ��������
        class SerialPropAccTaskList
        {
            typedef std::list<JobType> JobList;
        public:
            // max_task_count: ����������,��������������������;0���ʾ������
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

            // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
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

            // �Ƴ�һ����������,����Ϊ��ʱ��������
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
                // �Ƿ�����ֹ�ж�
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
            // �ȴ�ֱ���ɼ����µ�����������
            void wait_for_can_add()
            {
                std::unique_lock<std::mutex> locker(m_cv_mtx);
                m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
            }

            // ��������������
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

            // �Ƿ���δ��״̬
            bool not_full() const
            {
                return m_max_task_count == 0 || m_wait_tasks.size() < m_max_task_count;
            }

            // �Ƿ��ڿ�״̬
            bool not_empty() const
            {
                return !m_wait_tasks.empty();
            }

        private:
            // �Ƿ�����ֹ��ʶ��
            std::atomic<bool>                   m_bstop;

            // ���������
            mutable rwMutex                     m_tasks_mtx;
            // �ܴ�ִ���������,�������еĴ�ִ������
            std::list<JobList>                  m_wait_tasks;
            boost::bimap<PropType, JobList*>    m_cur_props;
            // ����������,��Ϊ0ʱ��ʾ������
            size_t                              m_max_task_count;

            // ����������
            std::mutex                  m_cv_mtx;
            // ��Ϊ�յ���������
            std::condition_variable     m_cv_not_empty;
            // û��������������
            std::condition_variable     m_cv_not_full;
        };

#pragma endregion

        enum {
            STP_MAX_THREAD = 2000,   // ����߳���
        };

        BOOST_CONCEPT_ASSERT((boost::SGIAssignable<PropType>));         // �������캯�����
        BOOST_CONCEPT_ASSERT((boost::DefaultConstructible<PropType>));  // Ĭ�Ϲ��캯�����

    public:
        // ������ͬ��������������ִ�е��̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        SerialPropListTaskPool(size_t max_task_count = 0)
            : m_cur_thread_ver(0)
            , m_task_list(max_task_count)
        {
        }

        ~SerialPropListTaskPool() {
            stop();
        }

        // �Ƿ�������
        bool has_start() const {
            return m_atomic_switch.has_started();
        }

        // �Ƿ�����ֹ
        bool has_stop() const {
            return m_atomic_switch.has_stoped();
        }
#if (defined __linux)
        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
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
        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
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

        // �����̳߳ظ���,ÿ����һ���߳�ʱ�����һ��ָ����ڴ�����(�߳���Դ���Զ��ͷ�),ִ��stop��������������������������
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        // ע��:���뿪���̳߳غ󷽿���Ч
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency())
        {
            if (!m_atomic_switch.has_started())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

        // ��ֹ�̳߳�
        // ע��˴���������,��ȫֹͣ�󷽿����¿���
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

        // �����������,�������������ʱ��������
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
        // �����߳�
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

        // �̳߳��߳�
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
        // ԭ����ͣ��־
        AtomicSwitch                m_atomic_switch;

        std::shared_ptr<Task>       m_func_task;

        // �̶߳�����,ʹ�õݹ���,����ʹ��
        std::mutex                  m_threads_mtx;
        // �̶߳���
        std::vector<SafeThread*>    m_cur_thread;
        // ��ǰ�����̰߳汾��,ÿ�����������߳���ʱ,���������ֵ
        size_t                      m_cur_thread_ver;

        // ��ִ���������
        SerialPropAccTaskList       m_task_list;
    };

}