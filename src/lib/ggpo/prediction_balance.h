/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _PREDICTION_BALANCE_H
#define _PREDICTION_BALANCE_H

/*
 * PredictionBalance -- keeps one peer from doing nearly all the predicting.
 *
 * How many frames a peer plays on a prediction of the other's input ("depth":
 * the frame being played less the last confirmed frame, at the frame's first
 * play) is set by the connection, but which peer carries it is not.  The time
 * sync estimates the remote frame as the last input received plus HALF the
 * round trip, so on a route that is slower in one direction -- or simply
 * while it hunts -- one peer runs at depth 2, 3 or 4 for long spells while
 * the other runs at 0.
 *
 * Each peer knows its own depth exactly, so the two exchange a rolling
 * average of it (in the quality report) and the one predicting more is told
 * to wait.  The rules are Fightcade's "rift balancing", on that signal:
 *
 *   - a rolling average over WINDOW frames;
 *   - nothing is done until the two averages, over the SAME frames, are
 *     THRESHOLD apart.  Depth is whole frames: a peer at 1 with the other at 0
 *     is the least predicting a link short by under a frame allows, and
 *     "sharing" it puts BOTH at 1.  Only a gap of two frames or more can be
 *     closed without adding prediction, so that is the only thing closed;
 *   - the peer predicting more waits a QUARTER of the gap (a wait of w frames
 *     takes w off its depth and adds it to the other's, so the gap halves);
 *     the other peer does nothing;
 *   - then nothing more until a window made only of frames played after that
 *     wait was complete (SPREAD frames, over which the caller should spread
 *     it): a correction shows in the average only once the window has moved
 *     past it.
 *
 * Only the recommendation is made here, as GGPO_EVENTCODE_PREDICTION_BALANCE.
 * The caller waits, as it does for GGPO_EVENTCODE_TIMESYNC -- wall time only,
 * nothing the simulation sees -- and the time sync is left exactly as it is.
 * (Feeding the gap into the time sync's own figure as a standing offset was
 * tried and ran away: that figure is acted on in full every 120 frames and
 * answers slowly, so a half-frame offset was overshot by five frames.)
 */
class PredictionBalance {
public:
   enum {
      WINDOW      = 90,       /* frames an average covers */
      HISTORY     = 512,      /* frames of depth kept; a power of two */
      THRESHOLD   = 150,      /* hundredths of a frame apart before anything is done */
      SPREAD      = 50,       /* frames a wait should be spread over */
      MAX_DEPTH   = 15
   };

   PredictionBalance();
   void Reset();

   /* A frame's first play, `depth` frames past the last confirmed one. */
   void Frame(int frame, int depth);

   /* The local average, in hundredths, over the WINDOW frames ending at the
    * newest frame played.  False until WINDOW frames have been. */
   bool Report(int *frame, int *average) const;

   /* The remote peer's Report. */
   void Remote(int frame, int average);

   /* True when the local peer should wait `frames` of a frame, spread over
    * SPREAD frames.  Each recommendation is given once. */
   bool Recommend(float *frames, float *local, float *remote);

private:
   int Average(int frame) const;

   unsigned char  _depth[HISTORY];
   int            _top;                /* the newest frame in _depth; -1 */
   int            _remote_frame;
   int            _remote_average;
   bool           _remote_pending;
   int            _quiet_to;           /* no recommendation on a window starting before this frame */
};

#endif
