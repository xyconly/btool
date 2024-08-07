/*************************************************
File name:  file_system.hpp
Author:
Version:
Date:
Description:    windows�ļ�ϵͳ������,����ϵͳ����ʵ��
*************************************************/
#pragma once
#include <tuple>
#include <string>
#include <vector>
#define PATH_DELIMITER '/'
#define PATH_BACKSLASH_DELIMITER '\\'

#if defined(_MSC_VER)

#include <io.h>
#include <direct.h>
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tchar.h>

namespace BTool {

    class FileSystem
    {
    public:
        FileSystem() {}
        ~FileSystem() {}

        // ����Ŀ¼����Ŀ¼
        // folder: ����������ʱ���Ŀ¼
        static bool CreateDir(const std::string& folder) {
            std::string folder_builder;
            std::string sub;
            sub.reserve(folder.size());
            for (auto it = folder.begin(); it != folder.end(); ++it)
            {
                //cout << *(folder.end()-1) << endl;
                const char c = *it;
                sub.push_back(c);
                if (c == PATH_DELIMITER || c == PATH_BACKSLASH_DELIMITER || it == folder.end() - 1)
                {
                    folder_builder.append(sub);
                    if (0 != ::_access(folder_builder.c_str(), 0))
                    {
                        // this folder not exist
                        if (0 != ::_mkdir(folder_builder.c_str()))
                        {
                            // create failed
                            return false;
                        }
                    }
                    sub.clear();
                }
            }
            return true;
        }

        // ɾ���ļ�
        static bool DeleteFile(const std::string& file_name) {
            return std::remove(file_name.c_str()) == 0;
        }

        static std::string GetAppPath()
        {

            char pBuf[_MAX_PATH] = {};

            GetModuleFileNameA(nullptr, pBuf, _MAX_PATH);
            char* pch = strchr(pBuf, '\\');
            if (pch)
                *(pch + 1) = 0;
            else
                strcat_s(pBuf, _MAX_PATH, "\\");
            return pBuf;
        }

        // fullFilePath: �ļ���ȫ·��, �ļ��������Ǵ����ļ�
        // isOver: �Ƿ��Ը�����ʽд���ļ�,Ĭ����׷����ʽ����
        static bool WriteToFile(const char* fullFilePath, const char* msg, size_t len, bool isOver = false)
        {
            HANDLE hFile = CreateFileA(
                fullFilePath,
                GENERIC_WRITE,
                FILE_SHARE_WRITE,
                NULL,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile == INVALID_HANDLE_VALUE)
            {
                CloseHandle(hFile);        //һ��ע���ں����˳�֮ǰ�Ծ�������ͷš�
                return false;
            }

            if (!isOver)
                SetFilePointer(hFile, NULL, NULL, FILE_END);

            DWORD dwSize = 0;
            if (!WriteFile(hFile, msg, (DWORD)len, &dwSize, NULL))
            {
                CloseHandle(hFile);
                return false;
            }
            CloseHandle(hFile);
            return true;
        }
    };
}


#elif __GNUC__

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <iostream>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>
#include "boost_net/memory_stream.hpp"

namespace BTool {

    class FileSystem
    {
    public:
        FileSystem() {}
        ~FileSystem() {}

        static std::string GetAbsolutePath(const std::string& file_name) {
            if (file_name.empty()) {
                return file_name;
            }

            if (file_name[0] != '/') {
                return GetAppPath() + "/" + file_name;
            }

            return file_name;
        }

        static bool DeleteFile(const std::string & file_name)
        {
            return remove(file_name.c_str()) == 0;
        }

        static std::string GetAppPath()
        {
            char currentPath[PATH_MAX] = { 0 };
            return getcwd(currentPath, sizeof(currentPath));
        }

        static bool WriteToFile(const char* fullFilePath, const char* msg, size_t len, bool isOver = false) {

            FILE *fp;
            if (!isOver) {
                if ((fp = fopen(fullFilePath, "ab+")) == NULL)
                    return false;
            }
            else {
                if ((fp = fopen(fullFilePath, "w+")) == NULL)
                    return false;
            }

            if(!fwrite(msg, len, 1, fp)){
                fclose(fp);
                return false;
            }

            fclose(fp);
            return true;
        }

