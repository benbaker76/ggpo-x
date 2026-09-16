/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include <time.h>

static struct timespec start = { 0, 0 };

uint32 Platform::GetCurrentTimeMS() {
    if (start.tv_sec == 0 && start.tv_nsec == 0) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        return 0;
    }
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);

    return (uint32)(((current.tv_sec - start.tv_sec) * 1000) +
                    ((current.tv_nsec - start.tv_nsec) / 1000000));
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
