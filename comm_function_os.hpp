#ifndef __COMM_OS_H__
#define __COMM_OS_H__


#ifdef _AIX /* IBM AIX OS */
#define __COMM_AIX__
#endif
#ifdef __hpux /* HP HP-UX OS */
#define __COMM_HPUX__
#endif
#ifdef __SVR4 /* Sun Solaris OS */
#define __COMM_SOLARIS__
#endif
#ifdef __FreeBSD__ /* Berkeley FreeBSD OS */
#define __COMM_FREEBSD__
#endif
#ifdef __linux /* GNU Linux OS */
#define __COMM_LINUX__
#endif
#ifdef __APPLE__ /* Apple Mac OS X */
#define __COMM_APPLE__
#endif
#ifdef _WIN32 /* Microsoft Windows OS */
#define __COMM_WINDOWS__
#endif
/* Hardwares */
#ifdef __sparc /* Sun Sparc Machine */
#define __COMM_SPARC__
#endif
#ifdef __ppc__ /* IBM PowerPC */
#define __COMM_POWERPC__
#endif
#if defined(__COMM_AIX__) || defined(__COMM_HPUX__) ||                         \
    defined(__COMM_SOLARIS__) || defined(__COMM_FREEBSD__) ||                  \
    defined(__COMM_LINUX__)
#define __COMM_UNIX__
#endif

#if defined(__COMM_WINDOWS__)
# define WIN32_LEAN_AND_MEAN /* for socket api */
#include <windows.h>

# ifndef LOAD_LIBRARY
#   define LOAD_LIBRARY(lib) LoadLibrary(lib)
# endif
# ifndef GET_LIBRARY_SYMBOL
#   define GET_LIBRARY_SYMBOL(lib, func) GetProcAddress((HMODULE)(lib), func)
# endif
# ifndef CLOSE_LIBRARY
#   define CLOSE_LIBRARY(lib) FreeLibrary((HMODULE)(lib))
# endif
# ifndef LIB_HANDLE
#   define LIB_HANDLE HMODULE
# endif
# ifndef EXPORT_LIBRARY_SYMBOL
#   define EXPORT_LIBRARY_SYMBOL __declspec(dllexport)
# endif

# define OS_Sleep(t) Sleep((t) * 1000) /* second */
# define OS_SleepMs Sleep               /* millisecond */

#elif defined(__COMM_APPLE__) || defined(__COMM_UNIX__)
/* hp-ux */
# ifdef __COMM_HPUX__
#  define _XOPEN_SOURCE_EXTENDED 1
# endif

# ifndef LIKELY
#  if defined(__COMM_LINUX__)
#    define LIKELY(x) (__builtin_expect((x), 1))
#  else
#    define LIKELY(x) (x)
#  endif
# endif

# ifndef UNLIKELY
#  if defined(__COMM_LINUX__)
#    define UNLIKELY(x) (__builtin_expect((x), 0))
#  else
#    define UNLIKELY(x) (x)
#  endif
# endif

#include <dlfcn.h>
# ifndef LOAD_LIBRARY
#   define LOAD_LIBRARY(lib) dlopen(lib, RTLD_LAZY | RTLD_LOCAL)
#   define LOAD_LIBRARY_GLOBAL(lib) dlopen(lib, RTLD_LAZY | RTLD_GLOBAL)
# endif
# ifndef GET_LIBRARY_SYMBOL
#   define GET_LIBRARY_SYMBOL(lib, func) dlsym(lib, func)
# endif
# ifndef CLOSE_LIBRARY
#   define CLOSE_LIBRARY(lib) dlclose(lib)
# endif
# ifndef LIB_HANDLE
#   define LIB_HANDLE void *
# endif
# ifndef EXPORT_LIBRARY_SYMBOL
#   define EXPORT_LIBRARY_SYMBOL __attribute__((visibility("default")))
# endif

#include <unistd.h>
# define OS_Sleep sleep                  /* second */
# define OS_SleepMs(t) usleep((t) * 1000) /* millisecond */
#endif


#ifdef __COMM_WINDOWS__
#include <string>
#include <vector>
#include <conio.h>
// for mac
#include <Assert.h>
#include <Iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")
class CommonOS {
public:
  static bool BindCore(int core_id) {
    HANDLE thread = GetCurrentThread();
    SetThreadAffinityMask(thread, 1 << core_id);
  }

  static int GetCh() { return _getch(); }

