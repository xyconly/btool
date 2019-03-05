/*************************************************
File name:  serial_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ������ͬ��������������ִ�е��̳߳�
1, ÿ�����Զ�����ͬʱ��Ӷ������;
2, �кܶ�����Ժͺܶ������;
3, ÿ��������ӵ��������������ִ��,����ͬһʱ�̲�����ͬʱִ��һ���û�����������;
4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
5. �ṩ����չ�������̳߳��������ܡ�
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
    // ���������̳߳�
    template<typename PropType>
    class SerialTaskPool : private boost::noncopyable
    {
#pragma region �������

        //////////////////////////////////////////////////////////////////////////
        // ���������̳߳ش�ִ���������
        // PropType: ��������
        class SerialTaskList
        {
#pragma region ������ز���
            class Task
            {
            public:
#if (defined __linux)
#else
                Task(PropType&& prop) :m_prop(std::forward<PropType>(prop)) {}
#endif
                Task(const PropType& prop) :m_prop(prop) {}

                // ��ȡ��������
                const PropType& get_prop_type() const {
                    return m_prop;
                }

                // ִ�е��ú���
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

                // ִ�е��ú���
                void invoke()
                {
                    constexpr auto size = std::tuple_size<typename std::decay<TTuple>::type>::value;
                    invoke_impl(std::make_index_sequence<size>{});
                }

            private:
                // ִ�е��ú�������ʵ��
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
            // max_task_count: ����������,��������������������;0���ʾ������
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

            // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
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

            // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
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

            // �Ƴ�һ������ǵ�ǰִ����������,����Ϊ��ʱ��������
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
                    m_wait_tasks.push_back(std::forward<TaskPtr>(new_task_item));
                }

                m_cv_not_empty.notify_one();
                return true;
            }

            // ������ǰ��������
            // �Ѵ��ڷ���false���������뷵��true
            bool add_cur_prop(PropType prop_type)
            {
                std::lock_guard<std::mutex> locker(m_props_mtx);
                if (m_cur_props.find(prop_type) != m_cur_props.end())
                    return false;
                m_cur_props.insert(prop_type);
                return true;
            }

            // ɾ����ǰ��������
            void remove_cur_prop(PropType prop_type)
            {
                std::lock_guard<std::mutex> locker(m_props_mtx);

                auto prop_iter = m_cur_props.find(prop_type);
                if (prop_iter != m_cur_props.end())
                    m_cur_props.erase(prop_iter);
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
            std::atomic<bool>           m_bstop;

            // ���������
            mutable rwMutex             m_tasks_mtx;
            // �ܴ�ִ���������,�������еĴ�ִ������
            std::list<TaskPtr>          m_wait_tasks;
            // ����������,��Ϊ0ʱ��ʾ������
            size_t                      m_max_task_count;

            // ����������
            std::mutex                  m_cv_mtx;
            // ��Ϊ�յ���������
            std::condition_variable     m_cv_not_empty;
            // û��������������
            std::condition_variable     m_cv_not_full;

            // ��ǰִ���������Զ�����
            std::mutex                  m_props_mtx;
            // ��ǰ����ִ���е���������
            std::set<PropType>          m_cur_props;

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
        SerialTaskPool(size_t max_task_count = 0)
            : m_cur_thread_ver(0)
            , m_task_list(max_task_count)
        {
        }

        ~SerialTaskPool()
        {
            stop();
        }

        // �Ƿ�������
        bool has_start() const
        {
            return m_atomic_switch.has_started();
        }

        // �Ƿ�����ֹ
        bool has_stop() const
        {
            return m_atomic_switch.has_stoped();
        }

        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        void start(size_t thread_num = std::thread::hardware_concurrency())
        {
            if (!m_atomic_switch.start())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

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

#if (defined __linux)
        template<typename Function, typename... Args>
        bool add_task(const PropType& prop, const Function& func, Args&... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_list.add_task(prop, func, args...);
        }
#else
        // �����������,�������������ʱ��������
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
        // �����߳�
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

                m_task_list.pop_task();
            }
        }

    private:
        // ԭ����ͣ��־
        AtomicSwitch                m_atomic_switch;

        // �̶߳�����,ʹ�õݹ���,����ʹ��
        std::mutex                  m_threads_mtx;
        // �̶߳���
        std::vector<SafeThread*>    m_cur_thread;
        // ��ǰ�����̰߳汾��,ÿ�����������߳���ʱ,���������ֵ
        size_t                      m_cur_thread_ver;

        // ��ִ���������
        SerialTaskList              m_task_list;
    };
}