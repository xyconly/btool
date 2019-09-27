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
                   return;
               // todo...
               if (!judge.start())
                   return;
   ����:ֻϣ��ֹͣһ��,�����ܴ�����ֹ��ʱ�����;��ʱ���Ƚ�m_bstart��λ,��δ��ͷŽ������޸�m_init��ʶ��ֹ�ظ�����;
               AtomicSwitch judge;
               if (!judge.stop())
                   return;
               // todo...
               judge.reset()
               return;
*************************************************/

#pragma once

#include <atomic>

namespace BTool
{
    // �ṩ�ж�������ֹͣ��ԭ�Ӳ���
    class AtomicSwitch
    {
    public:
        AtomicSwitch() : m_binit(false), m_bstart(false){}
        virtual ~AtomicSwitch() {}

    public:
        // �޸ĳ�ʼ����ʶ
        // ��m_binitΪ�ѳ�ʼ����ֱ�ӷ���false, �����޸�m_binitΪ����״̬,������true
        bool init() {
            bool target(false);
            return m_binit.compare_exchange_strong(target, true);
        }

        // �޸�������ʶ
        // ��m_bstart��Ϊ����״̬��ֱ�ӷ���false, �����޸�m_bstartΪ����״̬,������true
        bool start() {
            if (!m_binit.load())
                return false;

            bool target(false);
            return m_bstart.compare_exchange_strong(target, true);
        }

        // �޸���ֹ��ʶ
        // ��m_bstart��Ϊ��ֹ״̬��ֱ�ӷ���false, �����޸�m_bstartΪ��ֹ״̬,������true
        bool stop() {
            bool target(true);
            return m_bstart.compare_exchange_strong(target, false);
        }

        // ��λ��ʶ
        void reset() {
            m_binit.store(false);
            m_bstart.store(false);
        }

        // ǿ���޸�������ʶ
        void store_start_flag(bool target) {
            m_bstart.store(target);
        }

        // ��ȡ��ʼ����ʶ
        bool load_init_flag() const {
            return m_binit.load();
        }

        // ��ȡ������ʶ
        bool load_start_flag() const {
            return m_bstart.load();
        }

        // �Ƿ��������ж�
        bool has_started() const {
            return m_bstart.load() && m_binit.load();
        }

        // �Ƿ�����ֹ�ж�
        bool has_stoped() const {
            return !m_binit.load() || !m_bstart.load();
        }

    private:
        // �Ƿ��ѳ�ʼ����ʶ��
        std::atomic<bool>           m_binit;
        // �Ƿ���������ʶ��
        std::atomic<bool>           m_bstart;
    };
}