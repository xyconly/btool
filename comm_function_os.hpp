#ifndef __COMM_OS_H__
#define __COMM_OS_H__

/* C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* comm_os */

/* Softwares */

#ifdef _AIX	/* IBM AIX OS */
#define __COMM_AIX__
#endif

#ifdef __hpux	/* HP HP-UX OS */
#define __COMM_HPUX__
#endif

#ifdef __SVR4	/* Sun Solaris OS */
#define __COMM_SOLARIS__
#endif

#ifdef __FreeBSD__	/* Berkeley FreeBSD OS */
#define __COMM_FREEBSD__
#endif

#ifdef __linux	/* GNU Linux OS */
#define __COMM_LINUX__
#endif

#ifdef __APPLE__	/* Apple Mac OS X */
#define __COMM_APPLE__
#endif

#ifdef _WIN32	/* Microsoft Windows OS */
#define __COMM_WINDOWS__
#endif

/* Hardwares */
#ifdef __sparc	/* Sun Sparc Machine */
#define __COMM_SPARC__
#endif

#ifdef __ppc__ /* IBM PowerPC */
#define __COMM_POWERPC__
#endif

#if defined(__COMM_AIX__) || defined(__COMM_HPUX__) || defined(__COMM_SOLARIS__) || defined(__COMM_FREEBSD__) || defined(__COMM_LINUX__) || defined(__COMM_APPLE__)
#define __COMM_UNIX__
#endif

#if defined(__COMM_WINDOWS__)	/* windows */

#define WIN32_LEAN_AND_MEAN	/* for socket api */
#include <windows.h>
#elif defined(__COMM_UNIX__)	/* unix */

/* hp-ux */
#ifdef __COMM_HPUX__
#define _XOPEN_SOURCE_EXTENDED 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#else	/* Unknown OS */
#error Operation System not supported!
#endif	/* __COMM_WINDOWS__ */


#ifdef __COMM_WINDOWS__
#define comm_os_sleep(t) Sleep((t)*1000)	/* second */
#define comm_os_msleep Sleep			/* millisecond */
#else
#define comm_os_sleep sleep			/* second */
#define comm_os_msleep(t) usleep((t)*1000)			/* millisecond */
#endif

/* C++ */
#ifdef __cplusplus
}
#endif

#include <vector>

#ifdef __COMM_WINDOWS__
#include <conio.h>
// for mac
#include <Iphlpapi.h>
#include <Assert.h>
#pragma comment(lib, "iphlpapi.lib")
class CommonOS
{
public:
    static int getch() {
        return _getch();
    }

    static std::vector<std::string> get_mac(const char* ifr_name = "eth0")
    {
        std::vector<std::string> rslt;
        DWORD MACaddress = 0;
        IP_ADAPTER_INFO AdapterInfo[16];       // Allocate information
                                               // for up to 16 NICs
        DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

        DWORD dwStatus = GetAdaptersInfo(      // Call GetAdapterInfo
            AdapterInfo,                 // [out] buffer to receive data
            &dwBufLen);                  // [in] size of receive data buffer
        assert(dwStatus == ERROR_SUCCESS);  // Verify return value is
                                            // valid, no buffer overflow

        PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo; // Contains pointer to
                                                     // current adapter info
        char tmp[20];
        do {
            if (MACaddress == 0)
                MACaddress = pAdapterInfo->Address[5] + pAdapterInfo->Address[4] * 256
                + pAdapterInfo->Address[3] * 256 * 256
                + pAdapterInfo->Address[2] * 256 * 256 * 256;

            sprintf_s(tmp, "%02X:%02X:%02X:%02X:%02X:%02X", pAdapterInfo->Address[0], pAdapterInfo->Address[1], pAdapterInfo->Address[2], pAdapterInfo->Address[3], pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
            pAdapterInfo = pAdapterInfo->Next;    // Progress through linked list
            rslt.push_back(tmp);
        } while (pAdapterInfo);                    // Terminate if last adapter

        return rslt;
    }
};

#else

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

class CommonOS
{
public:

    static int getch() {
        struct termios oldtc, newtc;
        int ch;
        tcgetattr(STDIN_FILENO, &oldtc);
        newtc = oldtc;
        newtc.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newtc);
        ch=getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldtc);
        return ch;
    }

    static std::vector<std::string> get_mac(const char* ifr_name_str = "eth0")
    {

        std::vector<std::string> rslt;

        struct ifreq ifreq;
        int sock;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("socket");
            return rslt;
        }
        strcpy(ifreq.ifr_name, ifr_name_str);

        if (ioctl(sock, SIOCGIFHWADDR, &ifreq) < 0)
        {
            perror("ioctl");
            return rslt;
        }
        char mac[20] = { 0 };
        snprintf(mac, 20, "%X:%X:%X:%X:%X:%X", (unsigned char)ifreq.ifr_hwaddr.sa_data[0], (unsigned char)ifreq.ifr_hwaddr.sa_data[1], (unsigned char)ifreq.ifr_hwaddr.sa_data[2], (unsigned char)ifreq.ifr_hwaddr.sa_data[3], (unsigned char)ifreq.ifr_hwaddr.sa_data[4], (unsigned char)ifreq.ifr_hwaddr.sa_data[5]);
        rslt.push_back(mac);
        return rslt;
    }
}; 
#endif


#endif	/* __COMM_OS_H__ */
