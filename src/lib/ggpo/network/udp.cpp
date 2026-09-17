/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "udp.h"
#include <string>
SOCKET
CreateSocket(uint16 bind_port, int retries)
{
   SOCKET s;
   sockaddr_in sin;
   uint16 port;
   int optval = 1;

   s = socket(AF_INET, SOCK_DGRAM, 0);
   setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof optval);
#ifdef SO_DONTLINGER   /* Winsock only */
   setsockopt(s, SOL_SOCKET, SO_DONTLINGER, (const char *)&optval, sizeof optval);
#endif

   // non-blocking...
   u_long iMode = 1;
   ioctlsocket(s, FIONBIO, &iMode);

   sin.sin_family = AF_INET;
   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   for (port = bind_port; port <= bind_port + retries; port++) {
      sin.sin_port = htons(port);
      if (bind(s, (sockaddr *)&sin, sizeof sin) != SOCKET_ERROR) {
         Log("Udp bound to port: %d.\n", port);
         return s;
      }
   }
   closesocket(s);
   return INVALID_SOCKET;
}

Udp::Udp() :
   _socket(INVALID_SOCKET),
   _callbacks(NULL),
   _key0(0),
   _key1(0)
{
}

/* SWOS United: packet integrity.
 *
 * GGPO trusted every field of every packet it received; the only check was a
 * 16-bit magic number, which a damaged or forged packet keeps. One flipped bit in
 * an input packet's start_frame was enough to end a match, and the
 * disconnect_requested bit is a one-bit "end the match" switch for anyone who can
 * put a packet on the path.
 *
 * Every packet now carries SipHash-2-4 of itself under a per-session key, and a
 * packet that does not match is dropped before it is read; GGPO already resends
 * what is not acknowledged. SipHash because it is a keyed function built for
 * short inputs, small, and fast enough to run on every datagram. With the key a
 * peer never sent in the clear to strangers, a party off the path cannot make a
 * packet that is accepted; a party ON the path can read the key if it crossed the
 * same network, but can also simply drop packets, which no checksum prevents. */
static inline uint64 rotl64(uint64 x, int b) { return (x << b) | (x >> (64 - b)); }

static inline uint64 read_le64(const uint8 *p)
{
   return (uint64)p[0] | ((uint64)p[1] << 8) | ((uint64)p[2] << 16) | ((uint64)p[3] << 24) |
          ((uint64)p[4] << 32) | ((uint64)p[5] << 40) | ((uint64)p[6] << 48) | ((uint64)p[7] << 56);
}

#define SIPROUND                                                         \
   do {                                                                  \
      v0 += v1; v1 = rotl64(v1, 13); v1 ^= v0; v0 = rotl64(v0, 32);      \
      v2 += v3; v3 = rotl64(v3, 16); v3 ^= v2;                           \
      v0 += v3; v3 = rotl64(v3, 21); v3 ^= v0;                           \
      v2 += v1; v1 = rotl64(v1, 17); v1 ^= v2; v2 = rotl64(v2, 32);      \
   } while (0)

void Udp::SetPacketKey(const uint8 key[16])
{
   _key0 = read_le64(key);
   _key1 = read_le64(key + 8);
}

uint32 Udp::PacketMac(const uint8 *data, int len) const
{
   uint64 v0 = 0x736f6d6570736575ULL ^ _key0;
   uint64 v1 = 0x646f72616e646f6dULL ^ _key1;
   uint64 v2 = 0x6c7967656e657261ULL ^ _key0;
   uint64 v3 = 0x7465646279746573ULL ^ _key1;
   const uint8 *end = data + (len - (len % 8));
   uint64 m, b = (uint64)len << 56;

   for (; data != end; data += 8) {
      m = read_le64(data);
      v3 ^= m; SIPROUND; SIPROUND; v0 ^= m;
   }
   switch (len & 7) {
   case 7: b |= (uint64)data[6] << 48; /* fall through */
   case 6: b |= (uint64)data[5] << 40; /* fall through */
   case 5: b |= (uint64)data[4] << 32; /* fall through */
   case 4: b |= (uint64)data[3] << 24; /* fall through */
   case 3: b |= (uint64)data[2] << 16; /* fall through */
   case 2: b |= (uint64)data[1] << 8;  /* fall through */
   case 1: b |= (uint64)data[0];       break;
   case 0: break;
   }
   v3 ^= b; SIPROUND; SIPROUND; v0 ^= b;
   v2 ^= 0xff; SIPROUND; SIPROUND; SIPROUND; SIPROUND;
   uint64 h = v0 ^ v1 ^ v2 ^ v3;
   return (uint32)(h ^ (h >> 32));
}
#undef SIPROUND