  static std::vector<std::string> GetMac() {
    std::vector<std::string> rslt;
    DWORD MACaddress = 0;
    IP_ADAPTER_INFO AdapterInfo[16];      // Allocate information
                                          // for up to 16 NICs
    DWORD dwBufLen = sizeof(AdapterInfo); // Save memory size of buffer

    DWORD dwStatus = GetAdaptersInfo( // Call GetAdapterInfo
        AdapterInfo,                  // [out] buffer to receive data
        &dwBufLen);                   // [in] size of receive data buffer
    // assert(dwStatus == ERROR_SUCCESS);  // Verify return value is
    //  valid, no buffer overflow

    PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to
                                                 // current adapter info
    char tmp[20];
    do {
      if (MACaddress == 0)
        MACaddress = pAdapterInfo->Address[5] + pAdapterInfo->Address[4] * 256 +
                     pAdapterInfo->Address[3] * 256 * 256 +
                     pAdapterInfo->Address[2] * 256 * 256 * 256;

      sprintf_s(tmp, "%02X:%02X:%02X:%02X:%02X:%02X", pAdapterInfo->Address[0],
                pAdapterInfo->Address[1], pAdapterInfo->Address[2],
                pAdapterInfo->Address[3], pAdapterInfo->Address[4],
                pAdapterInfo->Address[5]);
      pAdapterInfo = pAdapterInfo->Next; // Progress through linked list
      rslt.push_back(tmp);
    } while (pAdapterInfo); // Terminate if last adapter

    return rslt;
  }

  static std::vector<std::string> GetIps() {
    std::vector<std::string> result;
    IP_ADAPTER_INFO *pAdpFree = NULL;
    IP_ADAPTER_INFO *pIpAdpInfo =
        (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
    unsigned long ulBufLen = sizeof(IP_ADAPTER_INFO);
    int ret;
    // 第一次调用获取需要开辟的内存空间大小
    if ((ret = GetAdaptersInfo(pIpAdpInfo, &ulBufLen)) ==
        ERROR_BUFFER_OVERFLOW) {
      free(pIpAdpInfo);
      // 分配实际所需要的内存空间
      pIpAdpInfo = (IP_ADAPTER_INFO *)malloc(ulBufLen);
      if (NULL == pIpAdpInfo) {
        return result;
      }
    }

    if ((ret = GetAdaptersInfo(pIpAdpInfo, &ulBufLen)) == NO_ERROR) {
      pAdpFree = pIpAdpInfo;

      for (int i = 0; pIpAdpInfo; i++) {
        std::string addr;
        IP_ADDR_STRING *pIps = &pIpAdpInfo->IpAddressList;
        while (pIps) {
          addr = pIps->IpAddress.String;
          pIps = pIps->Next;
        }
        result.push_back(addr);
        pIpAdpInfo = pIpAdpInfo->Next;
      }
    }
    if (pAdpFree) {
      free(pAdpFree);
    }
    return result;
  }
};

#elif defined(__COMM_UNIX__)

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <termios.h>

class CommonOS {
public:
  static bool BindCore(int core_id) {
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(core_id, &cpuSet);

    if (sched_setaffinity(0, sizeof(cpuSet), &cpuSet) == -1) {
      return false;
    } else {
      return true;
    }
  }

