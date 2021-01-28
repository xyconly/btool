/*************************************************
File name:  unique_id.hpp
Author:     AChar
Version:
Date:
Purpose: 对各类唯一ID生成
Note:   SnowflakeID源自:https://www.jianshu.com/p/c961dac7bfb4
*************************************************/

#pragma once

#include <string>
#include <chrono>
#include <boost/noncopyable.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "utility/instance.hpp"

namespace BTool
{
    // UUID
    class UniqueID
    {
    public:
        // 生成UUID
        static std::string GetUUID() {
            return boost::uuids::to_string(boost::uuids::random_generator()());
        }
    };

    namespace BTool {

        /**
         * @brief 分布式id生成类
         * https://segmentfault.com/a/1190000011282426
         * https://github.com/twitter/snowflake/blob/snowflake-2010/src/main/scala/com/twitter/service/snowflake/IdWorker.scala
         *
         * 64bit id: 0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000  0000
         *           ||                                                           ||            |  |              |
         *           ||-------------------------41位时间戳------------------------||--10位机器--|  |--12位序列号--|
         *           |
         *         不用
         * SnowFlake的优点: 整体上按照时间自增排序, 并且整个分布式系统内不会产生ID碰撞(由配置的机器ID和序列号作区分), 并且效率较高, 经测试, SnowFlake每秒能够产生26万ID左右.
         */
        class SnowFlakeID
            : public instance_base<SnowFlakeID>
            , private boost::noncopyable
        {
        public:
            enum {
                INVALID_ID = 0,
            };

            SnowFlakeID() : m_worker_id(0), m_sequence(0), m_last_timestamp(0) { }

            // worker_id: 机器ID (0-1023)
            void setWorkerId(uint32_t worker_id) {
                m_worker_id = worker_id;
            }

            // 获取一个新的id
            uint64_t get_id() {
                uint64_t timestamp = timeGen();

                std::unique_lock<std::mutex> lock(m_mtx);

                // 如果当前时间小于上一次ID生成的时间戳,说明系统时钟回退过
                // 此时应返回无效ID
                if (timestamp < m_last_timestamp) {
                    return INVALID_ID;
                }

                if (m_last_timestamp == timestamp) {
                    // 如果是同一时间生成的，则进行毫秒内序列
                    m_sequence = (m_sequence + 1) & m_sequence_mask;
                    if (0 == m_sequence) {
                        // 毫秒内序列溢出, 阻塞到下一个毫秒,获得新的时间戳
                        timestamp = tilNextMillis(m_last_timestamp);
                    }
                }
                else {
                    m_sequence = 0;
                }

                m_last_timestamp = timestamp;

                // 移位并通过或运算拼到一起组成64位的ID
                return ((timestamp - m_twepoch) << m_timestamp_shift)
                    | (m_worker_id << m_worker_id_shift)
                    | m_sequence;
            }

            /**
             * 返回以毫秒为单位的当前时间
             *
             * @return 当前时间(毫秒)
             */
            uint64_t timeGen() const {
                auto t = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now());
                return t.time_since_epoch().count();
            }

            /**
             * 阻塞到下一个毫秒，直到获得新的时间戳
             *
             * @param m_last_timestamp 上次生成ID的时间截
             * @return 当前时间戳
             */
            uint64_t tilNextMillis(uint64_t m_last_timestamp) const {
                uint64_t timestamp = timeGen();
                while (timestamp <= m_last_timestamp) {
                    timestamp = timeGen();
                }
                return timestamp;
            }

        private:
            std::mutex      m_mtx;
            // 开始时间截 (2021-01-01 00:00:00.000)
            const uint64_t  m_twepoch = 1609430400000;
            // 机器ID所占的位数
            const uint8_t   m_worker_id_bits = 10;
            // 序列号所占的位数
            const uint8_t   m_sequence_bits = 12;
            // 机器ID向左移12位
            const uint8_t   m_worker_id_shift = m_sequence_bits;
            // 时间截向左移22位
            const uint8_t   m_timestamp_shift = m_worker_id_bits + m_sequence_bits;
            // 最大支持机器个数
            const uint8_t   m_max_work_count = -1 ^ (-1 << m_worker_id_bits);
            // 机器ID
            uint32_t        m_worker_id = 0;
            // 每 4096 台机器rolls over一次(具有保护, 以避免在同一 ms 中rollover）
            uint32_t        m_sequence = 0;
            // 生成序列的掩码，这里为4095
            const uint32_t  m_sequence_mask = -1 ^ (-1 << m_sequence_bits);
            // 上次生成ID的时间截
            uint32_t        m_last_timestamp = 0;
        };

    }

}
