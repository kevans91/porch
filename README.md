# orch

Orch is a program orchestration tool, inspired by expect(1) but scripted with
lua.  This utility allows scripted manipulation of programs for, e.g., testing
or automation purposes.  Orch drives spawn processes over a pts(4)
pseudo-terminal, which allows for a broader range of interactions with a program
under orchestration.

The authoritative source for this software is located at
https://git.kevans.dev/kevans/orch, but it's additionally mirrored to
[GitHub](https://github.com/kevans91/orch) for user-facing interactions.  Pull
requests and Issues are open on GitHub.

orch(1) strives to be portable.  Currently supported platforms:
 - FreeBSD
 - OpenBSD
 - NetBSD
 - macOS
 - Linux (tested on Ubuntu only)

## Notes for porting

We build on all of the above platforms.  To build and actually use orch, one
needs:

 - bmake
 - liblua + headers (orch(1) supports 5.2+)
 - a compiler
 - this source tree

The following vars will need to be set, as the defaults are likely not correct
for your system:

 - ORCHLUA_PATH (no default; we install it in /usr/local/share/orch on FreeBSD.
    This is where orch.lua will be installed)
 - LUA_INCDIR (default: /usr/local/include/lua54.  Path to directory containing
    liblua headers)
 - LUA_LIB (default: -L/usr/local/lib -llua-5.4.  All flags the link command
    will need too link orch(1))