  static int GetCh() {
    struct termios oldtc, newtc;
    int ch;
    tcgetattr(STDIN_FILENO, &oldtc);
    newtc = oldtc;
    newtc.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newtc);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldtc);
    return ch;
  }

  static std::vector<std::string> GetMac() {
    std::vector<std::string> rslt;

    int fdSock = 0;
    struct ifconf ifMyConf;
    struct ifreq ifMyReq;
    char szBuf[20480] = {0};

    ifMyConf.ifc_len = 2048;
    ifMyConf.ifc_buf = szBuf;

    if ((fdSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      return rslt;

    if (ioctl(fdSock, SIOCGIFCONF, &ifMyConf)) {
      close(fdSock);
      return rslt;
    }

    ifreq *it = ifMyConf.ifc_req;
    const struct ifreq *const end =
        it + (ifMyConf.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
      strcpy(ifMyReq.ifr_name, it->ifr_name);
      if (strcmp(ifMyReq.ifr_name, "lo") != 0 &&
          ioctl(fdSock, SIOCGIFADDR, &ifMyReq) == 0) {
        char mac[20] = {0};
        snprintf(mac, 20, "%X:%X:%X:%X:%X:%X",
                 (unsigned char)ifMyReq.ifr_hwaddr.sa_data[0],
                 (unsigned char)ifMyReq.ifr_hwaddr.sa_data[1],
                 (unsigned char)ifMyReq.ifr_hwaddr.sa_data[2],
                 (unsigned char)ifMyReq.ifr_hwaddr.sa_data[3],
                 (unsigned char)ifMyReq.ifr_hwaddr.sa_data[4],
                 (unsigned char)ifMyReq.ifr_hwaddr.sa_data[5]);
        rslt.push_back(mac);
      }
    }

    close(fdSock);
    return rslt;
  }

  static std::vector<std::string> GetIps() {
    std::vector<std::string> rslt;

    int fdSock = 0;
    struct ifconf ifMyConf;
    struct ifreq ifMyReq;
    char szBuf[20480] = {0};

    ifMyConf.ifc_len = 2048;
    ifMyConf.ifc_buf = szBuf;

    if ((fdSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      return rslt;

    if (ioctl(fdSock, SIOCGIFCONF, &ifMyConf)) {
      close(fdSock);
      return rslt;
    }

    ifreq *it = ifMyConf.ifc_req;
    const struct ifreq *const end =
        it + (ifMyConf.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
      strcpy(ifMyReq.ifr_name, it->ifr_name);
      if (strcmp(ifMyReq.ifr_name, "lo") != 0 &&
          ioctl(fdSock, SIOCGIFADDR, &ifMyReq) == 0) {
        struct sockaddr_in *sin;
        sin = (struct sockaddr_in *)&ifMyReq.ifr_addr;
        rslt.push_back((const char *)inet_ntoa(sin->sin_addr));
      }
    }

    close(fdSock);
    return rslt;
  }
};
#elif defined(__COMM_APPLE__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <termios.h>
#include <iomanip>
#include <thread>
#include <string>
#include <vector>
#include <sstream>

class CommonOS {
public:
    static bool BindCore(int core_id) {
        // mac 不支持
        // pthread_t thread = pthread_self();
        // struct sched_param param;
        // param.sched_priority = sched_get_priority_max(SCHED_RR);
        // pthread_setschedparam(thread, SCHED_RR, &param);
        return false;
    }

    static int GetCh() {
        struct termios oldtc, newtc;
        int ch;
        tcgetattr(STDIN_FILENO, &oldtc); // 获取当前终端设置
        newtc = oldtc;
        newtc.c_lflag &= ~(ICANON | ECHO);        // 禁用回显和缓冲
        tcsetattr(STDIN_FILENO, TCSANOW, &newtc); // 应用新设置
        ch = getchar();                           // 获取字符
        tcsetattr(STDIN_FILENO, TCSANOW, &oldtc); // 恢复旧设置
        return ch;
    }
    static std::vector<std::string> GetMac() {
        std::vector<std::string> rslt;
        struct ifaddrs *ifaddr, *ifa;

        if (getifaddrs(&ifaddr) == -1) {
          return rslt;
        }

        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
          if (ifa->ifa_addr == nullptr || !(ifa->ifa_flags & IFF_UP) ||
              (ifa->ifa_flags & IFF_LOOPBACK)) {
            continue;
          }

          if (ifa->ifa_addr->sa_family == AF_LINK) {
            struct sockaddr_dl *sdl = (struct sockaddr_dl *)ifa->ifa_addr;
            unsigned char *mac = (unsigned char *)LLADDR(sdl);

            std::ostringstream macStream;
            macStream << std::hex << std::setfill('0');
            for (int i = 0; i < sdl->sdl_alen; ++i) {
              macStream << std::setw(2) << static_cast<int>(mac[i]);
              if (i != sdl->sdl_alen - 1) {
                macStream << ":";
              }
            }
            rslt.push_back(macStream.str());
          }
        }

        freeifaddrs(ifaddr);
        return rslt;
    }

    static std::vector<std::string> GetIps() {
        std::vector<std::string> rslt;
        struct ifaddrs *ifaddr, *ifa;

        if (getifaddrs(&ifaddr) == -1) {
          return rslt;
        }

        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
          if (ifa->ifa_addr == nullptr || !(ifa->ifa_flags & IFF_UP) ||
              (ifa->ifa_flags & IFF_LOOPBACK)) {
            continue;
          }

          if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            if (inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip))) {
              rslt.push_back(ip);
            }
          } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char ip[INET6_ADDRSTRLEN];
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            if (inet_ntop(AF_INET6, &sin6->sin6_addr, ip, sizeof(ip))) {
              rslt.push_back(ip);
            }
          }
        }

        freeifaddrs(ifaddr);
        return rslt;
    }
};
#else /* Unknown OS */
#error Operation System not supported!
#endif /* __COMM_WINDOWS__ */

#endif /* __COMM_OS_H__ */
