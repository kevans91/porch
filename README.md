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
