/*************************************************
File name:  mmap_file.hpp
Author:     AChar
Version:
Date:
Purpose: 提供mmap文件映射读写
Note:    文件写时, 无法确保数据安全写入
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
        // 错误代码
        enum class error_t : int {
            ok = 0,
            unknow = -1,
            open_fail = -2,
            close_fail = -3,
            mmap_fail = -5,
            munmap_fail = -6,
            ftruncate_fail = -7,
        };

        // 错误信息
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
        // 按顺序一次读取, 下次读取会释放上一次的读取mmap文件, 非线程安全
        // 注意程序core时无法保证数据安全写入
        class Reader {
        public:
            explicit Reader(bool can_change = false) : m_can_change(can_change), m_item_size(1) {}
            explicit Reader(int item_size, bool can_change = false) : m_can_change(can_change), m_item_size(item_size) {}
            explicit Reader(size_t item_size, bool can_change = false) : m_can_change(can_change), m_item_size(item_size) {}
            ~Reader() { close(); }

            error open(const char* file, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // 打开文件
                struct stat sb;
                if (!m_can_change)
                    m_fd = ::open(file, O_RDONLY);
                else
                    m_fd = ::open(file, O_RDWR);
                if (m_fd == -1 || fstat(m_fd, &sb) == -1) {
                    return create_error(error_t::open_fail);
                }

                // 获取文件总大小
                m_file_size = sb.st_size;
                // 块大小对齐
                m_chunk_size = chunk_size * m_item_size;
                if (m_chunk_size > (size_t)sb.st_size) {
                    m_chunk_size = sb.st_size;
                }
                // 计算块总数
                if (m_chunk_size > 0) {
                    // 上取整
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
                // 清空上次读取
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
                // 获取下一边界
                m_pa_offset = m_cur_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
                // 最后一块需要对齐
                m_cur_read_length = (m_cur_chunk_index < m_num_chunks - 1) ? m_chunk_size : m_file_size - m_cur_chunk_index * m_chunk_size;
            }

        private:
            // 是否可改变文件内容
            bool        m_can_change;
            // 子项大小
            int         m_item_size;
            // 文件句柄
            int         m_fd = -1;
            // 文件总大小
            off_t       m_file_size = 0;
            // 当前读取漂移位置
            off_t       m_cur_offset = 0;
            // 分区位置
            off_t       m_pa_offset = 0;
            // 分批读取的块大小
            size_t      m_chunk_size = 0;
            // 分批读取的块总数
            size_t      m_num_chunks = 0;
            // 当前分批读取的块下标
            size_t      m_cur_chunk_index = 0;
            // 当前读取的内存地址
            void*       m_cur_read_addr = MAP_FAILED;
            // 当前读取的内存长度
            size_t      m_cur_read_length = 0;
        };

        // 从后往前读取
        class ReverseReader {
        public:
            ReverseReader() : m_item_size(1) {}
            ReverseReader(int item_size) : m_item_size(item_size) {}
            ~ReverseReader() { close(); }

            error open(const char* file, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // 打开文件
                struct stat sb;
                m_fd = ::open(file, O_RDONLY);
                if (m_fd == -1 || fstat(m_fd, &sb) == -1) {
                    return create_error(error_t::open_fail);
                }

                // 获取文件总大小
                m_file_size = sb.st_size;
                // 块大小对齐
                m_chunk_size = chunk_size * m_item_size;
                if (m_chunk_size > (size_t)sb.st_size) {
                    m_chunk_size = sb.st_size;
                }
                // 计算块总数
                if (m_chunk_size > 0) {
                    // 上取整
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
                // 清空上次读取
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
                // 获取下一边界
                m_pa_offset = m_cur_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
                // 最后一块需要对齐
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
            // 子项大小
            int         m_item_size;
            // 文件句柄
            int         m_fd = -1;
            // 文件总大小
            off_t       m_file_size = 0;
            // 当前读取漂移位置
            off_t       m_cur_offset = 0;
            // 分区位置
            off_t       m_pa_offset = 0;
            // 分批读取的块大小
            size_t      m_chunk_size = 0;
            // 分批读取的块总数
            size_t      m_num_chunks = 0;
            // 当前分批读取的块下标
            size_t      m_cur_chunk_index = 0;
            // 当前读取的内存地址
            void*       m_cur_read_addr = MAP_FAILED;
            // 当前读取的内存长度
            size_t      m_cur_read_length = 0;
        };

        // 追加mmap
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
                // 文件大小超过最大值, 需要扩展文件大小
                if (ftruncate(m_fd, new_file_size) == -1) {
                    return create_error(error_t::ftruncate_fail);
                }
                // 开启文件映射并追加写
                off_t pa_offset = m_cur_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
                void* map = mmap(NULL, new_file_size - pa_offset, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, pa_offset);
                if (map == MAP_FAILED) {
                    return create_error(error_t::mmap_fail);
                }
                memcpy((char*)map, data.data(), data.length());
                m_cur_offset += data.length();

                // 取消文件映射并关闭文件描述符
                if (munmap(map, new_file_size - pa_offset) == -1) {
                    return create_error(error_t::munmap_fail);
                }
                return error(error_t::ok);
            }

            void close() {
                ::close(m_fd);
            }

        private:
            // 文件句柄
            int     m_fd = -1;
            // 当前读取漂移位置
            off_t   m_cur_offset = 0;
        };
    };
}