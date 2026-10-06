# The `swos-united` branch

This branch is what SWOS United (github.com/benbaker76/SWOS-United) builds
against, as a submodule at `third_party/ggpo-x`.

**The patch list is the diff**, not a table someone has to keep honest:

```bash
git diff master...swos-united
```

Based on **`dd7fa14`**, upstream `master` as of 2026-09-17. It started on `a24d115`,
the commit SWOS 2020 pins, and caught up by a merge, gated on the tests below. What
the seven commits between them changed, for a two-player session like SWOS United's:

- **`ggpo_start_session` takes the frame rate** (`float fps`, afaeb04). The remote
  frame estimate used a hard-coded 60; SWOS plays at 50 (Amiga) or 70 (DOS).
- **Time sync subtracts the REMOTE peer's input delay** (b0428eb); it subtracted our
  own. Two peers with the same delay -- SWOS United always agrees one -- get the same
  `frames_ahead` as before. `TimeSync::_frameDelay2` is now initialised: it was sent
  uninitialised in the first sync request, and in every one when the delay is 0.
- Ping is a 10-second smoothed average, `MAX_FRAME_ADVANTAGE` is 30, the send and
  input queues are twice the size, and `remote_frames_behind` changed sign.
- `9f59543` moves input delay out of the *VectorWar demo* only (it now simulates with
  no delay and draws an older frame). The library's `ggpo_set_frame_delay` is unchanged.

## What is on it

**Portability.** ggpo-x has only ever been built on Windows with MSVC: the Linux
platform file did not compile, and the network and logging code call Winsock and
the MSVC secure CRT directly. This builds with GCC and Clang on Linux, Cygwin
and macOS, and is callable from C without a bridge layer. Nothing here is
SWOS-specific and all of it should be useful to anyone off Windows.

**Six additions, and two fixes.**

- `ggpo_get_confirmed_frame` — the sync layer's last confirmed frame. A peer may
  only end a match on a frame that nothing will roll back again, and upstream
  exposes only the *current* frame.
- `Udp::SetTransportFilter(filter, pump)` — an optional filter on outgoing
  packets, for emulating latency, jitter and packet loss without a real network.
  A filter returning 0 takes ownership of the packet (holds it, or drops it) and
  the send reports success, because that is what a lossy link looks like to a
  sender. Both pointers are null by default, so an unfiltered build pays one
  predictable branch per send. Deliberately generic: no SWOS types, no SWOS
  headers, nothing to strip if it ever goes upstream.
- `GGPO_ERRORCODE_NETWORK_ERROR` from `ggpo_start_session` / `ggpo_start_spectating`
  when the UDP socket could not be opened. Upstream ignores a failed bind and
  returns `GGPO_OK`, so a session on a port that is already taken runs with no
  socket and waits forever for a peer it cannot hear. Found on Windows, where WSL2's
  mirrored networking reserves a whole block of ports that nothing lists.
- **Packet integrity.** Every packet carries SipHash-2-4 of itself under a per-session
  key (`hdr.mac`, `Udp::PacketMac`) and must be exactly the size its header describes
  (`UdpMsg::SizeIsValid`); what fails is dropped before any field is read.
  `ggpo_set_packet_key` sets the 16-byte key, before `ggpo_add_player`; without it the
  key is zero, which still rejects damaged packets. Upstream checked only a 16-bit magic
  number, so one flipped bit in `start_frame` ended a match, and so could one forged
  packet with `disconnect_requested` set. **This changes the wire format**: a build with
  it and one without cannot play each other.

- **Prediction balance** (`lib/ggpo/prediction_balance.h`, `GGPO_PREDICTION_BALANCE`,
  on by default; 0 builds the library as it was). Keeps one peer from doing nearly all
  the predicting. The time sync estimates the remote frame as the last input received
  plus HALF the round trip, so on a route that is slower one way -- or with one machine a
  fraction of a frame ahead -- both peers are reported level while one plays 2, 3 or 4
  frames past the confirmed inputs and the other 0. Each peer knows its own depth
  exactly, so:
  - `Peer2PeerBackend::SyncInput` records the depth (frame - last confirmed frame) of
    every frame's first play;
  - the quality report carries the average over the last 90 frames and the frame that
    window ends at (`prediction_frame`, `prediction_depth` -- **two new fields: both
    peers must be built alike**);
  - `DoPoll` compares the remote average with the LOCAL one over the SAME frames. When
    the local peer is deeper by 1.5 frames or more it raises
    `GGPO_EVENTCODE_PREDICTION_BALANCE`: wait a quarter of the gap, spread over 50 frames.
    Then nothing until a window made only of frames played after that wait.

  The thresholds are Fightcade's rift balancing (act only on a gap of frames, rarely);
  the signal is not -- Fightcade uses the time sync's own figures, which cannot see the
  one-way case. A gap under two frames is deliberately left alone: depth is whole frames,
  and a peer at 1 with the other at 0 is the least a link short by under a frame allows
  ("sharing" it puts both at 1). The caller does the waiting, as for `TIMESYNC`.
  SWOS United's side and the measurements: its `docs/NETPLAY_BALANCE.md`.

- **`GGPO_SEND_FRAME_ADVANTAGE`** (default 1, GGPO's own behaviour). 0 sends no frame
  advantage in the quality report, so a peer's time sync recommendation rests on its own
  estimate alone (`-local / 2`). SWOS United builds with 0: its pacing was tuned that way
  (see the second fix below). With 1 the figure is converted through a signed int;
  upstream casts a negative float straight to `uint8`, which is undefined.

**Fix: `min` / `max` in `platform_linux.h` returned a dangling reference.** They stand in
for `<windows.h>`'s macros, and their return type was a bare `decltype(a < b ? a : b)` --
with both arguments of one type, a reference to one of the function's own parameters. The
one caller is the line that fills in `quality_report.frame_advantage`, so **every build of
this branch before 2026-10-06 sent garbage there** (compiled, it read four bytes of an
unrelated pointer as a float: nearly always 0), and the time sync steered on it. Now
`std::decay`. Upstream, on Windows, was never affected.

**The fix: `Platform::GetCurrentTimeMS` never returns 0** on the POSIX platform layer. It
returned 0 on its first call, so the first sync request was stamped 0 and
`UdpProtocol::OnLoopPoll` (`if (_last_send_time && ...)`) never re-sent it: if that
packet was lost, both peers waited for each other forever. Windows' `timeGetTime` is
never 0 at startup.

## Keeping it

**Merge** a newer upstream; do not rebase. SWOS United's history pins commits of this
branch as its submodule, and a rebase would rewrite them away -- every older SWOS United
commit would then point at a GGPO that no longer exists on the remote. The patch list
stays readable either way: `git diff master...swos-united` diffs from the merge base.
After any change, SWOS United must still pass:

```bash
bin/SWOS.x86_64 --netplay-selftest        # GGPO's own tests + the link
scripts/netplay_test.sh --modes "direct p2p"   # a recorded match, frame for frame
scripts/netplay_live_test.sh              # two processes playing each other
```
