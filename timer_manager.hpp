/************************************************************************
File name:  timer_manager.hpp
Author:	    AChar
Purpose:  时间轮定时器管理
Note:     插入时间复杂度: O(logN)
          删除时间复杂度: O(logN)
          每个时间插入后存在时间间隔的问题,会自动对应至下一个时间间隔,而不是完全一致的时间
          由于windows的最小时钟间隔为 0.5ms - 15.6001ms,故外界判断时应允许判断15.6001ms的误差
          实际测试中发现基本误差在1ms以内,向前漂移或者向后漂移
/************************************************************************/
#pragma once

#include <set>
#include <fstream>
#include <chrono>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include "task_pool.hpp"
#include "io_service_pool.hpp"  // 便于启停,可直接使用boost::asio::io_service
#include "atomic_switch.hpp"

namespace BTool {
    // 定时器管理器
    class TimerManager : private boost::noncopyable
    {
        typedef boost::asio::basic_waitable_timer<std::chrono::system_clock> my_system_timer;
        typedef std::shared_ptr<my_system_timer> timer_ptr;


    public:
        enum {
            INVALID_TID = TimerTaskVirtual::INVALID_TID, // 无效定时器ID
        };
        typedef TimerTaskVirtual::TimerId               TimerId;
        typedef TimerTaskVirtual::system_time_point     system_time_point;

    protected:
        typedef std::shared_ptr<TimerTask>      TimerTaskPtr;
//         typedef std::shared_ptr<TimerTaskVirtual>      TimerTaskPtr;

#pragma region 定时器队列
        /*************************************************
        Description:定时器队列,用于存储当前定时器排序,内部无锁,非线程安全!!!
        *************************************************/
        class TimerQueue : private boost::noncopyable
        {
            typedef std::map<TimerId, TimerTaskPtr> TimerMap;
        public:
            // 根据新增任务顺序并行有序执行的线程池
            // workers: 工作线程数,为0时默认系统核数
            // max_task_count: 最大任务缓存个数,超过该数量将产生阻塞;0则表示无限制
            TimerQueue(size_t workers = 0, size_t max_task_count = 0)
                : m_task_pool(max_task_count) {
                m_task_pool.start(workers);
            }

            ~TimerQueue() {
                clear();
                m_task_pool.stop();
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
//                 // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
//                 // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
//                 // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
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

            // 获取总定时器个数
            size_t size() const {
                return m_all_timer_map.size();
            }

            // 获取定时器切分时间片个数
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
            }

            bool empty() const {
                return m_all_timer_map.empty();
            }

            const TimerTaskPtr& front() const {
                auto bg_iter = m_timer_part_queue.begin();
                return bg_iter->second.begin()->second;
            }

            // 清空指定时间的相关任务,返回被清空的任务
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

            // 执行指定时间的相关任务,若任务无需下次循环则会被删除
            void loop_point_timer(const system_time_point& time_point) {
                auto timer_iter = m_timer_part_queue.find(time_point);
                if (timer_iter == m_timer_part_queue.end())
                    return;

                auto point_timer_queue = timer_iter->second;
                for (auto& point_item_pair : point_timer_queue) {
                    // 加入并发执行队列,并累计一次循环计数
                    auto& item = point_item_pair.second;
                    m_task_pool.add_task([item, time_point = item->get_time_point()] {
                        item->invoke(time_point);
                    });

                    item->loop_once_count();

                    // 若循环计数超过指定计数次数则删除该定时器对象
                    if (!item->need_next()) {
                        remove_from_all_map(item->get_id());
                        remove_from_part_queue(item->get_id(), item->get_time_point());
                    }
                    // 否则先删除分片定时器队列中原有对象,原有对象累计间隔时间,将累计后的时间加入下次分片队列中
                    else {
                        remove_from_part_queue(item->get_id(), item->get_time_point());
                        item->loop_once_time();
                        m_timer_part_queue[item->get_time_point()][item->get_id()] = item;
                    }
                }
            }

        private:
            // 从所有定时器任务队列中删除
            void remove_from_all_map(TimerId timer_id) {
                m_all_timer_map.erase(timer_id);
            }

