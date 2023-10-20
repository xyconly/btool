/*************************************************
File name:  mmap_file.hpp
Author:     AChar
Version:
Date:
Purpose: �ṩmmap�ļ�ӳ���д
Note:    �ļ�дʱ, �޷�ȷ�����ݰ�ȫд��
*************************************************/
#pragma once
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <tuple>

namespace BTool {
    class MMapFile {
    public:
        // �������
        enum class error_t : int {
            ok = 0,
            unknow = -1,
            open_fail = -2,
            close_fail = -3,
            mmap_fail = -5,
            munmap_fail = -6,
            ftruncate_fail = -7,
        };

        // ������Ϣ
        struct error {
            error() : code_(error_t::unknow) {}
            error(error_t code) : code_(code) {}
            error(error_t code, const std::string& msg) : code_(code), msg_(msg) {}
            error(error_t code, std::string&& msg) : code_(code), msg_(std::move(msg)) {}
            inline operator bool() const { return code_ == error_t::ok; }
            inline const std::string& str() const { return msg_; }
            inline error_t code() const { return code_; }
            inline const char* c_str() const { return msg_.c_str(); }

        private:
            error_t code_ = error_t::unknow;
            std::string msg_;
        };

        static error create_error(error_t code) {
            char err_msg[256] = {0};
            auto ret = strerror_r(errno, err_msg, sizeof(err_msg));
            if (ret != nullptr) {
                return error(code, err_msg);
            }
            // return error(code, strerror(errno));
            return error(code, "unknow");
        }

    public:
        // ��˳��һ�ζ�ȡ, �´ζ�ȡ���ͷ���һ�εĶ�ȡmmap�ļ�, ���̰߳�ȫ
        // ע�����coreʱ�޷���֤���ݰ�ȫд��
        class Reader {
        public:
            explicit Reader(bool can_change = false) : m_can_change(can_change), m_item_size(1) {}
            explicit Reader(int item_size, bool can_change = false) : m_can_change(can_change), m_item_size(item_size) {}
            explicit Reader(size_t item_size, bool can_change = false) : m_can_change(can_change), m_item_size(item_size) {}
            ~Reader() { close(); }

            error open(const char* file, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // ���ļ�
                struct stat sb;
                if (!m_can_change)
                    m_fd = ::open(file, O_RDONLY);
                else
                    m_fd = ::open(file, O_RDWR);
                if (m_fd == -1 || fstat(m_fd, &sb) == -1) {
                    return create_error(error_t::open_fail);
                }

                // ��ȡ�ļ��ܴ�С
                m_file_size = sb.st_size;
                // ���С����
                m_chunk_size = chunk_size * m_item_size;
                if (m_chunk_size > (size_t)sb.st_size) {
                    m_chunk_size = sb.st_size;
                }
                // ���������
                if (m_chunk_size > 0) {
                    // ��ȡ��
                    m_num_chunks = (sb.st_size + m_chunk_size - 1) / m_chunk_size;
                }
                return error(error_t::ok);
            }

            error open(const char* file, size_t chunk_size) {
                return open(file, m_item_size, chunk_size);
            }

            inline long long get_obj_num() const {
                return m_file_size / m_item_size;
            }

            inline size_t get_obj_size() const {
                return m_item_size;
            }

            template<typename Type>
            std::tuple<error, Type*, size_t/*count*/> read() {
                auto [ok, data] = read();
                return std::forward_as_tuple(ok, (Type*)data.data(), data.length() / sizeof(Type));
            }

            std::tuple<error, std::string_view> read() {
                // ����ϴζ�ȡ
                error_t err_code = clean_mmap();
                if (err_code != error_t::ok) {
                    return std::forward_as_tuple(create_error(err_code), std::string_view{});
                }
                if ((size_t)m_cur_offset + m_cur_read_length >= (size_t)m_file_size) {
                    return std::forward_as_tuple(error_t::ok, std::string_view{});
                }

                calc_current();

                if (!m_can_change)
                    m_cur_read_addr = mmap(NULL, m_cur_read_length + m_cur_offset - m_pa_offset, PROT_READ, MAP_SHARED, m_fd, m_pa_offset);
                else
                    m_cur_read_addr = mmap(NULL, m_cur_read_length + m_cur_offset - m_pa_offset, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, m_pa_offset);

                if (m_cur_read_addr == MAP_FAILED) {
                    return std::forward_as_tuple(create_error(error_t::mmap_fail), std::string_view{});
                }
                m_cur_chunk_index++;
                return std::forward_as_tuple(error_t::ok, std::string_view((char*)m_cur_read_addr + m_cur_offset - m_pa_offset, m_cur_read_length));
            }

