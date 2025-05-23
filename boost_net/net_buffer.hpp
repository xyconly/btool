#pragma once

#include <queue>
#include <boost/asio.hpp>
#include "memory_stream.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // 接收缓存数据流
        // 作为数据输入进行发送, 使用data获取输入序列, 当发送完成后调用consume移除已经发送的内容
        // 作为数据输出进行接收, 使用prepare获取输出序列, 当读取完成后调用commit
        // 流本身带自动扩容功能
        class ReadBuffer
        {
        public:
            typedef size_t size_type;
            typedef boost::asio::streambuf streambuf_type;
            typedef streambuf_type::const_buffers_type const_buffers_type;
            typedef streambuf_type::mutable_buffers_type mutable_buffers_type;

        public:
            //
            // 读功能
            //
            // 获取输出序列的缓冲区列表，并给出给定的大小
            mutable_buffers_type prepare(size_type output_size) {
                return m_buf.prepare(output_size);
            }

            // 将字符从输出序列移动到输入序列
            void commit(size_type n) {
                m_buf.commit(n);
            }

            streambuf_type& streambuf() {
                return m_buf;
            }
            const streambuf_type& streambuf() const {
                return m_buf;
            }

            // 获取输入序列的缓存字节数
            size_type size() const {
                return m_buf.size();
            }

            // 检查输入序列的缓存
            const char* peek() const {
#if BOOST_VERSION >= 108000
                return static_cast<const char*>(m_buf.data().data());
#else
                return boost::asio::buffer_cast<const char*> (m_buf.data());
#endif
            }

            // 获取输入序列的缓存列表
            const_buffers_type data() const {
                return m_buf.data();
            }

            // 从输入序列中移除字符
            void consume(size_type n) {
                m_buf.consume(n);
            }

            // 将数据写入输入序列中,注意该操作可能修改data数据地址
            bool append(const void * data, size_type len) {
                std::streamsize count = boost::numeric_cast<std::streamsize>(len);
                return count == m_buf.sputn(static_cast<const char*>(data), count);
            }

        private:
            streambuf_type m_buf;
        };

        // 发送缓存
        template<size_t DEFAULT_SIZE = 1024>
        class WriteBuffer
        {
        public:
            typedef MemoryStream                            WriteMemoryStream;
            typedef std::unique_ptr<MemoryStream>           WriteMemoryStreamPtr;

            WriteBuffer(size_t pool_size = 1000) : m_all_len(0) {
                for(size_t i = 0; i < pool_size; ++i) {
                    m_free_pool.push(std::make_unique<WriteMemoryStream>(DEFAULT_SIZE));
                }
            }
            ~WriteBuffer() {
                destroy();
            }

        public:
            // 获取总数据大小
            size_t size() const {
                return m_all_len;
            }

            // 写功能
            bool append(const char* const msg, size_t len) {
                WriteMemoryStreamPtr memory_stream = acquire(msg, len);
                if (!memory_stream)
                    return false;
                return append(std::move(memory_stream));
            }
            bool append(WriteMemoryStreamPtr&& memory_stream) {
                if (!memory_stream)
                    return false;
                m_all_len += memory_stream->size();
                m_send_items.push_back(std::move(memory_stream));
                return true;
            }

            // 在当前消息尾追加
            // max_package_size:最大包长,0表示无限制
            bool append_tail(const char* const msg, size_t len, size_t max_package_size = 0) {
                if (m_send_items.empty())
                    return append(msg, len);

                auto& item = m_send_items.back();
                if(max_package_size != 0 && item->get_length() + len >= max_package_size)
                    return append(msg, len);

                item->append(msg, len);
                return true;
            }

            // 获取数据
            const WriteMemoryStreamPtr& front() const {
                return m_send_items.front();
            }

            // 获取并移除数据
            WriteMemoryStreamPtr pop_front() {
                auto msg = std::move(m_send_items.front());
                m_send_items.pop_front();
                m_all_len -= msg->size();
                return msg;
            }

            // 清空数据
            void clear() {
                while (!m_send_items.empty()) {
                    m_free_pool.push(std::move(m_send_items.front()));
                    m_send_items.pop_front();
                }
                m_all_len = 0;
            }

            // 是否为空
            bool empty() const {
                return m_send_items.empty();
            }

            void release(WriteMemoryStreamPtr& obj) {
                if (obj) {
                    m_free_pool.push(std::move(obj));
                }
            }

            WriteMemoryStreamPtr acquire(const char* const msg, size_t len) {
                if (!m_free_pool.empty()) {
                    auto obj = std::move(m_free_pool.front());
                    m_free_pool.pop();
                    if (len > 0)
                        obj->load(msg, len);
                    else
                        obj->clear();
                    return obj;
                }
                auto obj = std::make_unique<WriteMemoryStream>();
                obj->load(msg, len, DEFAULT_SIZE);
                return obj;
            }

        private:
            void destroy() {
                std::queue<WriteMemoryStreamPtr> empty;
                m_free_pool.swap(empty);
                m_send_items.clear();
            }

        private:
            // 尚未使用的空闲队列
            std::queue<WriteMemoryStreamPtr>    m_free_pool;
            // 等待使用的发送队列
            std::deque<WriteMemoryStreamPtr>    m_send_items;
            // 当前总等待发送数据长度
            size_t                              m_all_len;
        };
    }
}