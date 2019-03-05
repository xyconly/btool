/*************************************************
File name:  job_queue.hpp
Author:     AChar
Version:
Date:
Description:    提供有序FIFO队列
*************************************************/
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <boost/concept_check.hpp>

namespace BTool
{
    template<typename JobType>
    class JobQueue
    {
        BOOST_CONCEPT_ASSERT((boost::SGIAssignable<JobType>));         // 拷贝构造函数检查
        BOOST_CONCEPT_ASSERT((boost::DefaultConstructible<JobType>));  // 默认构造函数检查

         // 禁止拷贝
        JobQueue(const JobQueue&) = delete;
        JobQueue& operator=(const JobQueue&) = delete;
    public:
        JobQueue(size_t max_task_count = 0)
            : m_max_task_count(max_task_count)
            , m_bstop(false)
        {
        }

        virtual ~JobQueue() {
            stop();
        }

        bool push(const JobType& job)
        {
            std::unique_lock<std::mutex> locker(m_queue_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });

            if (m_bstop.load()) {
                return false;
            }
            m_queue.push(job);
            m_cv_not_empty.notify_one();
            return true;
        }

        // 移除一个顶层任务,队列为空时存在阻塞
        bool pop(JobType& job)
        {
            std::unique_lock<std::mutex> locker(m_queue_mtx);
            m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });

            if (m_bstop.load()) {
                return false;
            }
            if (m_queue.empty()) {
                return false;
            }
            job = m_queue.front();
            m_queue.pop();
            m_cv_not_full.notify_one();
            return true;
        }

        // 非阻塞的形式移除一个顶层任务
        bool pop_nowait(JobType& job)
        {
            std::unique_lock<std::mutex> locker(m_queue_mtx);
            if (m_queue.empty() || m_bstop.load()) {
                return false;
            }
            job = m_queue.front();
            m_queue.pop();
            m_cv_not_full.notify_one();
            return true;
        }

        void stop() 
        {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void clear() {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            std::queue<JobType> empty;
            m_queue.swap(empty);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        bool empty() const {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_queue.empty();
        }

        bool full() const {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_max_task_count != 0 && m_queue.size() >= m_max_task_count;
        }

        size_t size() const {
            std::lock_guard<std::mutex> locker(m_queue_mtx);
            return m_queue.size();
        }

    protected:
        // 是否处于未满状态
        bool not_full() const {
            return m_max_task_count == 0 || m_queue.size() < m_max_task_count;
        }

        // 是否处于空状态
        bool not_empty() const {
            return !m_queue.empty();
        }

    private:
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;

        // 任务队列锁
        mutable std::mutex          m_queue_mtx;
        // 总待执行任务队列,包含所有的待执行任务
        std::queue<JobType>         m_queue;
        // 最大任务个数,当为0时表示无限制
        size_t                      m_max_task_count;

        // 不为空的条件变量
        std::condition_variable     m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable     m_cv_not_full;
    };
}