            error close() {
                clean_mmap();

                if (m_fd != -1 && ::close(m_fd) == -1) {
                    return create_error(error_t::close_fail);
                }

                m_fd = -1;
                return error(error_t::ok);
            }

        private:
            error_t clean_mmap() {
                error_t err_code = error_t::ok;
                if (m_cur_read_addr != MAP_FAILED) {
                    if (munmap(m_cur_read_addr, m_cur_read_length + m_cur_offset - m_pa_offset) == -1) {
                        err_code = error_t::munmap_fail;
                    }
                    m_cur_read_addr = MAP_FAILED;
                }
                return err_code;
            }

            void calc_current() {
                m_cur_offset = m_cur_chunk_index * m_chunk_size;
                // ��ȡ��һ�߽�
                m_pa_offset = m_cur_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
                // ���һ����Ҫ����
                m_cur_read_length = (m_cur_chunk_index < m_num_chunks - 1) ? m_chunk_size : m_file_size - m_cur_chunk_index * m_chunk_size;
            }

        private:
            // �Ƿ�ɸı��ļ�����
            bool        m_can_change;
            // �����С
            int         m_item_size;
            // �ļ����
            int         m_fd = -1;
            // �ļ��ܴ�С
            off_t       m_file_size = 0;
            // ��ǰ��ȡƯ��λ��
            off_t       m_cur_offset = 0;
            // ����λ��
            off_t       m_pa_offset = 0;
            // ������ȡ�Ŀ��С
            size_t      m_chunk_size = 0;
            // ������ȡ�Ŀ�����
            size_t      m_num_chunks = 0;
            // ��ǰ������ȡ�Ŀ��±�
            size_t      m_cur_chunk_index = 0;
            // ��ǰ��ȡ���ڴ��ַ
            void*       m_cur_read_addr = MAP_FAILED;
            // ��ǰ��ȡ���ڴ泤��
            size_t      m_cur_read_length = 0;
        };

        // �Ӻ���ǰ��ȡ
        class ReverseReader {
        public:
            ReverseReader() : m_item_size(1) {}
            ReverseReader(int item_size) : m_item_size(item_size) {}
            ~ReverseReader() { close(); }

            error open(const char* file, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // ���ļ�
                struct stat sb;
                m_fd = ::open(file, O_RDONLY);
                if (m_fd == -1 || fstat(m_fd, &sb) == -1) {
                    return create_error(error_t::open_fail);
                }

                // ��ȡ�ļ��ܴ�С
                m_file_size = sb.st_size;
                // ���С����
                m_chunk_size = chunk_size * m_item_size;
                if (m_chunk_size > (size_t)sb.st_size) {
                    m_chunk_size = sb.st_size;
                }
                // ���������
                if (m_chunk_size > 0) {
                    // ��ȡ��
                    m_num_chunks = (sb.st_size + m_chunk_size - 1) / m_chunk_size;
                }
                m_cur_chunk_index = m_num_chunks;
                return error(error_t::ok);
            }

            error open(const char* file, size_t chunk_size) {
                return open(file, m_item_size, chunk_size);
            }

            inline long long get_obj_num() const {
                return m_file_size / m_item_size;
            }

            inline size_t get_obj_size() const {
                return m_item_size;
            }
            
            template<typename Type>
            std::tuple<error, const Type*, size_t/*count*/> read() {
                auto [ok, data] = read();
                return std::forward_as_tuple(ok, (const Type*)data.data(), data.length() / sizeof(Type));
            }

