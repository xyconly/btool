/*************************************************
File name:  atomic_switch.hpp
Author:     AChar
Version:
Date:
Description:    提供判断启动或停止的原子操作基类
   提供不同的两个判断依据,启动和终止,以应对可能出现启动/终止中间态的问题;
   例如:只希望启动一次,但可能存在启动延时的情况;此时通过判断m_init的情况即可避免重复进入;
               AtomicSwitch judge;
               if (!judge.init())
                   return;
               // todo...
               if (!judge.start())
                   return;
   例如:只希望停止一次,但可能存在终止延时的情况;此时优先将m_bstart置位,其次待释放结束后修改m_init标识防止重复进入;
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
    // 提供判断启动或停止的原子操作
    class AtomicSwitch
    {
    public:
        AtomicSwitch() : m_binit(false), m_bstart(false){}
        virtual ~AtomicSwitch() {}

    public:
        // 修改初始化标识
        // 若m_binit为已初始化则直接返回false, 否则修改m_binit为启动状态,并返回true
        bool init() {
            bool target(false);
            return m_binit.compare_exchange_strong(target, true);
        }

        // 修改启动标识
        // 若m_bstart已为启动状态则直接返回false, 否则修改m_bstart为启动状态,并返回true
        bool start() {
            if (!m_binit.load())
                return false;

            bool target(false);
            return m_bstart.compare_exchange_strong(target, true);
        }

        // 修改终止标识
        // 若m_bstart已为终止状态则直接返回false, 否则修改m_bstart为终止状态,并返回true
        bool stop() {
            bool target(true);
            return m_bstart.compare_exchange_strong(target, false);
        }

        // 复位标识
        void reset() {
            m_binit.store(false);
            m_bstart.store(false);
        }

        // 强制修改启动标识
        void store_start_flag(bool target) {
            m_bstart.store(target);
        }

        // 获取初始化标识
        bool load_init_flag() const {
            return m_binit.load();
        }

        // 获取启动标识
        bool load_start_flag() const {
            return m_bstart.load();
        }

        // 是否已启动判断
        bool has_started() const {
            return m_bstart.load() && m_binit.load();
        }

        // 是否已终止判断
        bool has_stoped() const {
            return !m_binit.load() || !m_bstart.load();
        }

    private:
        // 是否已初始化标识符
        std::atomic<bool>           m_binit;
        // 是否已启动标识符
        std::atomic<bool>           m_bstart;
    };
}