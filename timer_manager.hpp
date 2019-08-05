/************************************************************************
File name:  timer_manager.hpp
Author:	    AChar
Purpose:  ��ʱ������
Note:     ����ʱ�临�Ӷ�: O(logN)
          ɾ��ʱ�临�Ӷ�: O(logN)
          ����windows����Сʱ�Ӽ��Ϊ 0.5ms - 15.6001ms,������ж�ʱӦ�����ж�15.6001ms�����
          ʵ�ʲ����з��ֻ��������1ms����,��ǰƯ�ƻ������Ư��,����ʵ��ʱ���ֿ��޸���ǰƯ�Ƶ�����,��ʱ���ֿ��ܴ���ʱ����Ƭ�Ӻ������
/************************************************************************/
#pragma once

#include <set>
#include <fstream>
#include <chrono>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include "task_pool.hpp"
#include "io_service_pool.hpp"  // ������ͣ,��ֱ��ʹ��boost::asio::io_service
#include "atomic_switch.hpp"

namespace BTool {
    // ��ʱ��������
    class TimerManager : private boost::noncopyable
    {
        typedef boost::asio::basic_waitable_timer<std::chrono::system_clock> my_system_timer;
        typedef std::shared_ptr<my_system_timer> timer_ptr;

    public:
        typedef std::chrono::system_clock::time_point	system_time_point;
#pragma region ID����

#if defined(_MSC_VER)
        typedef unsigned __int64			        TimerId;
#elif __GNUC__
#if __WORDSIZE == 64
        typedef unsigned long int 			        TimerId;
#else
        __extension__ typedef unsigned long long	TimerId;
#endif
#else
        typedef unsigned long long			        TimerId;
#endif

#pragma endregion

        enum {
            INVALID_TID = 0, // ��Ч��ʱ��ID
        };

    private:
#pragma region ������ز���
        class VirtualTimerTask
        {
        public:
            VirtualTimerTask()
                : id_(INVALID_TID)
                , loop_index_(0)
                , loop_count_(-1)
                , interval_(0)
            {
            }

            VirtualTimerTask(TimerId id, const system_time_point& time_point
                , unsigned int interval_ms, int loop_count)
                : id_(id)
                , time_point_(time_point)
                , loop_index_(0)
                , loop_count_(loop_count)
                , interval_(interval_ms)
            {
            }

            virtual ~VirtualTimerTask() {
            }

            TimerId get_id() const {
                return id_;
            }

            const system_time_point& get_time_point() const {
                return time_point_;
            }

            void cancel() {
                loop_count_ = -1;
            }

            // ִ��һ�μ���ѭ��
            void loop_once_count() {
                if (!need_next()) {
                    return;
                }
                if (loop_count_ != 0) {
                    ++loop_index_;
                }
            }

            // ִ��һ�ζ�ʱѭ��
            void loop_once_time() {
                time_point_ = time_point_ + std::chrono::milliseconds(interval_);
            }

            bool need_next() const {
                return loop_count_ == 0 || loop_index_ < loop_count_;
            }

            // ִ�е��ú���
            virtual void invoke(const system_time_point& time_point) = 0;

        private:
            TimerId                 id_;            // ��ʱ����ӦID
            int                     loop_count_;    // ��ѭ��������Ĭ��Ϊ1, ��ʾһ��ѭ����0��ʾ����ѭ��
            std::atomic<int>        loop_index_;    // ��ǰѭ������������ֵ
            unsigned int            interval_;      // ѭ�����ʱ��,��λ����
            system_time_point       time_point_;    // ��ʱ����ֹʱ��
        };
        typedef std::shared_ptr<VirtualTimerTask> timer_task_ptr;
        typedef std::map<TimerId, timer_task_ptr> timer_map;

        template<typename Function, typename TTuple>
        class TimerTaskItem : public VirtualTimerTask
        {
        public:
#if (defined __linux)
            TimerTaskItem(TimerId id, const system_time_point& time_point
                , unsigned int interval_ms, int loop_count, const Function& func, const TTuple& t)
                : VirtualTimerTask(id, time_point, interval_ms, loop_count)
                , fun_cbk_(func)
                , tuple_(t)
            {
            }
#else
            TimerTaskItem(TimerId id, const system_time_point& time_point
                , unsigned int interval_ms, int loop_count, Function&& func, TTuple&& t)
                : VirtualTimerTask(id, time_point, interval_ms, loop_count)
                , fun_cbk_(std::forward<Function>(func))
                , tuple_(std::forward<TTuple>(t))
            {
            }
#endif

