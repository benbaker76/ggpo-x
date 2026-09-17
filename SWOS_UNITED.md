# The `swos-united` branch

This branch is what SWOS United (github.com/benbaker76/SWOS-United) builds
against, as a submodule at `third_party/ggpo-x`.

**The patch list is the diff**, not a table someone has to keep honest:

```bash
git diff master...swos-united
```

Based on **`a24d115`** — the commit SWOS 2020 pins — rather than the tip of
`master`, because that is the base SWOS United's tests are green on. `master` is
several commits ahead; catching up is a separate change to make deliberately,
with those tests as the gate. Two of the commits in between look directly
relevant and worth reading first: `b0428eb` ("Big improvemnts in rift handling")
and `9f59543` ("Don't deal with input delay inside GGPO").

## What is on it

**Portability.** ggpo-x has only ever been built on Windows with MSVC: the Linux
platform file did not compile, and the network and logging code call Winsock and
the MSVC secure CRT directly. This builds with GCC and Clang on Linux, Cygwin
and macOS, and is callable from C without a bridge layer. Nothing here is
SWOS-specific and all of it should be useful to anyone off Windows.

**Four additions, and one fix.**

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

**The fix: `Platform::GetCurrentTimeMS` never returns 0** on the POSIX platform layer. It
returned 0 on its first call, so the first sync request was stamped 0 and
`UdpProtocol::OnLoopPoll` (`if (_last_send_time && ...)`) never re-sent it: if that
packet was lost, both peers waited for each other forever. Windows' `timeGetTime` is
never 0 at startup.

## Keeping it

Rebase onto a newer upstream rather than merging, so the diff stays readable as
a patch series. After any change, SWOS United must still pass:

```bash
bin/SWOS.x86_64 --netplay-selftest        # GGPO's own tests + the link
scripts/netplay_test.sh --modes "direct p2p"   # a recorded match, frame for frame
scripts/netplay_live_test.sh              # two processes playing each other
```
