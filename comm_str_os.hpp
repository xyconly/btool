/************************************************************************/
/*           �Գ����ַ�������������ϵͳ�����ṩ��ƽ̨�ӿ�               */
/************************************************************************/

#pragma once
#include "comm_function_os.hpp"

#ifdef __COMM_WINDOWS__
#include <string>
#endif

#if defined(__COMM_APPLE__) || defined(__COMM_UNIX__)
#include <string.h>
# ifndef __COMM_STR_CPY__
#   define __COMM_STR_CPY__
#   define strncpy_s(dest, source, count) strncpy((dest), (source), (count))
# endif
#endif