            // 从所有定时器切分任务中删除
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
            TimerMap                                  m_all_timer_map;    // 存储所有定时器任务,牺牲新增时的些许性能,提高查询速度
            std::map<system_time_point, TimerMap>     m_timer_part_queue; // 对所有定时器的定时时间做切分,所有同一时刻定时任务置于同一timer_map中
            ParallelTaskPool                          m_task_pool;        // 定时器回调执行线程池
        };
#pragma endregion

    public:
        // 执行回调时的线程池数
        // workers: 回调执行工作线程数,为0时默认系统核数;注意此线程数并非定时器线程数,定时器线程始终只有一个
        // space_millsecond: 时间轮切片时间, 单位毫秒, 为0时则不切分,但不建议
        TimerManager(unsigned long long space_millsecond, int workers)
            : m_space_millsecond(space_millsecond)
            , m_ios_pool(1)
            , m_cur_task(nullptr)
            , m_next_id(INVALID_TID)
            , m_timer(nullptr)
            , m_timer_queue(workers)
        {
            m_atomic_switch.init();
        }

        ~TimerManager() {
            clear();
            stop();
        }

        void start() {
            if (!m_atomic_switch.start())
                return;

            m_ios_pool.start();
            m_timer = std::make_shared<my_system_timer>(m_ios_pool.get_io_service());
        }

        void stop() {
            if (!m_atomic_switch.stop())
                return;

            m_ios_pool.stop();

            m_atomic_switch.store_start_flag(false);
        }

        // 无循环定时器
        // dt: 触发时间点
        // insert_once(time_point, [param1, param2=...](BTool::TimerManager::TimerId id, const BTool::TimerManager::system_time_point& time_point){...})
        // insert_once(time_point, std::bind(&func, std::placeholders::_1, std::placeholders::_2, param1, param2))
        template<typename TFunction>
        TimerId insert_once(const system_time_point& time_point, TFunction&& func) {
            return insert(0, 1, time_point, std::forward<TFunction>(func));
        }
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
//         template<typename TFunction, typename... Args>
//         TimerId insert_once(const system_time_point& time_point, TFunction&& func, Args&&... args) {
//             return insert(0, 1, time_point, std::forward<TFunction>(func), std::forward<Args>(args)...);
//         }
        
        // interval_ms: 循环间隔时间,单位毫秒(注意该值会被时间轮间隔时间下取整,例如时间轮设定最小切片时间50ms,那么interval_ms设定为80时,实际interval_ms为100)
        // loop_count: 循环次数,0 表示无限循环
        // dt: 首次触发时间点
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
        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
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

        // 获取总定时器个数
        size_t size() const {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_timer_queue.size();
        }

        // 获取定时器切分时间片个数
        size_t time_point_size() const {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_timer_queue.time_point_size();
        }

        void erase(TimerId timer_id) {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            TimerTaskPtr erase_task = m_timer_queue.erase(timer_id);
            if (erase_task == m_cur_task)
                m_timer->cancel();
        }

        void clear() {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            m_timer_queue.clear();
            if(m_timer)
                m_timer->cancel();
        }

        // 清空指定时间下所有定时器
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

            // 存在提前触发的情况,改为expires_from_now
            m_timer->expires_at(timer_task->get_time_point());
            // expires_from_now同样存在一定误差
//             m_timer->expires_from_now(std::chrono::microseconds(std::chrono::duration_cast<std::chrono::microseconds>(timer_task->get_time_point() - std::chrono::system_clock::now()).count()));
            m_timer->async_wait(boost::bind(&TimerManager::handler, this, boost::placeholders::_1, timer_task, timer_task->get_time_point()));
        }

        void handler(const boost::system::error_code& error, const TimerTaskPtr& timer_task, const system_time_point& time_point) {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            m_cur_task = nullptr;

            if (!error) {
                m_timer_queue.loop_point_timer(time_point);
            }
            else { // 有错:1.被"cancel"了; 2.其他
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
        unsigned long long      m_space_millsecond;//切片时间,单位毫秒
        AsioServicePool         m_ios_pool;
        timer_ptr               m_timer;         // 定时器

        mutable std::mutex      m_queue_mtx;
        TimerTaskPtr            m_cur_task;     // 当前定时任务

        AtomicSwitch            m_atomic_switch;// 原子启停标志

        std::atomic<TimerId>	m_next_id;      // 下一个定时器ID
        TimerQueue              m_timer_queue;  // 定时器任务队列
    };
}