/*************************************************
File name:  atomic_switch.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ�ж�������ֹͣ��ԭ�Ӳ�������
   �ṩ��ͬ�������ж�����,��������ֹ,��Ӧ�Կ��ܳ�������/��ֹ�м�̬������;
   ����:ֻϣ������һ��,�����ܴ���������ʱ�����;��ʱͨ���ж�m_bstart��������ɱ����ظ�����;
               AtomicSwitch judge;
               if (!judge.start())
                   return;
   ����:ֻϣ��ֹͣһ��,�����ܴ�����ֹ��ʱ�����;��ʱ���Ƚ�m_bstop��λ,��δ��ͷŽ������޸�m_bstart��ʶ��ֹ�ظ�����;
               AtomicSwitch judge;
               if (!judge.stop())
                   return;
               // todo...
               judge.store_start_flag(false);
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
        AtomicSwitch() : m_bstart(false), m_bstop(true){}
        virtual ~AtomicSwitch() { stop(); }

    public:
        // �޸�������ʶ
        // ��m_bstart��Ϊ����״̬��ֱ�ӷ���false, �����޸�m_bstartΪ����״̬,������true
        // change_stop_flag: �Ƿ�ͬʱ�޸���ֹ״̬Ϊfalse
        bool start(bool change_stop_flag = true) {
            bool target(false);
            if (!m_bstart.compare_exchange_strong(target, true))
                return false;

            if(change_stop_flag)
                m_bstop.store(false);
            return true;
        }

        // �޸���ֹ��ʶ
        // ��m_bstop��Ϊ��ֹ״̬��ֱ�ӷ���false, �����޸�m_bstopΪ��ֹ״̬,������true
        // change_start_flag: �Ƿ�ͬʱ�޸�����״̬Ϊfalse
        bool stop(bool change_start_flag = false) {
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true))
                return false;

            if(change_start_flag)
                m_bstart.store(false);
            return true;
        }

        // ǿ���޸�������ʶ
        void store_start_flag(bool target) {
            m_bstart.store(target);
        }

        // ǿ���޸���ֹ��ʶ
        void store_stop_flag(bool target) {
            m_bstop.store(target);
        }

        // ��ȡ������ʶ
        bool load_start_flag() const {
            return m_bstart.load();
        }

        // ��ȡ��ֹ��ʶ
        bool load_stop_flag() const {
            return m_bstop.load();
        }

        // �Ƿ��������ж�
        bool has_started() const {
            if (m_bstop.load() || !m_bstart.load())
                return false;

            return true;
        }

        // �Ƿ�����ֹ�ж�
        bool has_stoped() const {
            return m_bstop.load();
        }

    private:
        // �Ƿ���������ʶ��
        std::atomic<bool>           m_bstart;
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;
    };
}