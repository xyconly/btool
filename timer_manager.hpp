/************************************************************************
File name:  timer_manager.hpp
Author:	    AChar
Purpose:  ʱ���ֶ�ʱ������
Note:     ����ʱ�临�Ӷ�: O(logN)
          ɾ��ʱ�临�Ӷ�: O(logN)
          ÿ��ʱ���������ʱ����������,���Զ���Ӧ����һ��ʱ����,��������ȫһ�µ�ʱ��
          ����windows����Сʱ�Ӽ��Ϊ 0.5ms - 15.6001ms,������ж�ʱӦ�����ж�15.6001ms�����
          ʵ�ʲ����з��ֻ��������1ms����,��ǰƯ�ƻ������Ư��

ע������: ���ص�ִ�еĺ���������ʱ����ʱ��ʱ,erase�޷�����ִ�еĴ���,��:
          workers==1�����,���1sִ�в���A,����A�����ʱ2��,
          ��ô��ʱ,���10���ִ��erase,�����ɻ�ִ��10�β���A,
          �ʶ���ִ��ʱ,Ӧ������ȷ���첽����ִ��ʱ�䲻�ø��ڼ��ʱ��,��ʹ��stop/clear������ֹ
************************************************************************/
#pragma once

#include <set>
#include <fstream>
#include <chrono>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include "task_item.hpp"
#include "task_pool.hpp"
#include "io_context_pool.hpp"  // ������ͣ,��ֱ��ʹ��boost::asio::io_context
#include "atomic_switch.hpp"

namespace BTool {
    // ��ʱ��������
    class TimerManager : private boost::noncopyable
    {
        typedef boost::asio::basic_waitable_timer<std::chrono::system_clock> my_system_timer;
        typedef std::shared_ptr<my_system_timer> timer_ptr;

    public:
        enum {
            INVALID_TID = TimerTaskVirtual::INVALID_TID, // ��Ч��ʱ��ID
        };
        typedef TimerTaskVirtual::TimerId               TimerId;
        typedef TimerTaskVirtual::system_time_point     system_time_point;

    protected:
        typedef std::shared_ptr<TimerTask>      TimerTaskPtr;
//         typedef std::shared_ptr<TimerTaskVirtual>      TimerTaskPtr;

   /**************   ��ʱ������  ******************/
        /*************************************************
        Description:��ʱ������,���ڴ洢��ǰ��ʱ������,�ڲ�����,���̰߳�ȫ!!!
        *************************************************/
        class TimerQueue : private boost::noncopyable
        {
            typedef std::map<TimerId, TimerTaskPtr> TimerMap;
        public:
            // ������������˳��������ִ�е��̳߳�
            // workers: �����߳���,Ϊ0ʱĬ��ϵͳ����
            // max_task_count: ������񻺴����,��������������������;0���ʾ������
            TimerQueue(size_t workers = 0, size_t max_task_count = 0)
                : m_task_pool(max_task_count) {
                m_task_pool.start(workers);
            }

            ~TimerQueue() {
                clear();
            }

