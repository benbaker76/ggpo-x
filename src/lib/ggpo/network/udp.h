/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _UDP_H
#define _UDP_H

#include "poll.h"
#include "udp_msg.h"
#include "ggponet.h"
#include "ring_buffer.h"
#define MAX_UDP_ENDPOINTS     16

static const int MAX_UDP_PACKET_SIZE = 4096;

/* An optional filter on outgoing packets, for emulating a network that
 * misbehaves (latency, jitter, packet loss) without needing a real one.
 *
 *   filter  return non-zero to have Udp send the packet itself as usual;
 *           return 0 to take ownership of it -- hold it, or drop it.
 *   pump    called on every network poll, so a filter holding packets has a
 *           regular opportunity to release the ones now due.
 *
 * Both null by default, which costs one predictable branch per send. */
typedef int  (*UdpSendFilter)(int socket, const char *buffer, int len, int flags,
                              const struct sockaddr *dst, int dstlen);
typedef void (*UdpPumpFn)(int socket);

class Udp : public IPollSink
{
public:
   struct Stats {
      int      bytes_sent;
      int      packets_sent;
      float    kbps_sent;
   };

   struct Callbacks {
      virtual ~Callbacks() { }
      virtual void OnMsg(sockaddr_in &from, UdpMsg *msg, int len) = 0;
   };


protected:
   void Log(const char *fmt, ...);

public:
   /* Install (or clear, with nulls) the transport filter above. Process-wide. */
   static void SetTransportFilter(UdpSendFilter filter, UdpPumpFn pump);

   Udp();

   void Init(uint16 port, Poll *p, Callbacks *callbacks);
   /* False when Init could not create or bind the socket. */
   bool IsBound() const { return _socket != INVALID_SOCKET; }
   
   bool SendTo(char *buffer, int len, int flags, struct sockaddr *dst, int destlen, int& errorCode);

   bool OnLoopPoll() override;

public:
   ~Udp(void);

protected:
   // Network transmission information
   SOCKET         _socket;

   // state management
   Callbacks      *_callbacks;
};

#endif
