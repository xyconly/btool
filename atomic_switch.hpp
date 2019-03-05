/*************************************************
File name:  atomic_switch.hpp
Author:     AChar
Version:
Date:
Description:    提供判断启动或停止的原子操作基类
   提供不同的两个判断依据,启动和终止,以应对可能出现启动/终止中间态的问题;
   例如:只希望启动一次,但可能存在启动延时的情况;此时通过判断m_bstart的情况即可避免重复进入;
               AtomicSwitch judge;
               if (!judge.start())
                   return;
   例如:只希望停止一次,但可能存在终止延时的情况;此时优先将m_bstop置位,其次待释放结束后修改m_bstart标识防止重复进入;
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
    // 提供判断启动或停止的原子操作
    class AtomicSwitch
    {
    public:
        AtomicSwitch() : m_bstart(false), m_bstop(true){}
        virtual ~AtomicSwitch() { stop(); }

    public:
        // 修改启动标识
        // 若m_bstart已为启动状态则直接返回false, 否则修改m_bstart为启动状态,并返回true
        // change_stop_flag: 是否同时修改终止状态为false
        bool start(bool change_stop_flag = true) {
            bool target(false);
            if (!m_bstart.compare_exchange_strong(target, true))
                return false;

            if(change_stop_flag)
                m_bstop.store(false);
            return true;
        }

        // 修改终止标识
        // 若m_bstop已为终止状态则直接返回false, 否则修改m_bstop为终止状态,并返回true
        // change_start_flag: 是否同时修改启动状态为false
        bool stop(bool change_start_flag = false) {
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true))
                return false;

            if(change_start_flag)
                m_bstart.store(false);
            return true;
        }

        // 强制修改启动标识
        void store_start_flag(bool target) {
            m_bstart.store(target);
        }

        // 强制修改终止标识
        void store_stop_flag(bool target) {
            m_bstop.store(target);
        }

        // 获取启动标识
        bool load_start_flag() const {
            return m_bstart.load();
        }

        // 获取终止标识
        bool load_stop_flag() const {
            return m_bstop.load();
        }

        // 是否已启动判断
        bool has_started() const {
            if (m_bstop.load() || !m_bstart.load())
                return false;

            return true;
        }

        // 是否已终止判断
        bool has_stoped() const {
            return m_bstop.load();
        }

    private:
        // 是否已启动标识符
        std::atomic<bool>           m_bstart;
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;
    };
}