            std::tuple<error, const std::string_view> read() {
                // ����ϴζ�ȡ
                error_t err_code = clean_mmap();
                if (err_code != error_t::ok) {
                    return std::forward_as_tuple(create_error(err_code), std::string_view{});
                }
                if (m_cur_chunk_index == 0) {
                    return std::forward_as_tuple(error_t::ok, std::string_view{});
                }

                calc_current();

                m_cur_read_addr = mmap(NULL, m_cur_read_length + m_cur_offset - m_pa_offset, PROT_READ, MAP_SHARED, m_fd, m_pa_offset);
                if (m_cur_read_addr == MAP_FAILED) {
                    return std::forward_as_tuple(create_error(error_t::mmap_fail), std::string_view{});
                }
                return std::forward_as_tuple(error_t::ok, std::string_view((char*)m_cur_read_addr + m_cur_offset - m_pa_offset, m_cur_read_length));
            }
            
        private:
            error_t clean_mmap() {
                error_t err_code = error_t::ok;
                if (m_cur_read_addr != MAP_FAILED) {
                    if (munmap(m_cur_read_addr, m_cur_read_length + m_cur_offset - m_pa_offset) == -1) {
                        err_code = error_t::munmap_fail;
                    }
                    m_cur_read_addr = MAP_FAILED;
                }
                return err_code;
            }

            void calc_current() {
                --m_cur_chunk_index;
                
                m_cur_offset = m_cur_chunk_index * m_chunk_size;
                // ��ȡ��һ�߽�
                m_pa_offset = m_cur_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
                // ���һ����Ҫ����
                m_cur_read_length = (m_cur_chunk_index < m_num_chunks - 1) ? m_chunk_size : m_file_size - m_cur_chunk_index * m_chunk_size;
            }

            error close() {
                clean_mmap();

                if (m_fd != -1 && ::close(m_fd) == -1) {
                    return create_error(error_t::close_fail);
                }

                m_fd = -1;
                return error(error_t::ok);
            }
        private:
            // �����С
            int         m_item_size;
            // �ļ����
            int         m_fd = -1;
            // �ļ��ܴ�С
            off_t       m_file_size = 0;
            // ��ǰ��ȡƯ��λ��
            off_t       m_cur_offset = 0;
            // ����λ��
            off_t       m_pa_offset = 0;
            // ������ȡ�Ŀ��С
            size_t      m_chunk_size = 0;
            // ������ȡ�Ŀ�����
            size_t      m_num_chunks = 0;
            // ��ǰ������ȡ�Ŀ��±�
            size_t      m_cur_chunk_index = 0;
            // ��ǰ��ȡ���ڴ��ַ
            void*       m_cur_read_addr = MAP_FAILED;
            // ��ǰ��ȡ���ڴ泤��
            size_t      m_cur_read_length = 0;
        };

        // ׷��mmap
        class Writer {
        public:
            Writer() = default;
            ~Writer() { close(); }

            error open(const char* file, bool is_append = true) {
                m_fd = ::open(file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                if (is_append) {
                    m_cur_offset = ::lseek(m_fd, 0, SEEK_END);
                    if (m_cur_offset == -1) {
                        return create_error(error_t::open_fail);
                    }
                }
                return error(error_t::ok);
            }

            void reset_offset(off_t off) { 
                m_cur_offset = off;
            }

            error write(std::string_view data) {
                if (data.empty()) {
                    return error(error_t::ok);
                }
                off_t new_file_size = m_cur_offset + data.length();
                // �ļ���С�������ֵ, ��Ҫ��չ�ļ���С
                if (ftruncate(m_fd, new_file_size) == -1) {
                    return create_error(error_t::ftruncate_fail);
                }
                // �����ļ�ӳ�䲢׷��д
                off_t pa_offset = m_cur_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
                void* map = mmap(NULL, new_file_size - pa_offset, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, pa_offset);
                if (map == MAP_FAILED) {
                    return create_error(error_t::mmap_fail);
                }
                memcpy((char*)map, data.data(), data.length());
                m_cur_offset += data.length();

                // ȡ���ļ�ӳ�䲢�ر��ļ�������
                if (munmap(map, new_file_size - pa_offset) == -1) {
                    return create_error(error_t::munmap_fail);
                }
                return error(error_t::ok);
            }

            void close() {
                ::close(m_fd);
            }

        private:
            // �ļ����
            int     m_fd = -1;
            // ��ǰ��ȡƯ��λ��
            off_t   m_cur_offset = 0;
        };
    };
}