            template<typename TFunction>
            TimerTaskPtr insert(unsigned int interval_ms, int loop_count, TimerId id
                , const system_time_point& time_point, TFunction&& func) {
                if (id == TimerTaskVirtual::INVALID_TID)
                    return nullptr;

                if (m_all_timer_map.find(id) != m_all_timer_map.end())
                    return nullptr;

                auto pItem = std::make_shared<TimerTask>(interval_ms, loop_count, id, time_point, std::forward<TFunction>(func));
                if (!pItem || pItem->get_id() == TimerTaskVirtual::INVALID_TID)
                    return nullptr;

                m_all_timer_map[id] = pItem;
                m_timer_part_queue[pItem->get_time_point()][id] = pItem;
                return pItem;
            }
//             template<typename TFunction, typename... Args>
//             TimerTaskPtr insert(unsigned int interval_ms, int loop_count, TimerId id
//                 , const system_time_point& time_point, TFunction&& func, Args&&... args) {
//                 if (id == TimerTaskVirtual::INVALID_TID)
//                     return nullptr;
//
//                 if (m_all_timer_map.find(id) != m_all_timer_map.end())
//                     return nullptr;
//
// //                 auto pItem = std::make_shared<PackagedTimerTask>(interval_ms, loop_count, id, time_point
// //                     , std::forward<TFunction>(func)
// //                     , std::forward<Args>(args)...);
//
//                 // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
//                 // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
//                 // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
//                 typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
//                 auto pItem = std::make_shared<TupleTimerTask<TFunction, TTuple>>(interval_ms, loop_count, id, time_point
//                     , std::forward<TFunction>(func)
//                     , std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...)));
//
//                 if (!pItem || pItem->get_id() == TimerTaskVirtual::INVALID_TID)
//                     return nullptr;
//
//                 m_all_timer_map[pItem->get_id()] = pItem;
//                 m_timer_part_queue[pItem->get_time_point()][pItem->get_id()] = pItem;
//                 return pItem;
//             }

            // ��ȡ�ܶ�ʱ������
            size_t size() const {
                return m_all_timer_map.size();
            }

            // ��ȡ��ʱ���з�ʱ��Ƭ����
            size_t time_point_size() const {
                return m_timer_part_queue.size();
            }

            TimerTaskPtr erase(TimerId timer_id) {
                auto id_iter = m_all_timer_map.find(timer_id);
                if (id_iter == m_all_timer_map.end())
                    return TimerTaskPtr();

                TimerTaskPtr timer_task = id_iter->second;
                timer_task->cancel();

                remove_from_all_map(timer_id);
                remove_from_part_queue(timer_id, timer_task->get_time_point());
                return timer_task;
            }

            void clear() {
                m_all_timer_map.clear();
                m_timer_part_queue.clear();
                m_task_pool.clear();
            }

            bool empty() const {
                return m_all_timer_map.empty();
            }

            const TimerTaskPtr& front() const {
                auto bg_iter = m_timer_part_queue.begin();
                return bg_iter->second.begin()->second;
            }

            // ���ָ��ʱ����������,���ر���յ�����
            std::set<TimerTaskPtr> clear_point_timer(const system_time_point& time_point) {
                std::set<TimerTaskPtr> rslt;

                auto timer_iter = m_timer_part_queue.find(time_point);
                if (timer_iter == m_timer_part_queue.end())
                    return rslt;

                auto& point_timer_queue = timer_iter->second;
                for (auto& item : point_timer_queue) {
                    TimerTaskPtr& timer_task = item.second;
                    timer_task->cancel();
                    m_all_timer_map.erase(timer_task->get_id());
                    rslt.emplace(timer_task);
                }
                m_timer_part_queue.erase(time_point);
                return rslt;
            }

            // ִ��ָ��ʱ����������,�����������´�ѭ����ᱻɾ��
            void loop_point_timer(const system_time_point& time_point) {
                auto timer_iter = m_timer_part_queue.find(time_point);
                if (timer_iter == m_timer_part_queue.end())
                    return;

                auto point_timer_queue = timer_iter->second;
                for (auto& point_item_pair : point_timer_queue) {
                    // ���벢��ִ�ж���,���ۼ�һ��ѭ������
                    auto& item = point_item_pair.second;
                    m_task_pool.add_task([item, time_point = item->get_time_point()] {
                        item->invoke(time_point);
                    });

                    item->loop_once_count();

                    // ��ѭ����������ָ������������ɾ���ö�ʱ������
                    if (!item->need_next()) {
                        remove_from_all_map(item->get_id());
                        remove_from_part_queue(item->get_id(), item->get_time_point());
                    }
                    // ������ɾ����Ƭ��ʱ��������ԭ�ж���,ԭ�ж����ۼƼ��ʱ��,���ۼƺ��ʱ������´η�Ƭ������
                    else {
                        remove_from_part_queue(item->get_id(), item->get_time_point());
                        item->loop_once_time();
                        m_timer_part_queue[item->get_time_point()][item->get_id()] = item;
                    }
                }
            }

