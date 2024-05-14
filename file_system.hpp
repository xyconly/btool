/*************************************************
File name:  file_system.hpp
Author:
Version:
Date:
Description:    windows文件系统操作类,其他系统后期实现
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

        // 创建目录及子目录
        // folder: 可以是运行时相对目录
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

        // 删除文件
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

        // fullFilePath: 文件名全路径, 文件不存在是创建文件
        // isOver: 是否以覆盖形式写入文件,默认以追加形式加入
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
                CloseHandle(hFile);        //一定注意在函数退出之前对句柄进行释放。
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
#include <sys/stat.h>
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
            //相对路径
            if ('/' != path[0])
            {
                //获取当前路径
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
                //绝对路径
                strcpy(currentPath, path.c_str());
                if (path[pathLen] != '/')
                {
                    strcat(currentPath, "/");
                }
                beginCmpPath = 1;
                endCmpPath = strlen(currentPath);
            }
            //创建各级目录
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

        // 获取文件夹下所有的文件名, 不包含递归获取
        static std::tuple<bool, std::vector<std::string>> GetDirectFile(const std::string& path)
        {
            std::vector<std::string> result;
            DIR* directory = opendir(path.c_str()); // 打开目录
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
    
    };

    class BatchWriter {
    public:
        // over_write: 是否覆盖原文件
        // realtime_flush: 是否实时刷新
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

        // 读取最后 n 个字符
        static std::string ReadLastByte(const std::string& filename, size_t len)
        {
            std::string ret;
            FILE* file = std::fopen(filename.c_str(), "rb"); // 打开文本文件
            if (file == nullptr) 
                return ret;

            // 确定文件的大小
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);

            // 计算要读取的字符的起始位置
            long startPos = fileSize - len;

            // 检查 startPos 是否为负数，如果是，则设置为 0
            if (startPos < 0) {
                startPos = 0;
            }

            // 设置文件指针到起始位置
            std::fseek(file, startPos, SEEK_SET);
            // 读取 n 个字符
            char* lastNChars = (char*)malloc(len + 1); // +1 用于终止 null 字符
            if (lastNChars != NULL) {
                std::fread(lastNChars, 1, len, file);
                lastNChars[len] = '\0'; // 添加 null 终止符以创建 C 字符串
                ret = std::string(lastNChars, len);
                std::free(lastNChars); // 释放内存
            }

            std::fclose(file); // 关闭文件
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
}


#endif