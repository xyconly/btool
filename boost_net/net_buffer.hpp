#pragma once

#include <queue>
#include <boost/asio.hpp>
#include "memory_stream.hpp"

namespace BTool
{
    namespace BoostNet
    {
        // ���ջ���������
        // ��Ϊ����������з���, ʹ��data��ȡ��������, ��������ɺ����consume�Ƴ��Ѿ����͵�����
        // ��Ϊ����������н���, ʹ��prepare��ȡ�������, ����ȡ��ɺ����commit
        // ��������Զ����ݹ���
        class ReadBuffer
        {
        public:
            typedef size_t size_type;
            typedef boost::asio::streambuf streambuf_type;
            typedef streambuf_type::const_buffers_type const_buffers_type;
            typedef streambuf_type::mutable_buffers_type mutable_buffers_type;

        public:
            //
            // ������
            //
            // ��ȡ������еĻ������б������������Ĵ�С
            mutable_buffers_type prepare(size_type output_size) {
                return m_buf.prepare(output_size);
            }

            // ���ַ�����������ƶ�����������
            void commit(size_type n) {
                m_buf.commit(n);
            }

            streambuf_type& streambuf() {
                return m_buf;
            }
            const streambuf_type& streambuf() const {
                return m_buf;
            }

            // ��ȡ�������еĻ����ֽ���
            size_type size() const {
                return m_buf.size();
            }

            // ����������еĻ���
            const char* peek() const {
#if BOOST_VERSION >= 108000
                return static_cast<const char*>(m_buf.data().data());
#else
                return boost::asio::buffer_cast<const char*> (m_buf.data());
#endif
            }

            // ��ȡ�������еĻ����б�
            const_buffers_type data() const {
                return m_buf.data();
            }

            // �������������Ƴ��ַ�
            void consume(size_type n) {
                m_buf.consume(n);
            }

            // ������д������������,ע��ò��������޸�data���ݵ�ַ
            bool append(const void * data, size_type len) {
                std::streamsize count = boost::numeric_cast<std::streamsize>(len);
                return count == m_buf.sputn(static_cast<const char*>(data), count);
            }

        private:
            streambuf_type m_buf;
        };

        // ���ͻ���
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
            // ��ȡ�����ݴ�С
            size_t size() const {
                return m_all_len;
            }

            // д����
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

            // �ڵ�ǰ��Ϣβ׷��
            // max_package_size:������,0��ʾ������
            bool append_tail(const char* const msg, size_t len, size_t max_package_size = 0) {
                if (m_send_items.empty())
                    return append(msg, len);

                auto& item = m_send_items.back();
                if(max_package_size != 0 && item->get_length() + len >= max_package_size)
                    return append(msg, len);

                item->append(msg, len);
                return true;
            }

            // ��ȡ����
            const WriteMemoryStreamPtr& front() const {
                return m_send_items.front();
            }

            // ��ȡ���Ƴ�����
            WriteMemoryStreamPtr pop_front() {
                auto msg = std::move(m_send_items.front());
                m_send_items.pop_front();
                m_all_len -= msg->size();
                return msg;
            }

            // �������
            void clear() {
                while (!m_send_items.empty()) {
                    m_free_pool.push(std::move(m_send_items.front()));
                    m_send_items.pop_front();
                }
                m_all_len = 0;
            }

            // �Ƿ�Ϊ��
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
            // ��δʹ�õĿ��ж���
            std::queue<WriteMemoryStreamPtr>    m_free_pool;
            // �ȴ�ʹ�õķ��Ͷ���
            std::deque<WriteMemoryStreamPtr>    m_send_items;
            // ��ǰ�ܵȴ��������ݳ���
            size_t                              m_all_len;
        };
    }
}