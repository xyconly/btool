/*************************************************
File name:  work_queue.hpp
Author:     AChar
Version:
Date:
Purpose: ��boost::asio::io_service��װ,ʵ���̳߳��������
Note:    �������������뺬��void operator()()������ִ�лص�
*************************************************/
#pragma once

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <type_traits>

namespace BTool {

    class WorkQueue
    {
    public:
        // workers: �����߳���,Ϊ0ʱĬ��ϵͳ����
        WorkQueue(size_t workers = 0)
            : m_io_service(new boost::asio::io_service)
            , m_work(new boost::asio::io_service::work(*m_io_service))
        {
            if(workers == 0)
                workers = boost::thread::hardware_concurrency();

            for (size_t i = 0; i < workers; ++i) {
                threads_.create_thread(
                    boost::bind(&boost::asio::io_service::run, m_io_service));
            }
        }

        virtual ~WorkQueue()
        {
            m_io_service->stop();
            m_io_service.reset();
            m_work.reset();
        }

        template <typename TTask>
        void add_task(TTask& task)
        {
            m_io_service->post(task);
        }

        template <typename TTask>
        void add_task(TTask&& task)
        {
            m_io_service->post(std::move(task));
        }

    private:
        boost::thread_group threads_;

        typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
        typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

        io_service_ptr  m_io_service;
        work_ptr        m_work;
    };
}
