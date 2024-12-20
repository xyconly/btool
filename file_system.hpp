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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <iostream>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>

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

        // 定义一个递归函数，用于获取指定路径下的所有文件和目录
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
                            // 如果是目录，则递归遍历
                            GetFilesRecursively(fullPath, fileList);
                        } else {
                            // 如果是文件，则添加到文件列表中
                            fileList.push_back(fullPath);
                        }
                    }
                }
            }
            closedir(dir);
            return true;
        }
        
    };

}


#endif