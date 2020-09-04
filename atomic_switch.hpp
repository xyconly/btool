/*************************************************
File name:  atomic_switch.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ�ж�������ֹͣ��ԭ�Ӳ�������
   �ṩ��ͬ�������ж�����,��������ֹ,��Ӧ�Կ��ܳ�������/��ֹ�м�̬������;
   ����:ֻϣ������һ��,�����ܴ���������ʱ�����;��ʱͨ���ж�m_init��������ɱ����ظ�����;
               AtomicSwitch judge;
               if (!judge.init())
                   return false;
               // ������ʼ������
               if (!judge.start())
                   return false;
   ����:ֻϣ��ֹͣһ��,�����ܴ�����ֹ��ʱ�����;��ʱ���Ƚ�m_bstop��λ,��δ��ͷŽ������޸�m_init��ʶ��ֹ�ظ�����;
               AtomicSwitch judge;
               if (!judge.stop())
                   return false;
               // �����ͷŲ���, �´������������init
               judge.reset();
               return true;
*************************************************/

#pragma once

#include <atomic>

namespace BTool
{
    // �ṩ�ж�������ֹͣ��ԭ�Ӳ���
    class AtomicSwitch
    {
    public:
        AtomicSwitch() : m_binit(false), m_bstart(false), m_bstop(false) {}
        virtual ~AtomicSwitch() {}

    public:
        // �޸ĳ�ʼ����ʶ
        // ����ԭ���޸�m_binitΪ�ѳ�ʼ��,���ѳ�ʼ���򷵻�false
        bool init() {
            bool target(false);
            return m_binit.compare_exchange_strong(target, true);
        }

        // �޸�������ʶ
        // ��m_bstop��Ϊ��ֹ״̬��, ����m_binitΪδ��ʼ��״̬��, ��ֱ�ӷ���false
        // ������ԭ���޸�m_bstartΪ����״̬
        bool start() {
            if (m_bstop.load() || !m_binit.load())
                return false;

            bool target(false);
            return m_bstart.compare_exchange_strong(target, true);
        }

        // �޸���ֹ��ʶ
        // ��m_bstartΪδ����״̬, ����m_binitΪδ��ʼ��״̬��, ��ֱ�ӷ���false
        // ������ԭ���޸�m_bstopΪ����״̬
        bool stop() {
            if (!m_binit.load() || !m_bstart.load())
                return false;

            bool target(false);
            return m_bstop.compare_exchange_strong(target, true);
        }

        // ��λ��ʶ
        void reset() {
            m_bstart.store(false);
            m_bstop.store(false);
            m_binit.store(false);
        }

        // �Ƿ��ѳ�ʼ���ж�
        bool has_init() const {
            return m_binit.load();
        }

        // �Ƿ��������ж�
        bool has_started() const {
            return !m_bstop.load() && m_bstart.load() && has_init();
        }

        // �Ƿ�����ֹ�ж�
        bool has_stoped() const {
            return !has_init() || !m_bstart.load() || m_bstop.load();
        }

    private:
        // �Ƿ��ѳ�ʼ����ʶ��
        std::atomic<bool>           m_binit;
        // �Ƿ���������ʶ��
        std::atomic<bool>           m_bstart;
        // �Ƿ��ѽ�����ֹ״̬��ʶ��
        std::atomic<bool>           m_bstop;
    };
}