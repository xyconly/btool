/*************************************************
File name:  job_queue.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ����FIFO����
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
        BOOST_CONCEPT_ASSERT((boost::SGIAssignable<JobType>));         // �������캯�����
        BOOST_CONCEPT_ASSERT((boost::DefaultConstructible<JobType>));  // Ĭ�Ϲ��캯�����

         // ��ֹ����
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

        // �Ƴ�һ����������,����Ϊ��ʱ��������
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

        // ����������ʽ�Ƴ�һ����������
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
            // �Ƿ�����ֹ�ж�
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
        // �Ƿ���δ��״̬
        bool not_full() const {
            return m_max_task_count == 0 || m_queue.size() < m_max_task_count;
        }

        // �Ƿ��ڿ�״̬
        bool not_empty() const {
            return !m_queue.empty();
        }

    private:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;

        // ���������
        mutable std::mutex          m_queue_mtx;
        // �ܴ�ִ���������,�������еĴ�ִ������
        std::queue<JobType>         m_queue;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                      m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable     m_cv_not_empty;
        // û��������������
        std::condition_variable     m_cv_not_full;
    };
}