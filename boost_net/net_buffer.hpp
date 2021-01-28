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

            // ��ȡ�������еĻ����ֽ���
            size_type size() const {
                return m_buf.size();
            }

            // ����������еĻ���
            const char* peek() const {
                return boost::asio::buffer_cast<const char*> (m_buf.data());
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
        class WriteBuffer
        {
        public:
            typedef MemoryStream                            WriteMemoryStream;
            typedef std::shared_ptr<MemoryStream>           WriteMemoryStreamPtr;

            WriteBuffer() : m_all_len(0) { }
            ~WriteBuffer() {
                clear();
            }

        public:
            // ��ȡ�����ݴ�С
            size_t size() const {
                return m_all_len;
            }

            // д����
            bool append(const char* const msg, size_t len) {
                WriteMemoryStreamPtr memory_stream = std::make_shared<WriteMemoryStream>();
                if (!memory_stream)
                    return false;
                memory_stream->append(msg, len);

                return append(memory_stream);
            }
            bool append(const WriteMemoryStreamPtr& memory_stream) {
                if (!memory_stream)
                    return false;
                m_all_len += memory_stream->size();
                m_send_items.push(memory_stream);
                return true;
            }

            // �ڵ�ǰ��Ϣβ׷��
            // max_package_size:������,0��ʾ������
            bool append_tail(const char* const msg, size_t len, size_t max_package_size = 0) {
                if (m_send_items.empty())
                    return append(msg, len);

                auto item = m_send_items.back();
                if(max_package_size != 0 && item->get_length() + len >= max_package_size)
                    return append(msg, len);

                item->append(msg, len);
                return true;
            }

            // ��ȡ����
            WriteMemoryStreamPtr front() const {
                return m_send_items.front();
            }

            // ��ȡ���Ƴ�����
            WriteMemoryStreamPtr pop_front() {
                auto msg = m_send_items.front();
                m_send_items.pop();
                m_all_len -= msg->size();
                return msg;
            }

            // �������
            void clear() {
                if (!m_send_items.empty())
                    m_send_items = std::queue<WriteMemoryStreamPtr>();
                m_all_len = 0;
            }

            // �Ƿ�Ϊ��
            bool empty() const {
                return m_send_items.empty();
            }

        private:
            std::queue<WriteMemoryStreamPtr>    m_send_items;
            size_t                              m_all_len;
        };
    }
}