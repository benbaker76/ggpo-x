/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include <time.h>

static struct timespec start = { 0, 0 };

/* Milliseconds since the first call -- plus one, so it is NEVER 0.
 *
 * GGPO uses 0 to mean "not yet": UdpProtocol::OnLoopPoll re-sends a sync request only
 * `if (_last_send_time && _last_send_time + interval < now)`. This clock started at 0,
 * so the very first sync request of a process was stamped 0, and if THAT packet was
 * lost the retry never fired: both peers sat waiting for each other for good. Windows'
 * timeGetTime is never 0 at startup, which is why upstream never saw it. Found by the
 * packet-tampering test (SWOS United netplay_netem.c), which loses first packets. */
uint32 Platform::GetCurrentTimeMS() {
    if (start.tv_sec == 0 && start.tv_nsec == 0)
        clock_gettime(CLOCK_MONOTONIC, &start);
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);

    return (uint32)(((current.tv_sec - start.tv_sec) * 1000) +
                    ((current.tv_nsec - start.tv_nsec) / 1000000)) + 1;
}

/* Same as platform_windows.cpp: no runtime configuration, so logging stays off. */
int Platform::GetConfigInt(const char *)
{
    return 0;
}

bool Platform::GetConfigBool(const char *)
{
    return false;
}