        private:
            // �����ж�ʱ�����������ɾ��
            void remove_from_all_map(TimerId timer_id) {
                m_all_timer_map.erase(timer_id);
            }

            // �����ж�ʱ���з�������ɾ��
            void remove_from_part_queue(TimerId timer_id, const system_time_point& time_point) {
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

            void handler_task(const TimerTaskPtr& timer_task, const system_time_point& time_point) {
                timer_task->invoke(time_point);
            }

        private:
            TimerMap                                  m_all_timer_map;    // �洢���ж�ʱ������,��������ʱ��Щ������,��߲�ѯ�ٶ�
            std::map<system_time_point, TimerMap>     m_timer_part_queue; // �����ж�ʱ���Ķ�ʱʱ�����з�,����ͬһʱ�̶�ʱ��������ͬһtimer_map��
            ParallelTaskPool                          m_task_pool;        // ��ʱ���ص�ִ���̳߳�
        };

    public:
        // ִ�лص�ʱ���̳߳���
        // space_millsecond: ʱ������Ƭʱ��, ��λ����, Ϊ0ʱ���з�,��������
        // workers: �ص�ִ�й����߳���,Ϊ0ʱĬ��ϵͳ����;ע����߳������Ƕ�ʱ���߳���,��ʱ���߳�ʼ��ֻ��һ��
        TimerManager(unsigned long long space_millsecond, int workers)
            : m_space_millsecond(space_millsecond)
            , m_ioc_pool(1)
            , m_timer(nullptr)
            , m_cur_task(nullptr)
            , m_next_id(INVALID_TID)
            , m_timer_queue(workers)
        {
        }

        ~TimerManager() {
            stop();
        }

        void start() {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            m_ioc_pool.start();
            m_timer = std::make_shared<my_system_timer>(m_ioc_pool.get_io_context());
        }

        void stop() {
            if (!m_atomic_switch.stop())
                return;

            m_ioc_pool.stop();
            clear();

            m_atomic_switch.reset();
        }

        // ѭ������������ʱ��
        // interval_ms: ѭ�����ʱ��,��λ����(ע���ֵ�ᱻʱ���ּ��ʱ����ȡ��,����ʱ�����趨��С��Ƭʱ��50ms,��ôinterval_ms�趨Ϊ80ʱ,ʵ��interval_msΪ100)
        // loop_count: ѭ������,0 ��ʾ����ѭ��
        // insert_now(interval_ms, loop_count, [param1, param2=...](BTool::TimerManager::TimerId id, const BTool::TimerManager::system_time_point& time_point){...})
        // insert_now(interval_ms, loop_count, std::bind(&func, std::placeholders::_1, std::placeholders::_2, param1, param2))
        template<typename TFunction>
        TimerId insert_now(unsigned int interval_ms, int loop_count, TFunction&& func) {
            return insert(interval_ms, loop_count, std::chrono::system_clock::now(), std::forward<TFunction>(func));
        }
        // ��ѭ������������ʱ��
        // insert_now_once([param1, param2=...](BTool::TimerManager::TimerId id, const BTool::TimerManager::system_time_point& time_point){...})
        // insert_now_once(std::bind(&func, std::placeholders::_1, std::placeholders::_2, param1, param2))
        template<typename TFunction>
        TimerId insert_now_once(TFunction&& func) {
            return insert(0, 1, std::chrono::system_clock::now(), std::forward<TFunction>(func));
        }

