/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GGPO_LINUX_H_
#define _GGPO_LINUX_H_

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * SWOS United: the POSIX platform layer (Linux, macOS, Cygwin).
 *
 * Upstream ggpo-x has only ever been built on Windows: this header and its .cpp
 * did not compile, and the network and logging code call Winsock and the MSVC
 * secure CRT directly. Rather than touch every call site, the Windows names they
 * use are provided here on top of BSD sockets and the C library. See
 * third_party/ggpo-x/SWOS_UNITED.md for the full patch list.
 */
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <climits>
#include <stdexcept>
#include <utility>      /* std::move -- <windows.h> pulls it in transitively */

#define TRUE     1
#define FALSE    0
#define MAX_PATH PATH_MAX

typedef int SOCKET;
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)
#define WSAEWOULDBLOCK  EWOULDBLOCK

static inline int WSAGetLastError() { return errno; }
static inline int closesocket(SOCKET s) { return close(s); }

/* Only FIONBIO is used (udp.cpp); fcntl is the portable spelling of it. */
static inline int ioctlsocket(SOCKET s, long cmd, u_long *argp)
{
   if (cmd == (long)FIONBIO) {
      int fl = fcntl(s, F_GETFL, 0);
      if (fl < 0) {
         return -1;
      }
      return fcntl(s, F_SETFL, *argp ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
   }
   return ioctl(s, (unsigned long)cmd, argp);
}

static inline void OutputDebugStringA(const char *) { }
static inline void DebugBreak() { abort(); }
static inline int CreateDirectoryA(const char *path, void *) { return mkdir(path, 0777) == 0; }

template <size_t N>
inline int sprintf_s(char (&buf)[N], const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   int n = vsnprintf(buf, N, fmt, args);
   va_end(args);
   return n;
}

inline int sprintf_s(char *buf, size_t size, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   int n = vsnprintf(buf, size, fmt, args);
   va_end(args);
   return n;
}

inline int vsprintf_s(char *buf, size_t size, const char *fmt, va_list args)
{
   return vsnprintf(buf, size, fmt, args);
}

inline int strncat_s(char *dst, size_t size, const char *src, size_t count)
{
   size_t len = strnlen(dst, size);
   if (len + 1 >= size) {
      return ERANGE;
   }
   strncat(dst, src, count < size - len - 1 ? count : size - len - 1);
   return 0;
}

template <size_t N>
inline int strcpy_s(char (&dst)[N], const char *src)
{
   snprintf(dst, N, "%s", src);
   return 0;
}

inline int fopen_s(FILE **fp, const char *name, const char *mode)
{
   *fp = fopen(name, mode);
   return *fp ? 0 : errno;
}

/* <windows.h> min/max, as functions rather than macros so they cannot collide
 * with std::min/std::max in the standard headers. */
template <class A, class B>
inline auto min(A a, B b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template <class A, class B>
inline auto max(A a, B b) -> decltype(a > b ? a : b) { return a > b ? a : b; }

class Platform {
public:  // types
   typedef pid_t ProcessID;

public:  // functions
   static ProcessID GetProcessID() { return getpid(); }
   static void AssertFailed(char *msg) { fprintf(stderr, "%s\n", msg); }
   static uint32 GetCurrentTimeMS();
   static int GetConfigInt(const char *name);
   static bool GetConfigBool(const char *name);
};

#endif
