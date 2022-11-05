/*************************************************
File name:  file_system.hpp
Author:
Version:
Date:
Description:    windows�ļ�ϵͳ������,����ϵͳ����ʵ��
*************************************************/
#pragma once
#include <string>
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
#include <sys/stat.h>
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
    };

}


#endif