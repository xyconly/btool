/*************************************************
File name:  io_context_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩio_context�Ķ����,�ṩ�첽start��ͬ��stop�ӿ�
*************************************************/
#pragma once

#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread.hpp>
#include <boost/functional/factory.hpp>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>

namespace BTool
{
    // io_context�Ķ����
    class AsioContextPool : private boost::noncopyable
    {
    public:
        typedef boost::asio::io_context         ioc_type;
        typedef boost::asio::io_context::work   work_type;

        // io_context�Ķ����
        // pool_size ȱʡʱ,����������ǻ�����cpu��
        AsioContextPool(int pool_size = 0)
            : m_bstart(false)
            , m_pool_size(pool_size)
            , m_io_context(nullptr)
            , m_io_work(nullptr)
        {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            start();
        }

        ~AsioContextPool() {
            stop();
        }

        // ������ʽ��������
        void start() {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // ���������˳�
                return;

            init(m_pool_size);
        }

        void restart(int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            m_pool_size = pool_size;
            init(m_pool_size);
        }

        // ����ʽ��������,ʹ��join_all�ȴ�
        void run() {
            start();
            m_threads.join_all();
        }

        // ֹͣ����,���ܲ�������
        void stop() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // δ�������˳�
                return;

            stop_sync();
        }

        // ѭ����ȡio_context
        ioc_type& get_io_context() {
            return *m_io_context;
        }

    private:
        void init(int pool_size) {
            stop_sync();

            m_io_context = new ioc_type(pool_size);
            m_io_work = new work_type(*m_io_context);
            for (size_t i = 0; i < pool_size; i++) {
                m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*m_io_context)));
            }
        }

        void stop_sync() {
            if (m_io_context) {
                m_io_context->stop();
                delete m_io_context;
                m_io_context = nullptr;

                delete m_io_work;
                m_io_work = nullptr;
            }
            m_threads.join_all();
        }

    private:
        // �̳߳ظ���
        int                     m_pool_size;
        // io_context�Ķ����
        ioc_type*               m_io_context;
        // io_context�����κ����������±���˳�,Ϊȷ���������²����˳�,����workȷ��������
        work_type*              m_io_work;
        // �̳߳�
        boost::thread_group     m_threads;
        // �Ƿ��ѿ���
        std::atomic<bool>       m_bstart;
    };
}