Udp::~Udp(void)
{
   if (_socket != INVALID_SOCKET) {
      closesocket(_socket);
      _socket = INVALID_SOCKET;
   }
}

void
Udp::Init(uint16 port, Poll *poll, Callbacks *callbacks)
{
   _callbacks = callbacks;

   poll->RegisterLoop(this);

   Log("binding udp socket to port %d.\n", port);
   _socket = CreateSocket(port, 0);
}

/* The optional transport filter. Static rather than per-instance: a process has
 * one network, and a test that wants to model it wants to model all of it. */
static UdpSendFilter s_sendFilter = nullptr;
static UdpPumpFn     s_pump       = nullptr;

void Udp::SetTransportFilter(UdpSendFilter filter, UdpPumpFn pump)
{
   s_sendFilter = filter;
   s_pump       = pump;
}

bool Udp::SendTo(char *buffer, int len, int flags, struct sockaddr *dst, int destlen, int& errorcode)
{
   /* Optional transport filter (Udp::SetTransportFilter). A filter returning 0
    * has taken ownership of the packet -- held for later, or dropped -- and
    * this reports success, because from the sender's point of view that is
    * exactly what a lossy network looks like. Used to emulate latency and
    * packet loss without a real network; no filter installed, no cost. */
   if (s_sendFilter && !s_sendFilter(_socket, buffer, len, flags, dst, destlen)) {
      return true;
   }
    // Just for artificially triggering a network error
   /* struct sockaddr_in* to = (struct sockaddr_in*)dst;
    if (GetKeyState('A') & 0x8000 && ntohs(to->sin_port)==9567)
    {
        errorcode = 999;
        return false;
    }*/
   int res = sendto(_socket, buffer, len, flags, dst, destlen);
   if (res == SOCKET_ERROR)
   {
       errorcode = WSAGetLastError();
       return false;
   }
   if (res < len)
   {
       errorcode = res-len;
       return false;
   }
   return true;
 
 //  char dst_ip[1024];
 //  Log("sent packet length %d to %s:%d (ret:%d).\n", len, inet_ntop(AF_INET, (void *)&to->sin_addr, dst_ip, ARRAY_SIZE(dst_ip)), ntohs(to->sin_port), res);
}

bool
Udp::OnLoopPoll()
{
   /* Give a transport filter its regular tick: anything it is holding whose
    * delay has elapsed goes out now. This is the network's own heartbeat, so
    * it is the right place for it. */
   if (s_pump)
      s_pump(_socket);

   uint8          recv_buf[MAX_UDP_PACKET_SIZE];
   sockaddr_in    recv_addr;
   socklen_t      recv_addr_len;

   for (;;) {
      recv_addr_len = sizeof(recv_addr);
      int len = recvfrom(_socket, (char *)recv_buf, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);

      // TODO: handle len == 0... indicates a disconnect.

      if (len == -1) {
         int error = WSAGetLastError();
         if (error != WSAEWOULDBLOCK) {
            Log("recvfrom WSAGetLastError returned %d (%x).\n", error, error);
         }
         break;
      } else if (len > 0) {
         char src_ip[1024];
         Log("recvfrom returned (len:%d  from:%s:%d).\n", len, inet_ntop(AF_INET, (void*)&recv_addr.sin_addr, src_ip, ARRAY_SIZE(src_ip)), ntohs(recv_addr.sin_port) );
         UdpMsg *msg = (UdpMsg *)recv_buf;
         _callbacks->OnMsg(recv_addr, msg, len);
      } 
   }
   return true;
}


void
Udp::Log(const char *fmt, ...)
{
   char buf[1024];
   size_t offset;
   va_list args;

   strcpy_s(buf, "udp | ");
   offset = strlen(buf);
   va_start(args, fmt);
   vsnprintf(buf + offset, ARRAY_SIZE(buf) - offset - 1, fmt, args);
   buf[ARRAY_SIZE(buf)-1] = '\0';
   ::Log(buf);
   va_end(args);
}
