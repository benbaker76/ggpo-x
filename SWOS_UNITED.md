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

**Two additions.**

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

## Keeping it

Rebase onto a newer upstream rather than merging, so the diff stays readable as
a patch series. After any change, SWOS United must still pass:

```bash
bin/SWOS.x86_64 --netplay-selftest        # GGPO's own tests + the link
scripts/netplay_test.sh --modes "direct p2p"   # a recorded match, frame for frame
scripts/netplay_live_test.sh              # two processes playing each other
```
