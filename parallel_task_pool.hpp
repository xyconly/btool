/*************************************************
File name:  parallel_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ��������ִ�е��̳߳�
1, ��ͬʱ��Ӷ������;
2, �����������Ⱥ�ִ��˳��,�����ܻ�ͬʱ����;
4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
5. �ṩ����չ�������̳߳��������ܡ�
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
    // �������������
    class ParallelTaskPool : private boost::noncopyable
    {
        enum {
            STP_MAX_THREAD = 2000,   // ����߳���
        };

#pragma region �������
        class ParallelTaskQueue
        {
#pragma region ������ز���
            class VirtualTask
            {
            public:
                VirtualTask() {}
                virtual ~VirtualTask() {}

                // ִ�е��ú���
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

                // ִ�е��ú���
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
            // max_task_count: ����������,��������������������;0���ʾ������
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

            // �Ƴ�һ������ǵ�ǰִ����������,����Ϊ��ʱ��������
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
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelTaskPool(size_t max_task_count = 0)
            : m_cur_thread_ver(0)
            , m_task_queue(max_task_count)
        {
        }

        ~ParallelTaskPool() {
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
        // �����������,�������������ʱ��������
        template<typename Function, typename... Args>
        bool add_task(const Function& func, Args... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_queue.add_task(func, args...);
        }
#else
        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename Function, typename... Args>
        bool add_task(Function&& func, Args&&... args)
        {
            if (!m_atomic_switch.has_started())
                return false;

            return m_task_queue.add_task(std::forward<Function>(func), std::forward<Args>(args)...);
        }
#endif

    private:
        // �����߳�
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

        // �̳߳��߳�
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
        // ԭ����ͣ��־
        AtomicSwitch                m_atomic_switch;

        // �̶߳�����
        std::mutex                  m_threads_mtx;
        // �̶߳���
        std::vector<SafeThread*>    m_cur_thread;
        // ��ǰ�����̰߳汾��,ÿ�����������߳���ʱ,���������ֵ
        size_t                      m_cur_thread_ver;

        // ��ִ���������
        ParallelTaskQueue           m_task_queue;
    };
}