            ~TimerTaskItem() {
            }

            // ִ�е��ú���
            void invoke(const system_time_point& time_point) override {
                constexpr auto size = std::tuple_size<typename std::decay<TTuple>::type>::value;
                invoke_impl(time_point, std::make_index_sequence<size>{});
            }

        private:
            // ִ�е��ú�������ʵ��
            template<std::size_t... Index>
            decltype(auto) invoke_impl(const system_time_point& time_point, std::index_sequence<Index...>)
            {
                return fun_cbk_(get_id(), time_point, std::get<Index>(std::forward<TTuple>(tuple_))...);
            }
        private:
            TTuple      tuple_;
            Function    fun_cbk_;
        };

#pragma endregion

#pragma region ��ʱ������
        // ��ʱ������,���ڴ洢��ǰ��ʱ������
        class TimerQueue : private boost::noncopyable
        {
        public:
            // ������������˳��������ִ�е��̳߳�
            // workers: �����߳���,Ϊ0ʱĬ��ϵͳ����
            // max_task_count: ������񻺴����,��������������������;0���ʾ������
            TimerQueue(size_t workers = 0, size_t max_task_count = 0)
                : m_task_pool(max_task_count)
            {
                m_task_pool.start(workers);
            }

            ~TimerQueue() {
                clear();
                m_task_pool.stop();
            }

            bool insert(const timer_task_ptr& pItem)
            {
                if(!pItem || pItem->get_id() == INVALID_TID)
                    return false;

                if (m_all_timer_map.find(pItem->get_id()) != m_all_timer_map.end())
                    return false;

                m_all_timer_map[pItem->get_id()] = pItem;
                m_timer_part_queue[pItem->get_time_point()][pItem->get_id()] = pItem;
                return true;
            }

            // ��ȡ�ܶ�ʱ������
            size_t size() const
            {
                return m_all_timer_map.size();
            }

            // ��ȡ��ʱ���з�ʱ��Ƭ����
            size_t time_point_size() const
            {
                return m_timer_part_queue.size();
            }

            timer_task_ptr erase(TimerId timer_id)
            {
                auto id_iter = m_all_timer_map.find(timer_id);
                if (id_iter == m_all_timer_map.end())
                    return timer_task_ptr();

                timer_task_ptr timer_task = id_iter->second;
                timer_task->cancel();

                remove_from_all_map(timer_id);
                remove_from_part_queue(timer_id, timer_task->get_time_point());
                return timer_task;
            }

            void clear()
            {
                m_all_timer_map.clear();
                m_timer_part_queue.clear();
            }

            bool empty() const
            {
                return m_all_timer_map.empty();
            }

            const timer_task_ptr& front() const
            {
                auto bg_iter = m_timer_part_queue.begin();
                return bg_iter->second.begin()->second;
            }

            // ���ָ��ʱ����������,���ر���յ�����
            std::set<timer_task_ptr> clear_point_timer(const system_time_point& time_point)
            {
                std::set<timer_task_ptr> rslt;

                auto timer_iter = m_timer_part_queue.find(time_point);
                if (timer_iter == m_timer_part_queue.end())
                    return rslt;

                auto& point_timer_queue = timer_iter->second;
                for (auto& item : point_timer_queue)
                {
                    timer_task_ptr& timer_task = item.second;
                    timer_task->cancel();
                    m_all_timer_map.erase(timer_task->get_id());
                    rslt.emplace(timer_task);
                }
                m_timer_part_queue.erase(time_point);
                return rslt;
            }

