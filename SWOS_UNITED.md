# GGPO-X in SWOS United

Rollback networking library used by `src/netplay.c` (see `docs/NETPLAY.md`).

| | |
|---|---|
| Upstream | https://github.com/thomashenry79/ggpo-x (a fork of GGPO, MIT — see `LICENSE`) |
| Commit | `a24d115d4dc0616333d9031bc7e83f759365b430` ("change mtd options for 32-bit verison") |
| Taken from | `C:\Projects\GitHub\swos-2020\ggpo-x`, SWOS 2020's submodule, pinned at that commit |
| Copied | `LICENSE`, `README.md`, `src/include/`, `src/lib/` — the library only. Not the VectorWar sample app, the Visual Studio projects or the CMake files: the main `Makefile` builds it (`NETPLAY=1`). |

Keep the upstream layout, so a diff against a fresh checkout of that commit shows
exactly the patches below. When updating, re-apply them and re-run
`bin/SWOS.x86_64 --netplay-selftest`.

## Patches

Upstream ggpo-x has only ever been built on Windows with MSVC: its Linux platform
file did not compile, and the network and logging code call Winsock and the MSVC
secure CRT directly. SWOS United builds it with GCC/Clang on Linux, Cygwin and
macOS, and calls it from C.

| File | Change | Why |
|---|---|---|
| `src/include/ggponet.h` | `<stdint.h>`/`<stdbool.h>` for C; `GGPOSession` a `struct` in C; `typedef struct GGPOSessionCallbacks`; default member initializers and the `int&` of `ggpo_get_current_frame` behind `GGPO_DEFAULT`/`GGPO_OUT_REF`; empty `__cdecl` where the compiler has none | The API is `extern "C"` but the header was C++-only. With these, C includes it directly — no bridge layer. |
| `src/lib/ggpo/platform_linux.h` | Rewritten: BSD socket headers; `SOCKET`, `INVALID_SOCKET`, `SOCKET_ERROR`, `WSAGetLastError`, `WSAEWOULDBLOCK`, `closesocket`, `ioctlsocket(FIONBIO)` (via `fcntl`); `sprintf_s`, `vsprintf_s`, `strcpy_s`, `strncat_s`, `fopen_s`; `min`/`max` as function templates; `TRUE`/`FALSE`, `MAX_PATH`; `OutputDebugStringA` (no-op), `DebugBreak` (`abort`), `CreateDirectoryA` (`mkdir`); `<utility>`, `<climits>`, `<stdexcept>`; `Platform::GetConfigInt/GetConfigBool`; `AssertFailed` prints to stderr | The Windows names the library uses, on POSIX. Call sites untouched. |
| `src/lib/ggpo/platform_linux.cpp` | Missing semicolons; includes `types.h` (for `uint32`); `GetConfigInt/Bool` return 0/false like `platform_windows.cpp` | Did not compile. |
| `src/lib/ggpo/main.cpp` | `DllMain` under `#if defined(_WINDOWS)` | Windows DLL entry point. |
| `src/lib/ggpo/network/udp.cpp` | `SO_DONTLINGER` under `#ifdef`; `recvfrom` length is `socklen_t` | Winsock-only option; POSIX `recvfrom` takes `socklen_t *`. |
| `src/lib/ggpo/network/udp_proto.cpp` | `sin_addr.S_un.S_addr` -> `sin_addr.s_addr` | `S_un` is Winsock-only; `s_addr` works on both. |
| `src/lib/ggpo/backends/p2p.cpp` | `max(maxDif, diff)` -> a conditional; `std::exception(buf)` -> `std::runtime_error(buf)` (+ `<stdexcept>`) | A local named `max` shadows a function (MSVC's `max` is a macro); `std::exception(const char *)` is an MSVC extension. |
| `src/lib/ggpo/backends/synctest.cpp` | `BeginLog` returns, and the per-frame `Checksum ... for frame N matches.` line is not printed, unless `Platform::GetConfigBool("ggpo.log")` | Otherwise a synctest opens two log files **per frame** in the working directory — on POSIX, 1200 files named `synclogs\log-NNNN-*.log` (with the backslash) in `bin/` for the 600-frame self-test — and prints a line per frame to stdout: tens of thousands for a whole match (`--netplay-match=synctest`). A mismatch still prints and aborts. |
| `src/include/ggponet.h`, `src/lib/ggpo/main.cpp`, `sync.h`, `backends/backend.h`, `backends/p2p.*`, `backends/synctest.*` | `ggpo_get_confirmed_frame`: the sync layer's last confirmed frame (a synctest: every frame played) | A peer may end a match only on a frame nothing will roll back again (docs/NETPLAY.md §5), and upstream exposes only the current frame. |

`platform_windows.*` are unmodified and not built.

## Known upstream behaviour, left as is

- `GetConfigBool` is always false, so GGPO's own log file (`ggpo.log`) never opens —
  which matters, because `log.cpp` reuses a consumed `va_list`.
- `Peer2PeerBackend::IncrementFrame` throws a C++ exception on an internal
  invariant failure. It propagates through C frames, so it terminates the process.