        // ѭ��ָ��Ư��ʱ�䴥����ʱ��
        // interval_ms: ѭ�����ʱ��,��λ����(ע���ֵ�ᱻʱ���ּ��ʱ����ȡ��,����ʱ�����趨��С��Ƭʱ��50ms,��ôinterval_ms�趨Ϊ80ʱ,ʵ��interval_msΪ100)
        // loop_count: ѭ������,0 ��ʾ����ѭ��
        // duration_ms: �״δ���ʱ�����뵱ǰʱ���Ư��ʱ��,��λ����
        // insert_duration(interval_ms, loop_count, duration_ms, [param1, param2=...](BTool::TimerManager::TimerId id, const BTool::TimerManager::system_time_point& time_point){...})
        // insert_duration(interval_ms, loop_count, duration_ms, std::bind(&func, std::placeholders::_1, std::placeholders::_2, param1, param2))
        template<typename TFunction>
        TimerId insert_duration(unsigned int interval_ms, int loop_count, unsigned int duration_ms, TFunction&& func) {
            return insert(interval_ms, loop_count, std::chrono::system_clock::now() + std::chrono::milliseconds(duration_ms), std::forward<TFunction>(func));
        }
        // ��ѭ��ָ��Ư��ʱ�䴥����ʱ��
        // duration_ms: ����ʱ�����뵱ǰʱ���Ư��ʱ��,��λ����
        // insert_duration_once(duration_ms, [param1, param2=...](BTool::TimerManager::TimerId id, const BTool::TimerManager::system_time_point& time_point){...})
        // insert_duration_once(duration_ms, std::bind(&func, std::placeholders::_1, std::placeholders::_2, param1, param2))
        template<typename TFunction>
        TimerId insert_duration_once(unsigned int duration_ms, TFunction&& func) {
            return insert(0, 1, std::chrono::system_clock::now() + std::chrono::milliseconds(duration_ms), std::forward<TFunction>(func));
        }

        // ��ѭ����ʱ��
        // time_point: ����ʱ���
        // insert_once(time_point, [param1, param2=...](BTool::TimerManager::TimerId id, const BTool::TimerManager::system_time_point& time_point){...})
        // insert_once(time_point, std::bind(&func, std::placeholders::_1, std::placeholders::_2, param1, param2))
        template<typename TFunction>
        TimerId insert_once(const system_time_point& time_point, TFunction&& func) {
            return insert(0, 1, time_point, std::forward<TFunction>(func));
        }
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
//         template<typename TFunction, typename... Args>
//         TimerId insert_once(const system_time_point& time_point, TFunction&& func, Args&&... args) {
//             return insert(0, 1, time_point, std::forward<TFunction>(func), std::forward<Args>(args)...);
//         }