            // ִ��ָ��ʱ����������,�����������´�ѭ����ᱻɾ��
            void loop_point_timer(const system_time_point& time_point)
            {
                auto timer_iter = m_timer_part_queue.find(time_point);
                if (timer_iter == m_timer_part_queue.end())
                    return;

                auto point_timer_queue = timer_iter->second;
                for (auto& point_item_pair : point_timer_queue)
                {
                    auto& item = point_item_pair.second;
                    m_task_pool.add_task(std::bind(&TimerQueue::handler_task, this, item, item->get_time_point()));
                    item->loop_once_count();

                    if (!item->need_next())
                    {
                        remove_from_all_map(item->get_id());
                        remove_from_part_queue(item->get_id(), item->get_time_point());
                    }
                    else
                    {
                        remove_from_part_queue(item->get_id(), item->get_time_point());
                        item->loop_once_time();
                        m_timer_part_queue[item->get_time_point()][item->get_id()] = item;
                    }
                }
            }

        private:
            void remove_from_all_map(TimerId timer_id) {
                m_all_timer_map.erase(timer_id);
            }

            void remove_from_part_queue(TimerId timer_id, const system_time_point& time_point)
            {
                auto timer_iter = m_timer_part_queue.find(time_point);
                if (timer_iter == m_timer_part_queue.end())
                    return;

                auto& point_timer_queue = timer_iter->second;
                auto point_timer_iter = point_timer_queue.find(timer_id);
                if (point_timer_iter == point_timer_queue.end())
                    return;

                point_timer_queue.erase(point_timer_iter);
                if (point_timer_queue.empty())
                    m_timer_part_queue.erase(time_point);
            }

            void handler_task(const timer_task_ptr& timer_task, const system_time_point& time_point) {
                timer_task->invoke(time_point);
            }

        private:
            timer_map                               m_all_timer_map;    // �洢���ж�ʱ������,��������ʱ��Щ������,��߲�ѯ�ٶ�
            std::map<system_time_point, timer_map>  m_timer_part_queue; // �����ж�ʱ���Ķ�ʱʱ�����з�,����ͬһʱ�̶�ʱ��������ͬһtimer_map��
            ParallelTaskPool                        m_task_pool;        // ��ʱ���ص�ִ���̳߳�
        };
#pragma endregion

    public:
        // ִ�лص�ʱ���̳߳���
        // workers: �����߳���,Ϊ0ʱĬ��ϵͳ����
        TimerManager(int workers = 0)
            : m_ios_pool(1)
            , m_cur_task(nullptr)
            , m_next_id(INVALID_TID)
            , m_timer(nullptr)
            , m_timer_queue(workers)
        {
        }

        ~TimerManager() {
            clear();
            stop();
        }

        void start()
        {
            if (!m_atomic_switch.start())
                return;

            m_ios_pool.start();
            m_timer = std::make_shared<my_system_timer>(m_ios_pool.get_io_service());
        }

        void stop()
        {
            if (!m_atomic_switch.stop())
                return;

            m_ios_pool.stop();

            m_atomic_switch.store_start_flag(false);
        }

#if (defined __linux)
        // ��ѭ����ʱ��
        // dt: ����ʱ���
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename Function, typename... Args>
        TimerId insert(const system_time_point& dt, const Function& func, Args&... args)
        {
            return insert(0, 1, dt, func, args...);
        }

        // interval_ms: ѭ�����ʱ��,��λ����
        // loop_count: ѭ������,0 ��ʾ����ѭ��
        // dt: �״δ���ʱ���
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename Function, typename... Args>
        TimerId insert(unsigned int interval_ms, int loop_count, const system_time_point& dt, const Function& func, Args&... args)
        {
            if (!m_atomic_switch.has_started())
                return INVALID_TID;

            typedef std::tuple<typename std::__strip_reference_wrapper<Args>::__type...> _Ttype;
            timer_task_ptr pItem = std::make_shared<TimerTaskItem<Function, _Ttype>>(get_next_timer_id()
                , dt
                , interval_ms
                , loop_count
                , func
                , std::tie(args...));

            if (!pItem)
                return INVALID_TID;

            std::lock_guard<std::mutex> locker(m_queue_mtx);
            if (!m_timer_queue.insert(pItem))
                return INVALID_TID;

            if (!m_cur_task)
            {
                m_cur_task = pItem;
                start(pItem);
            }
            else if (m_cur_task != m_timer_queue.front())
            {
                m_timer->cancel();
            }

            return pItem->get_id();
        }
#else
        // ��ѭ����ʱ��
        // dt: ����ʱ���
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename Function, typename... Args>
        TimerId insert(const system_time_point& dt, Function&& func, Args&&... args)
        {
            return insert(0, 1, dt, std::forward<Function>(func), std::forward<Args>(args)...);
        }

