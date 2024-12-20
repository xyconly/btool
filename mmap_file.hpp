/*************************************************
File name:  mmap_file.hpp
Author:     AChar
Version:
Date:
Purpose: �ṩmmap�ļ�ӳ���д
Note:    �ļ�дʱ, �޷�ȷ�����ݰ�ȫд��
*************************************************/
#pragma once
#include <iomanip>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cerrno>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <tuple>
#include "boost_net/memory_stream.hpp"

namespace BTool {
    class MMapFile {
    public:
        // �������
        enum class error_t : int {
            ok = 0,
            unknow = -1,
            open_fail = -2,
            close_fail = -3,
            free_space_fail = -4,    // ʣ��ռ䲻��
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
            inline operator bool() const { return code_ != error_t::ok; }
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

        // ���ļ�, user�ɶ�д, �������ܶ�
        template<bool is_shm = true>
        static error OpenWriteFile(int& shm_fd, bool& is_create, const std::string& title, size_t shm_size) {
            is_create = false;
            // �򿪻򴴽������ڴ�, user�ɶ�д, �������ܶ�
            if constexpr (is_shm) {
                shm_fd = ::shm_open(title.c_str(), O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            }
            else {
                shm_fd = ::open(title.c_str(), O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            }
            if (shm_fd == -1) {
                // errno �̰߳�ȫ
                if (errno == EEXIST) {
                    // �����ڴ��Ѵ��ڣ����Խ���
                    if constexpr (is_shm) {
                        shm_fd = ::shm_open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    }
                    else {
                        shm_fd = ::open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    }
                    if (shm_fd == -1) {
                        return create_error(error_t::open_fail);
                    }
                } else {
                    return create_error(error_t::open_fail);
                }
            }
            else {
                is_create = true;
            }
            
            if (shm_fd != -1 && shm_size > 0) {
                // ������´����Ĺ����ڴ棬���������С
                if (ftruncate(shm_fd, shm_size) == -1) {
                    return create_error(error_t::ftruncate_fail);
                }
            }
            return create_error(error_t::ok);
        }
    
    public:
        // ֱ��ӳ��, ��ShmReaderWriter���������ڶ�д���̶���С
        template<typename _Ty>
        class Mapper {
        public:
            Mapper() = default;
            ~Mapper() { close(); }

            error open(const std::string& title) {
                bool is_create = false;
                // �򿪻򴴽������ڴ�, user�ɶ�д, �������ܶ�
                m_shm_fd = ::open(title.c_str(), O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                if (m_shm_fd == -1) {
                    // errno �̰߳�ȫ
                    if (errno == EEXIST) {
                        // �����ڴ��Ѵ��ڣ����Խ���
                        m_shm_fd = ::open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                        if (m_shm_fd == -1) {
                            return create_error(error_t::open_fail);
                        }
                    } else {
                        return create_error(error_t::open_fail);
                    }
                } else {
                    // ������´����Ĺ����ڴ棬���������С
                    if (ftruncate(m_shm_fd, m_shm_size) == -1) {
                        return create_error(error_t::ftruncate_fail);
                    }
                    is_create = true;
                }
                return open_impl(is_create);
            }

            error shm_open(const std::string& title) {
                bool is_create = false;
                // �򿪻򴴽������ڴ�, user�ɶ�д, �������ܶ�
                m_shm_fd = ::shm_open(title.c_str(), O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                if (m_shm_fd == -1) {
                    // errno �̰߳�ȫ
                    if (errno == EEXIST) {
                        // �����ڴ��Ѵ��ڣ����Խ���
                        m_shm_fd = ::shm_open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                        if (m_shm_fd == -1) {
                            return create_error(error_t::open_fail);
                        }
                    } else {
                        return create_error(error_t::open_fail);
                    }
                } else {
                    // ������´����Ĺ����ڴ棬���������С
                    if (ftruncate(m_shm_fd, m_shm_size) == -1) {
                        return create_error(error_t::ftruncate_fail);
                    }
                    is_create = true;
                }
                return open_impl(is_create);
            }

            _Ty* get() {
                return m_data;
            }

            // �رչ����ڴ�
            void close() {
                if (m_data != nullptr) {
                    munmap(static_cast<void*>(m_data), m_shm_size); // �ر�ӳ��
                    m_data = nullptr;
                }
                if (m_shm_fd != -1) {
                    ::close(m_shm_fd); // �رչ����ڴ��ļ�������
                    m_shm_fd = -1;
                }
            }

        private:
            error open_impl(bool is_create) {
                // ӳ�乲���ڴ棬ȷ��ӳ�������ڴ�����
                void* addr = mmap(NULL, m_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_fd, 0);
                if (addr == MAP_FAILED) {
                    return create_error(error_t::mmap_fail);
                }

                // ��ȡӳ��Ĺ����ڴ�����
                m_data = static_cast<_Ty*>(addr);
                if (is_create) {
                    if (std::is_constructible<_Ty, int>::value) {
                        *m_data = _Ty{0};
                    }
                    else {
                        *m_data = _Ty();
                    }
                }
                return error(error_t::ok);
            }

        private:
            int                 m_shm_fd = -1;       // �����ڴ��ļ�������
            size_t              m_shm_size = sizeof(_Ty);
            _Ty*                m_data = nullptr;
        };
    
        // ��˳��һ�ζ�ȡ, �´ζ�ȡ���ͷ���һ�εĶ�ȡmmap�ļ�, ���̰߳�ȫ
        // ע�����coreʱ�޷���֤���ݰ�ȫд��
        class Reader {
        public:
            explicit Reader(bool can_change = false) : m_can_change(can_change), m_item_size(1) {}
            explicit Reader(int item_size, bool can_change = false) : m_can_change(can_change), m_item_size(item_size) {}
            explicit Reader(size_t item_size, bool can_change = false) : m_can_change(can_change), m_item_size(item_size) {}
            ~Reader() { close(); }

            error open(const std::string& file, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // ���ļ�
                struct stat sb;
                if (!m_can_change)
                    m_fd = ::open(file.c_str(), O_RDONLY);
                else
                    m_fd = ::open(file.c_str(), O_RDWR);
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

            error open(const std::string& file, size_t chunk_size) {
                return open(file, m_item_size, chunk_size);
            }

            error shm_open(const std::string& title, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // ���ļ�
                struct stat sb;
                if (!m_can_change)
                    m_fd = ::shm_open(title.c_str(), O_RDONLY, S_IRUSR);
                else
                    m_fd = ::shm_open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
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

            error shm_open(const std::string& title, size_t chunk_size) {
                return shm_open(title, m_item_size, chunk_size);
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

            error open(const std::string& file, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // ���ļ�
                struct stat sb;
                m_fd = ::open(file.c_str(), O_RDONLY);
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

            error open(const std::string& file, size_t chunk_size) {
                return open(file, m_item_size, chunk_size);
            }

            error shm_open(const std::string& title, int item_size, size_t chunk_size) {
                m_item_size = item_size;
                // ���ļ�
                struct stat sb;
                m_fd = ::shm_open(title.c_str(), O_RDONLY, S_IRUSR);
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

            error shm_open(const std::string& title, size_t chunk_size) {
                return shm_open(title, m_item_size, chunk_size);
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

        // ׷��mmap, ÿ�ξ����´�/�ر�
        class Writer {
        public:
            Writer() = default;
            ~Writer() { close(); }

            error open(const std::string& file, bool is_append = true) {
                m_fd = ::open(file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                if (is_append) {
                    m_cur_offset = ::lseek(m_fd, 0, SEEK_END);
                    if (m_cur_offset == -1) {
                        return create_error(error_t::open_fail);
                    }
                }
                return error(error_t::ok);
            }

            error shm_open(const std::string& title, bool is_append = true) {
                m_fd = ::shm_open(title.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
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
                if (m_fd != -1) {
                    ::close(m_fd); // �رչ����ڴ��ļ�������
                    m_fd = -1;
                }
            }

        private:
            // �ļ����
            int     m_fd = -1;
            // ��ǰ��ȡƯ��λ��
            off_t   m_cur_offset = 0;
        };
        
        // �ṩ���ڹ̶���������д��Ĵ��ڴ�, �ļ����Ȳ��̶�, ��ʵʱ����
        class VectorBuffer {
        protected:
    #pragma pack(push, 1)
            // ���λ�����Ԫ���ݽṹ
            struct MetaSt {
                size_t  item_size_;     // ÿ��д������Ĵ�С
                size_t  write_len_;     // ��д�����ݳ���
                size_t  file_len_;      // ���ݻ���������
                char    data_[0];       // ���ݻ�����
            };
    #pragma pack(pop)
        public:
            // ׷��mmap, ÿ�ξ�Ԥ�����д��, ���ֶ�Ϊ���ļ��ĳ���
            class Writer {
            public:
                Writer() = default;
                ~Writer() {
                    close();
                }

                error open(const std::string& file, size_t item_size, bool is_append = true, const off_t& init_buf_length = 4*1024*1024, const off_t& buf_step = 1024*1024) {
                    bool is_create = false;
                    auto err = MMapFile::OpenWriteFile<false>(m_fd, is_create, file, 0);
                    if (err) return err;
                    m_buf_step = buf_step;
                    err = check_add_length(init_buf_length);
                    if (err) return err;
                    if (is_create || !is_append) {
                        m_p_meta->write_len_ = 0;
                        m_p_meta->item_size_ = item_size;
                    }
                    return err;
                }

                error shm_open(const std::string& title, size_t item_size, bool is_append = true, const off_t& init_buf_length = 4*1024*1024, const off_t& buf_step = 1024*1024) {
                    bool is_create = false;
                    auto err = MMapFile::OpenWriteFile<true>(m_fd, is_create, title, 0);
                    if (err) return err;
                    m_buf_step = buf_step;
                    err = check_add_length(init_buf_length);
                    if (err) return err;
                    if (is_create || !is_append) {
                        m_p_meta->write_len_ = 0;
                        m_p_meta->item_size_ = item_size;
                    }
                    return err;
                }

                void reset_offset(const off_t& off) { 
                    m_p_meta->write_len_ = off;
                }

                template<typename _Ty>
                error write(_Ty&& data) {
                    return write((const char*)(&data), sizeof(_Ty));
                }

                error write(const char* data, const size_t& len) {
                    if (len == 0) {
                        return error(error_t::ok);
                    }
                    auto err = check_add_length(len);
                    if (err) {
                        return err;
                    }
                    
                    memcpy((char*)(m_data + m_p_meta->write_len_), data, len);
                    m_p_meta->write_len_ += len;
                    return error(error_t::ok);
                }

                void close() {
                    // ȡ���ļ�ӳ�䲢�ر��ļ�������
                    if (m_p_meta) {
                        auto write_len = m_p_meta->write_len_;
                        auto file_len = m_p_meta->file_len_;
                        m_p_meta->file_len_ = write_len + sizeof(MetaSt);
                        munmap((void*)m_p_meta, file_len);
                        auto ret = ftruncate(m_fd, write_len + sizeof(MetaSt));
                        (void)ret;
                        m_p_meta = nullptr;
                        m_data = nullptr;
                    }

                    if (m_fd != -1) {
                        ::close(m_fd);
                        m_fd = -1;
                    }
                }

            private:
                error check_add_length(const off_t& len) {
                    // ʣ��ռ����
                    size_t write_len = 0;
                    size_t file_len = 0;
                    if (m_p_meta) {
                        if (m_p_meta->write_len_ + len <= m_p_meta->file_len_) {
                            return error(error_t::ok);
                        }
                        write_len = m_p_meta->write_len_;
                        file_len = m_p_meta->file_len_;

                        // �ļ���С�������ֵ, ��Ҫ��չ�ļ���С
                        // ȡ���ļ�ӳ�䲢�ر��ļ�������
                        if (munmap((void*)m_p_meta, m_p_meta->file_len_) == -1) {
                            return create_error(error_t::munmap_fail);
                        }
                        m_p_meta = nullptr;
                        m_data = nullptr;
                    }

                    while (write_len + sizeof(MetaSt) + len > file_len) {
                        file_len += m_buf_step;
                    }
                    file_len = file_len & ~(sysconf(_SC_PAGE_SIZE) - 1);
                    if (ftruncate(m_fd, file_len) == -1) {
                        return create_error(error_t::ftruncate_fail);
                    }
                    // �����ļ�ӳ�䲢׷��д
                    void* data = mmap(NULL, file_len, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
                    if (data == MAP_FAILED) {
                        return create_error(error_t::mmap_fail);
                    }
                    m_p_meta = (MetaSt*)data;
                    m_p_meta->file_len_ = file_len;
                    m_data = (char*)data + sizeof(MetaSt);
                    return error(error_t::ok);
                }

            private:
                // �ļ����
                int         m_fd = -1;
                // ÿ�ε�������ֵ
                off_t       m_buf_step = 0;
                // �ļ�ʵ���ܳ��ȴ洢ָ��
                MetaSt*     m_p_meta = nullptr;
                // mmap��ʼ��ַ
                char*       m_data = nullptr;
            };
            
            // ���̰߳�ȫ
            class Reader {
            public:
                explicit Reader(bool can_change = false) : m_can_change(can_change), m_item_size(1) {}
                ~Reader() { close(); }

                error open(const std::string& file, size_t chunk_size) {
                    // ���ļ�
                    auto err = init<false>(file);
                    if (err) return err;
                    return open_impl(chunk_size);
                }

                error shm_open(const std::string& title, size_t chunk_size) {
                    // ���ļ�
                    auto err = init<true>(title);
                    if (err) return err;
                    return open_impl(chunk_size);
                }

                inline long long get_obj_num() const {
                    return m_meta.write_len_ / m_item_size;
                }

                inline size_t get_obj_size() const {
                    return m_item_size;
                }

                inline void* get_data() const {
                    return m_cur_read_addr;
                }

                inline size_t get_chunk_index() const {
                    return m_cur_chunk_index;
                }

                template<typename Type>
                std::tuple<error, Type*> read_next() {
                    if (m_cur_read_buf_length < m_cur_chunk_offset + sizeof(Type)) {
                        auto [err, item, count] = read<Type>();
                        if (err || count == 0)
                            return std::forward_as_tuple(err, nullptr);
                        m_cur_chunk_offset += sizeof(Type);
                        return std::forward_as_tuple(err, item); 
                    }
                    char* data = (char*)m_cur_read_addr + m_cur_chunk_offset;
                    m_cur_chunk_offset += sizeof(Type);
                    return std::forward_as_tuple(error_t::ok, (Type*)data);
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
                    if ((size_t)m_cur_file_offset + m_cur_read_buf_length >= (size_t)m_meta.write_len_) {
                        return std::forward_as_tuple(error_t::ok, std::string_view{});
                    }

                    calc_current();
                    if (!m_can_change)
                        m_cur_read_addr = mmap(NULL, m_cur_read_chunk_length, PROT_READ, MAP_SHARED, m_fd, m_pa_offset);
                    else
                        m_cur_read_addr = mmap(NULL, m_cur_read_chunk_length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, m_pa_offset);

                    if (m_cur_read_addr == MAP_FAILED) {
                        return std::forward_as_tuple(create_error(error_t::mmap_fail), std::string_view{});
                    }
                    m_cur_chunk_index++;
                    return std::forward_as_tuple(error_t::ok, std::string_view((char*)m_cur_read_addr + m_cur_file_offset - m_pa_offset, m_cur_read_buf_length));
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
                template<bool is_shm>
                error init(const std::string& title) {
                    Mapper<MetaSt> meta_mapping;
                    error err;
                    if constexpr (is_shm)
                        err = meta_mapping.shm_open(title);
                    else 
                        err = meta_mapping.open(title);
                    if (err) {
                        return err;
                    }
                    auto p_meta = meta_mapping.get();
                    m_file_size = sizeof(MetaSt) + p_meta->write_len_;
                    m_item_size = p_meta->item_size_;
                    m_meta = *p_meta;

                    if constexpr (is_shm) {
                        if (!m_can_change) 
                            m_fd = ::shm_open(title.c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                        else
                            m_fd = ::shm_open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    }
                    else {
                        if (!m_can_change) 
                            m_fd = ::open(title.c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                        else
                            m_fd = ::open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    }
                    return error(error_t::ok);
                }
                error open_impl(size_t chunk_size) {
                    size_t data_size = m_meta.write_len_;
                    // ��ȡ�ļ��ܴ�С
                    // ���С����
                    m_chunk_size = chunk_size * m_item_size;
                    if (m_chunk_size > data_size) {
                        m_chunk_size = data_size;
                    }
                    // ���������
                    if (m_chunk_size > 0) {
                        // ��ȡ��
                        m_num_chunks = (data_size + m_chunk_size - 1) / m_chunk_size;
                    }
                    return error(error_t::ok);
                }

                error_t clean_mmap() {
                    error_t err_code = error_t::ok;
                    if (m_cur_read_addr != MAP_FAILED) {
                        if (munmap(m_cur_read_addr, m_cur_read_chunk_length) == -1) {
                            err_code = error_t::munmap_fail;
                        }
                        m_cur_read_addr = MAP_FAILED;
                    }
                    return err_code;
                }

                void calc_current() {
                    m_cur_chunk_offset = 0;
                    m_cur_file_offset = sizeof(MetaSt) + m_cur_chunk_index * m_chunk_size;
                    
                    // ���һ����Ҫ����
                    m_cur_read_buf_length = (m_cur_chunk_index < m_num_chunks - 1) ? m_chunk_size : m_meta.write_len_ - m_cur_chunk_index * m_chunk_size;

                    // ��ȡ��һ�߽�
                    m_pa_offset = m_cur_file_offset & ~(m_page_size - 1);
                    m_cur_read_chunk_length = m_cur_read_buf_length + m_cur_file_offset - m_pa_offset;
                }

            private:
                // ҳ��С
                off_t       m_page_size = sysconf(_SC_PAGE_SIZE);
                // �Ƿ�ɸı��ļ�����
                bool        m_can_change;
                // �����С
                int         m_item_size;
                // �ļ����
                int         m_fd = -1;
                // �ļ��ܴ�С
                off_t       m_file_size = 0;
                // ��ǰ��ȡƯ��λ��(������sizeof(MetaSt))
                off_t       m_cur_chunk_offset = 0;
                // ����sizeof(MetaSt)
                off_t       m_cur_file_offset = 0;
                // ������ʼƯ��λ��
                off_t       m_pa_offset = 0;
                // ������ȡ�Ŀ��С
                size_t      m_chunk_size = 0;
                // ������ȡ�Ŀ�����
                size_t      m_num_chunks = 0;
                // ��ǰ������ȡ�Ŀ��±�
                size_t      m_cur_chunk_index = 0;
                // ��ǰ��ȡ���ڴ��ַ
                void*       m_cur_read_addr = MAP_FAILED;
                // ��ǰ��ȡ���ڴ泤��(������sizeof(MetaSt))
                size_t      m_cur_read_buf_length = 0;
                // ��ǰ��ȡ�Ŀ��С, ��ҳ����
                size_t      m_cur_read_chunk_length = 0;
                // ָ�����ڴ�ӳ���Ԫ����
                MetaSt      m_meta;
            };            
        };
       
        // �ṩ���ڶ�̬����д��Ĵ��ڴ�, �ļ����ȹ̶�, ���ʱ����
        class FixBuffer {
        protected:
    #pragma pack(push, 1)
            // ��̬����Ԫ���ݽṹ
            struct MetaSt {
                bool    has_finished_;  // �����д��
                size_t  write_len_;     // ��д�����ݳ���
                size_t  file_size_;     // ���ݻ���������
                char    data_[0];       // ���ݻ�����
            };
    #pragma pack(pop)

        public:
            // ���̰߳�ȫ
            class Writer {
            public:
                Writer() = default;
                ~Writer() { close(); }

                error open(const std::string& file, size_t buffer_size, bool is_append = true) {
                    bool is_create = false;
                    auto err = MMapFile::OpenWriteFile<false>(m_shm_fd, is_create, file, 0);
                    if (err) return err;
                    return open_impl(is_create, buffer_size, is_append);
                }

                error shm_open(const std::string& title, size_t buffer_size, bool is_append = true) {
                    bool is_create = false;
                    auto err = MMapFile::OpenWriteFile<true>(m_shm_fd, is_create, title, 0);
                    if (err) return err;
                    return open_impl(is_create, buffer_size, is_append);
                }

                template<typename _Ty>
                error write(_Ty&& data) {
                    size_t start_offset = 0;
                    return write(&data, sizeof(_Ty), start_offset);
                }
                
                error write(const char* msg, const size_t& len) {
                    size_t start_offset = 0;
                    return write(msg, len, start_offset);
                }

                // д����Ϣ������д��ǰ����ʼƫ����
                template<typename _Ty>
                std::enable_if_t<!std::is_pointer_v<_Ty>, error> write(_Ty&& data, size_t& start_offset) {
                    return write(&data, sizeof(_Ty), start_offset);
                }
                
                error write(const char* msg, const size_t& len, size_t& start_offset) {
                    // ���ʣ��ռ��Ƿ��㹻
                    if (!check_res_space(len)) {
                        return create_error(error_t::free_space_fail);
                    }
                    start_offset = m_meta->write_len_;
                    // ������д�뻺����
                    memcpy(m_meta->data_ + start_offset, msg, len);
                    // ������д������ݳ���
                    m_meta->write_len_ += len;
                    return error(error_t::ok);
                }

                void reset_offset(off_t off) { 
                    m_meta->write_len_ = off;
                }

                // ��ȡ�������ַ
                error get_data(const size_t& len, char*& data, size_t& start_offset) {
                    start_offset = m_meta->write_len_;
                    // ���ʣ��ռ��Ƿ��㹻
                    if (m_meta->has_finished_ || !check_res_space(len)) {
                        return create_error(error_t::free_space_fail);
                    }
                    m_meta->write_len_ += len;
                    data = m_meta->data_ + start_offset;
                    return error(error_t::ok);
                }
                
                void add_write_len(const size_t& len) {
                    m_meta->write_len_ += len;
                }

                void finished_write() {
                    // ��Ϊ��ʵ���ݴ�С
                    if (m_meta != nullptr) {
                        m_meta->has_finished_ = true;
                        auto real_buffer_size = m_meta->file_size_;
                        auto file_size = m_meta->file_size_ = m_meta->write_len_ + sizeof(MetaSt);
                        munmap(m_meta, real_buffer_size);
                        auto ret = ftruncate(m_shm_fd, file_size);
                        (void)ret;
                        m_meta = nullptr;
                    }
                    close();
                }

                // �رչ����ڴ�
                void close() {
                    if (m_meta != nullptr) {
                        auto real_file_size = m_meta->file_size_;
                        munmap(m_meta, real_file_size);
                        m_meta = nullptr;
                    }
                    if (m_shm_fd != -1) {
                        ::close(m_shm_fd); // �رչ����ڴ��ļ�������
                        m_shm_fd = -1;
                    }
                }

            protected:
                error open_impl(bool is_create, size_t buffer_size, bool is_append) {
                    // ���ù����ڴ��С
                    size_t shm_size = sizeof(MetaSt) + buffer_size;
                    size_t real_buffer_size = shm_size & ~(sysconf(_SC_PAGE_SIZE) - 1);

                    if (ftruncate(m_shm_fd, real_buffer_size) == -1) {
                        return create_error(error_t::ftruncate_fail);
                    }

                    // ӳ�乲���ڴ棬ȷ��ӳ�������ڴ�����
                    void* addr = mmap(NULL, real_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_fd, 0);
                    if (addr == MAP_FAILED) {
                        return create_error(error_t::mmap_fail);
                    }

                    // ��ȡӳ��Ĺ����ڴ�����
                    m_meta = static_cast<MetaSt*>(addr);
                    if (is_create || !is_append) {
                        *m_meta = MetaSt{false, 0, real_buffer_size};
                    }
                    return error(error_t::ok);
                }

                // ���ʣ��ռ��Ƿ����
                inline bool check_res_space(const size_t& len) {
                    return len <= m_meta->file_size_ - sizeof(MetaSt) - m_meta->write_len_;
                }

            protected:
                int             m_shm_fd = -1;       // �����ڴ��ļ�������
                MetaSt*         m_meta = nullptr;    // ָ�����ڴ�ӳ���Ԫ����
            };

            // ���̰߳�ȫ
            class Reader {
            public:
                Reader(bool can_change = false) : m_can_change(can_change) {}
                ~Reader() { close(); }

                error open(const std::string& file) {
                    {
                        Mapper<MetaSt> meta_mapping;
                        auto err = meta_mapping.open(file);
                        if (err) {
                            return err;
                        }
                        auto p_meta = meta_mapping.get();
                        m_file_size = sizeof(MetaSt) + p_meta->write_len_;
                    }
                    // ���ļ�
                    if (!m_can_change)
                        m_fd = ::open(file.c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    else
                        m_fd = ::open(file.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    return open_impl();
                }

                error shm_open(const std::string& title) {
                    {
                        Mapper<MetaSt> meta_mapping;
                        auto err = meta_mapping.shm_open(title);
                        if (err) {
                            return err;
                        }
                        auto p_meta = meta_mapping.get();
                        m_file_size = sizeof(MetaSt) + p_meta->write_len_;
                    }
                    // ���ļ�
                    if (!m_can_change)
                        m_fd = ::shm_open(title.c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    else
                        m_fd = ::shm_open(title.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

                    return open_impl();
                }

                void* get_data() const {
                    return m_data;
                }

                size_t get_length() const {
                    return m_meta->write_len_;
                }

                bool finished() const {
                    return m_meta->has_finished_;
                }

                void close() {
                    if (m_read_addr != nullptr) {
                        munmap(static_cast<void*>(m_read_addr), m_file_size); // �ر�ӳ��
                        m_read_addr = nullptr;
                        m_meta = nullptr;
                        m_data = nullptr;
                    }
                    if (m_fd != -1) {
                        ::close(m_fd); // �رչ����ڴ��ļ�������
                        m_fd = -1;
                    }
                }

            private:
                error open_impl() {
                    struct stat sb;
                    if (m_fd == -1 || fstat(m_fd, &sb) == -1 || (size_t)sb.st_size < m_file_size) {
                        return create_error(error_t::open_fail);
                    }

                    if (!m_can_change)
                        m_read_addr = mmap(NULL, m_file_size, PROT_READ, MAP_SHARED, m_fd, 0);
                    else
                        m_read_addr = mmap(NULL, m_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);

                    if (m_read_addr == MAP_FAILED) {
                        return create_error(error_t::mmap_fail);
                    }

                    m_meta = static_cast<MetaSt*>(m_read_addr);
                    if (sizeof(MetaSt) <= m_file_size)
                        m_data = (char*)m_read_addr + sizeof(MetaSt);
                    return error(error_t::ok);
                }

            private:
                // �Ƿ�ɸı��ļ�����
                bool        m_can_change;
                // �ļ����
                int         m_fd = -1;
                // ��ǰ��ȡ���ڴ��ַ
                size_t      m_file_size = 0;
                // ��ǰ��ȡ���ڴ��ַ
                void*       m_read_addr = MAP_FAILED;
                // ָ�����ڴ�ӳ���Ԫ����
                MetaSt*     m_meta = nullptr;
                char*       m_data = nullptr;
            };
        };

        class BatchWriter {
        public:
            // over_write: �Ƿ񸲸�ԭ�ļ�
            // realtime_flush: �Ƿ�ʵʱˢ��
            BatchWriter(const std::string& filename, bool over_write = true, bool realtime_flush = false, size_t buffer_size = 8192)
                : m_file(nullptr)
                , m_realtime_flush(realtime_flush)
                , m_buffer(buffer_size + 1024)
                , m_buffer_size(buffer_size)
            {
                if (over_write) {
                    m_file = std::fopen(filename.c_str(), "wb");
                }
                else {
                    m_file = std::fopen(filename.c_str(), "ab");
                    std::fseek(m_file, 0, SEEK_END);
                }

                if (!m_file) {
                    throw std::runtime_error("Error opening file for writing!file:" + filename);
                }
            }

            ~BatchWriter() {
                if (m_file) {
                    flush();
                    std::fclose(m_file);
                }
            }

            // ��ȡ��� n ���ַ�
            static std::string ReadLastByte(const std::string& filename, size_t len)
            {
                std::string ret;
                FILE* file = std::fopen(filename.c_str(), "rb"); // ���ı��ļ�
                if (file == nullptr) 
                    return ret;

                // ȷ���ļ��Ĵ�С
                fseek(file, 0, SEEK_END);
                long fileSize = ftell(file);

                // ����Ҫ��ȡ���ַ�����ʼλ��
                long startPos = fileSize - len;

                // ��� startPos �Ƿ�Ϊ����������ǣ�������Ϊ 0
                if (startPos < 0) {
                    startPos = 0;
                }

                // �����ļ�ָ�뵽��ʼλ��
                std::fseek(file, startPos, SEEK_SET);
                // ��ȡ n ���ַ�
                char* lastNChars = (char*)malloc(len + 1); // +1 ������ֹ null �ַ�
                if (lastNChars != NULL) {
                    std::fread(lastNChars, 1, len, file);
                    lastNChars[len] = '\0'; // ��� null ��ֹ���Դ��� C �ַ���
                    ret = std::string(lastNChars, len);
                    std::free(lastNChars); // �ͷ��ڴ�
                }

                std::fclose(file); // �ر��ļ�
                return ret;
            }

            bool empty() const {
                return std::ftell(m_file) == 0;
            }

            void reset_flush(bool realtime_flush) {
                m_realtime_flush = realtime_flush;
            }

            template<typename Type>
            void write(Type&& data) {
                m_buffer.append(std::forward<Type>(data));
                if (m_buffer.length() >= m_buffer_size) {
                    flush();
                }
            }

            void write_string(const char* data) {
                write((const void*)data, strlen(data));
            }
            void write_string(const std::string& data) {
                write((const void*)data.data(), data.length());
            }

            void write_string_line(const char* data) {
                write_line((const void*)data, strlen(data));
            }
            void write_string_line(const std::string& data) {
                write_line((const void*)data.data(), data.length());
            }

            void write(const void* buffer, size_t len) {
                m_buffer.append(buffer, len);
                if (m_realtime_flush || m_buffer.length() >= m_buffer_size) {
                    flush();
                }
            }

            void write_line(const void* buffer, size_t len) {
                m_buffer.append(buffer, len);
                m_buffer.append("\n", 1);
                if (m_realtime_flush || m_buffer.length() >= m_buffer_size) {
                    flush();
                }
            }

            void flush() {
                if (!m_buffer.empty()) {
                    std::fwrite(m_buffer.data(), sizeof(char), m_buffer.size(), m_file);
                    std::fflush(m_file);
                    m_buffer.clear();
                }
            }

            template<typename Type>
            void write_now(Type&& data) {
                std::fwrite((const char*)(&data), sizeof(char), sizeof(Type), m_file);
            }

            void write_now(const void* buffer, size_t len) {
                std::fwrite((const char*)buffer, sizeof(char), len, m_file);
            }
        private:
            FILE*                   m_file;
            bool                    m_realtime_flush;
            BTool::MemoryStream     m_buffer;
            size_t                  m_buffer_size;
        };

        class ShmReaderWriter {
        public:
            enum RWType { READER = O_RDONLY, WRITER = O_RDWR, READER_WRITER = WRITER };

            // exit_unlink: �˳�ʱ�Զ��رչ����ڴ�
            ShmReaderWriter(RWType type, bool exit_unlink = false)
                : m_type(type)
                , m_shm_size(0)
                , m_exit_unlink(exit_unlink)
                , m_shm_fd(-1)
                , m_shm_ptr(MAP_FAILED)
            {
                switch (type)
                {
                case RWType::READER:
                    m_mmap_type = PROT_READ;
                    break;
                case RWType::READER_WRITER:
                    m_mmap_type = PROT_READ | PROT_WRITE;
                    break;
                default:
                    throw std::invalid_argument("Invalid RWType");
                }
            }

            ~ShmReaderWriter() {
                close_and_unmap();
                if (m_exit_unlink) {
                    unlink();
                }
            }

            // ��ʼ��
            // shm_name: �����ڴ���
            // force_create: ��������ʱ�Ƿ�ǿ�ƴ���
            // shm_size Ϊ0 ��ʾ���¶�ȡ�����ڴ��С
            // shm_size ��Ϊ0��ֱ�Ӹı��С
            // default_value: ������ʱ��Ĭ���ڴ�
            bool init(const std::string& shm_name, bool force_create = false, size_t shm_size = 0, const void* default_value = nullptr) {
                m_shm_fd = shm_open(shm_name.c_str(), m_type, 0666);
                if (m_shm_fd == -1) {
                    // ��ǿ�ƴ�����ֱ���˳�
                    if (!force_create || shm_size == 0)
                        return false;

                    // ���������ڴ沢���ó�ʼ��С
                    m_shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
                    if (m_shm_fd == -1) {
                        // std::cerr << "Failed to create shared memory\n";
                        return false;
                    }
                    if (ftruncate(m_shm_fd, shm_size) == -1) {
                        close(m_shm_fd);
                        m_shm_fd = -1;
                        return false;
                    }
                    // ӳ�乲���ڴ沢д��Ĭ��ֵ
                    void* ptr = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_fd, 0);
                    if (ptr == MAP_FAILED) {
                        close(m_shm_fd);
                        m_shm_fd = -1;
                        // std::cerr << "Failed to map shared memory\n";
                        return false;
                    }
                    if (default_value) {
                        memcpy(ptr, default_value, shm_size);
                    } else {
                        memset(ptr, 0, shm_size);
                    }
                    munmap(ptr, shm_size);
                }
                else if (shm_size > 0) {
                    // ����ӳ�乲���ڴ������ڵ�shm_size
                    size_t existing_size = get_shared_memory_size(m_shm_fd);
                    if (shm_size != existing_size) {
                        if (ftruncate(m_shm_fd, shm_size) == -1) {
                            close(m_shm_fd);
                            m_shm_fd = -1;
                            return false;
                        }
                    }
                }

                m_shm_name = shm_name;
                
                if (shm_size == 0) {
                    shm_size = get_shared_memory_size(m_shm_fd);
                }
                m_shm_size = shm_size;

                m_shm_ptr = mmap(0, m_shm_size, m_mmap_type, MAP_SHARED, m_shm_fd, 0);
                if (m_shm_ptr == MAP_FAILED) {
                    // std::cerr << "Failed to map shared memory\n";
                    close(m_shm_fd);
                    m_shm_fd = -1;
                    return false;
                }
                return true;
            }

            // shm_size Ϊ0 ��ʾ���¶�ȡ�����ڴ��С
            // shm_size ��Ϊ0��ֱ�Ӹı��С
            bool resize(size_t shm_size = 0) {
                if (shm_size == 0) {
                    shm_size = get_shared_memory_size(m_shm_fd);
                }

                if (shm_size == m_shm_size) {
                    return true;
                }

                if (ftruncate(m_shm_fd, shm_size) == -1) {
                    return false;
                }
                if (munmap(m_shm_ptr, m_shm_size) == -1) {
                    return false;
                }
                m_shm_ptr = mmap(0, shm_size, m_mmap_type, MAP_SHARED, m_shm_fd, 0);
                if (m_shm_ptr == MAP_FAILED) {
                    throw std::runtime_error("Failed to map shared memory!");
                    return false;
                }
                m_shm_size = shm_size;
                return true;
            }

            void* read() {
                if (m_shm_fd == -1 || m_shm_ptr == MAP_FAILED)
                    return nullptr;

                return m_shm_ptr;
            }

            bool write(const void* data, size_t size) {
                if (m_type == READER || m_shm_fd == -1 || m_shm_ptr == MAP_FAILED) return false;
                if (size > m_shm_size) return false;
                memcpy(m_shm_ptr, data, size);
                return true;
            }

            void unlink() {
                if (!m_shm_name.empty())
                    shm_unlink(m_shm_name.c_str());
            }

            void close_and_unmap() {
                if (m_shm_ptr != MAP_FAILED) {
                    munmap(m_shm_ptr, m_shm_size);
                    m_shm_ptr = MAP_FAILED;
                }
                if (m_shm_fd != -1) {
                    close(m_shm_fd);
                    m_shm_fd = -1;
                }
            }
            
        private:
            static size_t get_shared_memory_size(int shm_fd) {
                struct stat shm_stat;
                if (fstat(shm_fd, &shm_stat) == -1) {
                    // std::cerr << "Failed to get shared memory size\n";
                    return 0;
                }
                return shm_stat.st_size;
            }


        private:
            // ��д����
            RWType              m_type;
            // mmapӳ���ʽ
            int                 m_mmap_type;
            // �����ڴ���
            std::string         m_shm_name;
            // �����ڴ��С
            size_t              m_shm_size;
            // �˳�ʱ�Զ��رչ����ڴ�
            bool                m_exit_unlink;
            // �����ڴ��ļ����
            int                 m_shm_fd;
            // �����ڴ�ָ��
            void*               m_shm_ptr;

            char*               m_buffer = nullptr;
        };
    };
}