        static bool Exists(const std::string& path) {
            struct stat buffer;
            if (stat(path.c_str(), &buffer) == 0) {
                return true;
            }
            return false;
        }

        static bool CreateDir(const std::string& path)
        {
            int beginCmpPath;
            int endCmpPath;
            int pathLen = path.length();
            char currentPath[PATH_MAX] = { 0 };
            //���·��
            if ('/' != path[0])
            {
                //��ȡ��ǰ·��
                char* tmp = getcwd(currentPath, sizeof(currentPath));
                if (!tmp) return false;
                strcat(currentPath, "/");
                beginCmpPath = strlen(currentPath);
                strcat(currentPath, path.c_str());
                if (path[pathLen] != '/')
                {
                    strcat(currentPath, "/");
                }
                endCmpPath = strlen(currentPath);
            }
            else
            {
                //����·��
                strcpy(currentPath, path.c_str());
                if (path[pathLen] != '/')
                {
                    strcat(currentPath, "/");
                }
                beginCmpPath = 1;
                endCmpPath = strlen(currentPath);
            }
            //��������Ŀ¼
            for (int i = beginCmpPath; i < endCmpPath; i++)
            {
                if ('/' == currentPath[i])
                {
                    currentPath[i] = '\0';
                    if (access(currentPath, 0) != 0)
                    {
                        if (mkdir(currentPath, 0755) == -1)
                            return false;
                    }
                    currentPath[i] = '/';
                }
            }
            return true;
        }
    
        static std::string GetDir(const std::string& filePath) {
            size_t pos = filePath.find_last_of("/\\");
            if (pos != std::string::npos) {
                return filePath.substr(0, pos);
            }
            return "";
        }

        // ��ȡ�ļ��������е��ļ���, �������ݹ��ȡ
        static std::tuple<bool, std::vector<std::string>> GetDirectFile(const std::string& path)
        {
            std::vector<std::string> result;
            DIR* directory = opendir(path.c_str()); // ��Ŀ¼
            if (directory == nullptr) {
                return std::forward_as_tuple(false, result);
            }

            dirent* entry;
            while ((entry = readdir(directory)) != nullptr) {
                std::string filename = entry->d_name;
                if (filename != "." && filename != "..") {
                    result.emplace_back(filename);
                }
            }

            closedir(directory);
            return std::forward_as_tuple(true, result);
        }

        // ����һ���ݹ麯�������ڻ�ȡָ��·���µ������ļ���Ŀ¼
        static bool GetFilesRecursively(const std::string& path, std::vector<std::string>& fileList) {
            DIR* dir = opendir(path.c_str());
            if (dir == nullptr) {
                // std::cerr << "Error opening directory: " << path << std::endl;
                return false;
            }

            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string entryName = entry->d_name;
                if (entryName != "." && entryName != "..") {
                    std::string fullPath = path + "/" + entryName;
                    struct stat statbuf;
                    if (stat(fullPath.c_str(), &statbuf) != -1) {
                        if (S_ISDIR(statbuf.st_mode)) {
                            // �����Ŀ¼����ݹ����
                            GetFilesRecursively(fullPath, fileList);
                        } else {
                            // ������ļ�������ӵ��ļ��б���
                            fileList.push_back(fullPath);
                        }
                    }
                }
            }
            closedir(dir);
            return true;
        }
        
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
                std::cerr << "Error opening file for writing!file:" << filename << std::endl;
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
                // std::cerr << "Failed to resize shared memory\n";
                return false;
            }
            if (munmap(m_shm_ptr, m_shm_size) == -1) {
                // std::cerr << "Failed to unmap shared memory\n";
                return false;
            }
            m_shm_ptr = mmap(0, shm_size, m_mmap_type, MAP_SHARED, m_shm_fd, 0);
            if (m_shm_ptr == MAP_FAILED) {
                // std::cerr << "Failed to map shared memory\n";
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

}


#endif