        // interval_ms: ѭ�����ʱ��,��λ����
        // loop_count: ѭ������,0 ��ʾ����ѭ��
        // dt: �״δ���ʱ���
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename Function, typename... Args>
        TimerId insert(unsigned int interval_ms, int loop_count, const system_time_point& dt, Function&& func, Args&&... args)
        {
            if (!m_atomic_switch.has_started())
                return INVALID_TID;

#if (!defined _MSC_VER) || (_MSC_VER < 1900)
#    error "Low Compiler."
#elif (_MSC_VER >= 1900 && _MSC_VER < 1915) // vs2015
            typedef std::tuple<typename std::_Unrefwrap<Args>::type...> _Ttype;
#else
            typedef std::tuple<typename std::_Unrefwrap_t<Args>...> _Ttype;
#endif
            timer_task_ptr pItem = std::make_shared<TimerTaskItem<Function, _Ttype>>(get_next_timer_id()
                                    , dt
                                    , interval_ms
                                    , loop_count
                                    , std::forward<Function>(func)
                                    , std::tie(std::forward<Args>(args)...));

            if (!pItem)
                return INVALID_TID;

            std::lock_guard<std::mutex> locker(m_queue_mtx);
            if (!m_timer_queue.insert(pItem))
                return INVALID_TID;

            if (!m_cur_task)
            {
                m_cur_task = pItem;
                start(pItem);
            }
            else if (m_cur_task != m_timer_queue.front())
            {
                m_timer->cancel();
            }

            return pItem->get_id();
        }
#endif

        // ��ȡ�ܶ�ʱ������
        size_t size() const
        {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_timer_queue.size();
        }

        // ��ȡ��ʱ���з�ʱ��Ƭ����
        size_t time_point_size() const
        {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_timer_queue.time_point_size();
        }

        void erase(TimerId timer_id)
        {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            timer_task_ptr erase_task = m_timer_queue.erase(timer_id);
            if (erase_task == m_cur_task)
                m_timer->cancel();
        }

        void clear()
        {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            m_timer_queue.clear();
            if(m_timer)
                m_timer->cancel();
        }

        // ���ָ��ʱ�������ж�ʱ��
        void clear_point_timer(const system_time_point& time_point)
        {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            std::set<timer_task_ptr> erase_set = m_timer_queue.clear_point_timer(time_point);
            if (erase_set.find(m_cur_task) != erase_set.end())
                m_timer->cancel();
        }

    private:
        void start(const timer_task_ptr& timer_task)
        {
            if (!m_atomic_switch.has_started())
                return;

            // ������ǰ���������,��Ϊexpires_from_now
            m_timer->expires_at(timer_task->get_time_point());
            // expires_from_nowͬ������һ�����
//             m_timer->expires_from_now(std::chrono::microseconds(std::chrono::duration_cast<std::chrono::microseconds>(timer_task->get_time_point() - std::chrono::system_clock::now()).count()));
            m_timer->async_wait(boost::bind(&TimerManager::handler, this, boost::placeholders::_1, timer_task, timer_task->get_time_point()));
        }

        void handler(const boost::system::error_code& error, const timer_task_ptr& timer_task, const system_time_point& time_point)
        {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            m_cur_task = nullptr;

            if (!error) {
                m_timer_queue.loop_point_timer(time_point);
            }
            else { // �д�1.��"cancel"�ˣ�2.����
                if (error != boost::asio::error::operation_aborted) {
                    m_timer_queue.erase(timer_task->get_id());
                }
            }

            if (!m_timer_queue.empty()) {
                m_cur_task = m_timer_queue.front();
                start(m_cur_task);
            }
       }
       
        TimerId get_next_timer_id() {
            return ++m_next_id;
        }

    private:
        AsioServicePool         m_ios_pool;
        timer_ptr               m_timer;         // ��ʱ��

        mutable std::mutex      m_queue_mtx;
        timer_task_ptr          m_cur_task;     // ��ǰ��ʱ����

        AtomicSwitch            m_atomic_switch;// ԭ����ͣ��־

        std::atomic<TimerId>	m_next_id;      // ��һ����ʱ��ID
        TimerQueue              m_timer_queue;  // ��ʱ���������
    };
}