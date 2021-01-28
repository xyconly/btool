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
                   return false;
               // 其他初始化操作
               if (!judge.start())
                   return false;
   例如:只希望停止一次,但可能存在终止延时的情况;此时优先将m_bstop置位,其次待释放结束后修改m_init标识防止重复进入;
               AtomicSwitch judge;
               if (!judge.stop())
                   return false;
               // 其他释放操作, 下次启动必须调用init
               judge.reset();
               return true;
*************************************************/

#pragma once

#include <atomic>

namespace BTool
{
    // 提供判断启动或停止的原子操作
    class AtomicSwitch
    {
    public:
        AtomicSwitch() : m_binit(false), m_bstart(false), m_bstop(false) {}
        virtual ~AtomicSwitch() {}

    public:
        // 修改初始化标识
        // 尝试原子修改m_binit为已初始化,若已初始化则返回false
        bool init() {
            bool target(false);
            return m_binit.compare_exchange_strong(target, true);
        }

        // 修改启动标识
        // 若m_bstop已为终止状态中, 或者m_binit为未初始化状态中, 则直接返回false
        // 否则尝试原子修改m_bstart为启动状态
        bool start() {
            if (m_bstop.load() || !m_binit.load())
                return false;

            bool target(false);
            return m_bstart.compare_exchange_strong(target, true);
        }

        // 修改终止标识
        // 若m_bstart为未启动状态, 或者m_binit为未初始化状态中, 则直接返回false
        // 否则尝试原子修改m_bstop为启动状态
        bool stop() {
            if (!m_binit.load() || !m_bstart.load())
                return false;

            bool target(false);
            return m_bstop.compare_exchange_strong(target, true);
        }

        // 复位标识
        void reset() {
            m_bstart.store(false);
            m_bstop.store(false);
            m_binit.store(false);
        }

        // 是否已初始化判断
        bool has_init() const {
            return m_binit.load();
        }

        // 是否已启动判断
        bool has_started() const {
            return !m_bstop.load() && m_bstart.load() && has_init();
        }

        // 是否已终止判断
        bool has_stoped() const {
            return !has_init() || !m_bstart.load() || m_bstop.load();
        }

    private:
        // 是否已初始化标识符
        std::atomic<bool>           m_binit;
        // 是否已启动标识符
        std::atomic<bool>           m_bstart;
        // 是否已进入终止状态标识符
        std::atomic<bool>           m_bstop;
    };
}