        // interval_ms: ѭ�����ʱ��,��λ����(ע���ֵ�ᱻʱ���ּ��ʱ����ȡ��,����ʱ�����趨��С��Ƭʱ��50ms,��ôinterval_ms�趨Ϊ80ʱ,ʵ��interval_msΪ100)
        // loop_count: ѭ������,0 ��ʾ����ѭ��
        // time_point: �״δ���ʱ���
        // insert(interval_ms, loop_count, time_point, [param1, param2=...](BTool::TimerManager::TimerId id, const BTool::TimerManager::system_time_point& time_point){...})
        // insert(interval_ms, loop_count, time_point, std::bind(&func, std::placeholders::_1, std::placeholders::_2, param1, param2))
        template<typename TFunction>
        TimerId insert(unsigned int interval_ms, int loop_count, const system_time_point& time_point, TFunction&& func)
        {
            if (!m_atomic_switch.has_started())
                return INVALID_TID;

            system_time_point revise_time_point = time_point;
            if (m_space_millsecond > 1) {
                auto tmp_count = std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
                auto tmp_time_point = (tmp_count + m_space_millsecond - 1) / m_space_millsecond * m_space_millsecond;
                revise_time_point = system_time_point(std::chrono::milliseconds(tmp_time_point));

                interval_ms = (interval_ms + m_space_millsecond - 1) / m_space_millsecond * m_space_millsecond;
            }

            std::lock_guard<std::mutex> locker(m_queue_mtx);
            auto pItem = m_timer_queue.insert(interval_ms, loop_count, get_next_timer_id(), revise_time_point, std::forward<TFunction>(func));

            if (!pItem)
                return INVALID_TID;

            if (!m_cur_task) {
                m_cur_task = pItem;
                start(pItem);
            }
            else if (m_cur_task != m_timer_queue.front()) {
                m_timer->cancel();
            }

            return pItem->get_id();
        }
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
//         template<typename TFunction, typename... Args>
//         TimerId insert(unsigned int interval_ms, int loop_count, const system_time_point& time_point, TFunction&& func, Args&&... args)
//         {
//             if (!m_atomic_switch.has_started())
//                 return INVALID_TID;
//
//             std::lock_guard<std::mutex> locker(m_queue_mtx);
//             auto pItem = m_timer_queue.insert(interval_ms, loop_count, get_next_timer_id(), time_point
//                 , std::forward<TFunction>(func) , std::forward<Args>(args)...);
//
//             if (!pItem)
//                 return INVALID_TID;
//
//             if (!m_cur_task)
//             {
//                 m_cur_task = pItem;
//                 start(pItem);
//             }
//             else if (m_cur_task != m_timer_queue.front())
//             {
//                 m_timer->cancel();
//             }
//
//             return pItem->get_id();
//         }

        // ��ȡ�ܶ�ʱ������
        size_t size() const {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_timer_queue.size();
        }

        // ��ȡ��ʱ���з�ʱ��Ƭ����
        size_t time_point_size() const {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_timer_queue.time_point_size();
        }

        void erase(TimerId timer_id) {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            TimerTaskPtr erase_task = m_timer_queue.erase(timer_id);
            if (erase_task && erase_task == m_cur_task)
                m_timer->cancel();
        }

        void clear() {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            m_timer_queue.clear();
            if(m_timer)
                m_timer->cancel();
            m_cur_task = nullptr;
        }

        // ���ָ��ʱ�������ж�ʱ��
        void clear_point_timer(const system_time_point& time_point) {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            std::set<TimerTaskPtr> erase_set = m_timer_queue.clear_point_timer(time_point);
            if (erase_set.find(m_cur_task) != erase_set.end())
                m_timer->cancel();
        }

    private:
        void start(const TimerTaskPtr& timer_task) {
            if (!m_atomic_switch.has_started())
                return;

            // ������ǰ���������,��Ϊexpires_from_now
            m_timer->expires_at(timer_task->get_time_point());
            // expires_from_nowͬ������һ�����
//             m_timer->expires_from_now(std::chrono::microseconds(std::chrono::duration_cast<std::chrono::microseconds>(timer_task->get_time_point() - std::chrono::system_clock::now()).count()));
            m_timer->async_wait(std::bind(&TimerManager::handler, this, std::placeholders::_1, timer_task, timer_task->get_time_point()));
        }

        void handler(const boost::system::error_code& error, const TimerTaskPtr& timer_task, const system_time_point& time_point) {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            m_cur_task = nullptr;

            if (!error) {
                m_timer_queue.loop_point_timer(time_point);
            }
            else { // �д�:1.��"cancel"��; 2.����
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
        unsigned long long      m_space_millsecond;//��Ƭʱ��,��λ����
        AsioContextPool         m_ioc_pool;
        timer_ptr               m_timer;         // ��ʱ��

        mutable std::mutex      m_queue_mtx;
        TimerTaskPtr            m_cur_task;     // ��ǰ��ʱ����

        AtomicSwitch            m_atomic_switch;// ԭ����ͣ��־

        std::atomic<TimerId>	m_next_id;      // ��һ����ʱ��ID
        TimerQueue              m_timer_queue;  // ��ʱ���������
    };
}