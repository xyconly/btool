/*************************************************
File name:  io_context_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩio_context�Ķ����,�ṩ�첽start��ͬ��stop�ӿ�
*************************************************/
#pragma once

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <boost/functional/factory.hpp>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>
#include <atomic>
#include <vector>
#include "comm_function_os.hpp"

namespace BTool
{
    // io_context�Ķ����
    class AsioContextPool : private boost::noncopyable
    {
    public:
        typedef boost::asio::io_context         ioc_type;
        typedef boost::asio::io_context::work   work_type;
        typedef std::shared_ptr<ioc_type>       ioc_ptr_type;
        typedef std::shared_ptr<work_type>      work_ptr_type;

        // io_context�Ķ����
        // pool_size ȱʡʱ,����������ǻ�����cpu��
        AsioContextPool(int pool_size = 0, bool auto_start = true, const std::vector<int>& bind_cores = {})
            : m_pool_size(pool_size)
            , m_next_ioc_index(0)
            , m_bstart(false)
            , m_bind_cores(bind_cores)
        {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            if (auto_start)
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

        void start(int pool_size, const std::vector<int>& bind_cores) {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // ���������˳�
                return;

            if (pool_size <= 0)
                pool_size = boost::thread::hardware_concurrency();

            assert (pool_size <= (int)bind_cores.size());

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
        }

        bool restart(const std::vector<int>& bind_cores, int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
            return true;
        }

        bool restart(int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)m_bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            
            init(m_pool_size);
            return true;
        }

        // ����ʽ��������,ʹ��join_all�ȴ�
        void run() {
            start();
            m_threads.join_all();
        }

        // ֹͣ����,���ܲ�������
        void stop() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // ����ֹ���˳�
                return;

            reset_sync();
        }

        // ����
        void reset() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // δ�������˳�
                return;

            reset_sync();
        }

        // ѭ����ȡio_context
        ioc_type& get_io_context() {
            auto& result = *m_io_contexts[m_next_ioc_index];
            if(++m_next_ioc_index == m_pool_size) 
                m_next_ioc_index = 0;
            return result;
        }

    private:
        void run_io_context(ioc_type& ioc, int core_id) {
            CommonOS::BindCore(core_id);
            ioc.run();
        }

        void init(int pool_size) {
            reset_sync();

            for (int i = 0; i < pool_size; i++) {
                auto new_ioc = std::make_shared<ioc_type>();
                m_io_contexts.emplace_back(new_ioc);
                m_io_works.emplace_back(std::make_shared<work_type>(*new_ioc));
                
                if ((int)m_bind_cores.size() > i) {
                    m_threads.create_thread(boost::bind(&AsioContextPool::run_io_context, this, boost::ref(*new_ioc), m_bind_cores[i]));
                }
                else {
                    m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*new_ioc)));
                }
            }
        }

        void reset_sync() {
            for (auto& ioc_ptr : m_io_contexts)
                ioc_ptr->stop();
			
            m_threads.join_all();
            m_io_works.clear();
            m_io_contexts.clear();
            m_next_ioc_index = 0;
        }

    private:
        // �̳߳ظ���
        int                         m_pool_size;
        // ��һio_context���±�
        int                         m_next_ioc_index;
        // io_context�Ķ����
        std::vector<ioc_ptr_type>   m_io_contexts;
        // io_context�����κ����������±���˳�,Ϊȷ���������²����˳�,����workȷ��������
        std::vector<work_ptr_type>  m_io_works;
        // �̳߳�
        boost::thread_group         m_threads;
        // �Ƿ��ѿ���
        std::atomic<bool>           m_bstart;
        // ���
        std::vector<int>            m_bind_cores;
    };


    // ��һio_context���̳߳�
	// ������������ʱ, �ȶ��ioc���ܸ���, �ʺ��������
    class AsioSingleContextPool : private boost::noncopyable
    {
    public:
        typedef boost::asio::io_context         ioc_type;
        typedef boost::asio::io_context::work   work_type;

        // io_context�Ķ����
        // pool_size ȱʡʱ,����������ǻ�����cpu��
        AsioSingleContextPool(int pool_size = 0, bool auto_start = true, const std::vector<int>& bind_cores = {})
            : m_pool_size(pool_size)
            , m_io_context(nullptr)
            , m_io_work(nullptr)
            , m_bstart(false)
            , m_bind_cores(bind_cores)
        {
            if (m_pool_size <= 0)
                m_pool_size = boost::thread::hardware_concurrency();

            if (auto_start)
                start();
        }

        ~AsioSingleContextPool() {
            stop();
        }

        // ������ʽ��������
        void start() {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // ���������˳�
                return;

            init(m_pool_size);
        }

        void start(int pool_size, const std::vector<int>& bind_cores) {
            bool expected = false;
            if (!m_bstart.compare_exchange_strong(expected, true))  // ���������˳�
                return;

            if (pool_size <= 0)
                pool_size = boost::thread::hardware_concurrency();

            assert (pool_size <= (int)bind_cores.size());

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
        }

        bool restart(const std::vector<int>& bind_cores, int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;
            m_bind_cores = bind_cores;

            init(m_pool_size);
            return true;
        }

        bool restart(int pool_size = 0) {
            m_bstart.exchange(true);
            if (pool_size == 0)
                pool_size = boost::thread::hardware_concurrency();

            if (pool_size > (int)m_bind_cores.size()) {
                return false;
            }

            m_pool_size = pool_size;

            init(m_pool_size);
            return true;
        }

        // ����ʽ��������,ʹ��join_all�ȴ�
        void run() {
            start();
            m_threads.join_all();
        }

        // ֹͣ����,���ܲ�������
        void stop() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // ����ֹ���˳�
                return;

            if (m_io_context) {
                m_io_context->stop();
            }
            m_threads.join_all();
        }

        // ����
        void reset() {
            bool expected = true;
            if (!m_bstart.compare_exchange_strong(expected, false))  // δ�������˳�
                return;

            reset_sync();
        }

        // ѭ����ȡio_context
        ioc_type& get_io_context() {
            return *m_io_context;
        }

    private:
        void run_io_context(ioc_type& ioc, int core_id) {
            CommonOS::BindCore(core_id);
            ioc.run();
        }

        void init(int pool_size) {
            reset_sync();

            m_io_context = new ioc_type(pool_size);
            m_io_work = new work_type(*m_io_context);
            for (int i = 0; i < pool_size; i++) {
                if ((int)m_bind_cores.size() > i) {
                    m_threads.create_thread(boost::bind(&AsioSingleContextPool::run_io_context, this, boost::ref(*m_io_context), m_bind_cores[i]));
                }
                else {
                    m_threads.create_thread(boost::bind(&ioc_type::run, boost::ref(*m_io_context)));
                }
            }
        }

        void reset_sync() {
            if (m_io_context) {
                m_io_context->stop();
            }
            m_threads.join_all();

            if (m_io_context) {
                delete m_io_work;
                m_io_work = nullptr;

                delete m_io_context;
                m_io_context = nullptr;
            }
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
        // ���
        std::vector<int>        m_bind_cores;
    };
}