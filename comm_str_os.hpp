/************************************************************************/
/*           对常见字符串拷贝操作等系统工具提供跨平台接口               */
/************************************************************************/

#pragma once

#ifdef _WIN32	/* Microsoft Windows OS */
#include <string>
# ifndef __COMM_WINDOWS__
#   define __COMM_WINDOWS__
# endif
#endif

#ifdef __linux	/* GNU Linux OS */
#include <string.h>
# ifndef __COMM_LINUX__
#   define __COMM_LINUX__
# endif
#endif

#ifdef __COMM_LINUX__
# ifndef __COMM_STR_CPY__
#   define __COMM_STR_CPY__
#   define strncpy_s(dest, source, count) strncpy(dest, source, count + 1) // 与WINDOWS对齐
# endif
#endif

