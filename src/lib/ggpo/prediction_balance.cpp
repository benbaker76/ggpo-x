/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "prediction_balance.h"
#include <string.h>

PredictionBalance::PredictionBalance()
{
   Reset();
}

void
PredictionBalance::Reset()
{
   memset(_depth, 0, sizeof _depth);
   _top = -1;
   _remote_frame = 0;
   _remote_average = 0;
   _remote_pending = false;
   _quiet_to = 0;
}

void
PredictionBalance::Frame(int frame, int depth)
{
   if (frame < 0) {
      return;
   }
   if (depth < 0) {
      depth = 0;
   }
   _depth[frame & (HISTORY - 1)] = (unsigned char)(depth < MAX_DEPTH ? depth : MAX_DEPTH);
   _top = frame;
}

/*
 * The average over the WINDOW frames ending at `frame`, in hundredths; -1
 * when the history does not hold them all.
 */
int
PredictionBalance::Average(int frame) const
{
   int first = frame - WINDOW + 1, sum = 0;

   if (frame > _top || first < 0 || _top - first >= HISTORY) {
      return -1;
   }
   for (int f = first; f <= frame; f++) {
      sum += _depth[f & (HISTORY - 1)];
   }
   return sum * 100 / WINDOW;
}

bool
PredictionBalance::Report(int *frame, int *average) const
{
   int a = Average(_top);
   if (a < 0) {
      return false;
   }
   *frame = _top;
   *average = a;
   return true;
}

void
PredictionBalance::Remote(int frame, int average)
{
   if (frame < 0 || average < 0 || average > MAX_DEPTH * 100) {
      return;
   }
   if (_remote_pending && frame <= _remote_frame) {
      return;                          /* an older report, late */
   }
   _remote_frame = frame;
   _remote_average = average;
   _remote_pending = true;
}

bool
PredictionBalance::Recommend(float *frames, float *local, float *remote)
{
   if (!_remote_pending) {
      return false;
   }
   /*
    * Like with like: the local average over the frames the remote one covers.
    * The remote peer may be a frame or two ahead; then this waits until the
    * local one has played them.
    */
   int mine = Average(_remote_frame);
   if (mine < 0) {
      if (_remote_frame <= _top) {
         _remote_pending = false;      /* too old to compare: the next one */
      }
      return false;
   }
   _remote_pending = false;

   int gap = mine - _remote_average;
   if (gap < THRESHOLD) {
      return false;                    /* level enough, or the other peer's to close */
   }
   if (_remote_frame - WINDOW + 1 < _quiet_to) {
      return false;                    /* the last wait is not behind this window yet */
   }
   _quiet_to = _top + SPREAD + 1;
   *frames = gap / 400.0f;             /* a quarter of the gap */
   *local = mine / 100.0f;
   *remote = _remote_average / 100.0f;
   return true;
}
