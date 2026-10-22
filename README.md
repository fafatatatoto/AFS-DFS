# AFS-Style Distributed File System

C++17 · gRPC · Protocol Buffers · Linux `inotify` · async callbacks

A client-cached distributed file system with **weak (AFS-like) consistency**, **per-file write locks**, and **bidirectional sync**. I implemented this for a graduate operating systems course at Georgia Tech.

**Source is private** (course policy). This repo is the public design write-up for recruiters and interviewers. I can walk through the code live or share the private repo on request.

## What it does

Multiple clients mount a local directory. The server holds the canonical files. After a change, every mount should converge on the same set of files and contents.

| Direction | Mechanism |
| --- | --- |
| Client → server | Linux `inotify` on the mount; local create/modify/delete becomes Store / Delete |
| Server → clients | Async long-poll callback; server replies when its file version changes |

Writes take an exclusive per-file lock. Unchanged files are skipped via CRC-32. Conflicts use last-write-wins on mtime. Fetches write `*.tmp` then `rename` so nobody reads a half file.

## Architecture

```text
  [user edits mount]                         [other clients]
         |                                          ^
         v                                          |
   inotify watcher                          reconcile cache
         |                                  (fetch / delete)
         v                                          ^
   Store / Delete RPC  ----->  [gRPC server] -------+
   (+ GetWriteLock)              file store         |
                                 lock table         |
                                 version + CV  --> async CallbackList
```

Two client threads run together: an `inotify` watcher and a callback loop. They share a mutex so a local store and an incoming invalidation do not fight over the same file.

The callback is **not** a server-push connection. The client sends `CallbackList` and keeps working. The server holds that RPC until a Store/Delete bumps a version counter, then returns the current file list (names, checksums, mtimes). The client reconciles, then issues the next callback.

## Consistency model

**Weak / AFS-style, whole-file cache.**

- Reads hit the local copy. Not every read goes to the server.
- A writer must hold the server lock for that file. One writer at a time.
- Other clients may see a stale copy until the next callback.
- After reconnect, the client diffs its mount against the server list so deleted or stale files are not served from cache.

This is a deliberate trade: fewer round trips, short windows of staleness. It is not POSIX close-to-open and not a striped HPC file system (Lustre / BeeGFS).

## Design choices

- **CRC-32 + mtime, not mtime alone.** Checksum decides “did bytes change?” (skip the transfer). Mtime decides “which copy is newer?” Clocks can skew; checksums do not tell you which side won.
- **Atomic rename.** Write to a temp file, then `rename`. Same-filesystem rename is atomic, so readers never see a partial object.
- **Idempotent lock for the holder.** If you already own the lock, Store can retry without unlocking yourself.
- **Known gap: lock leases.** A crashed client can pin a file until process teardown. Production would expire the lock on heartbeat timeout. I did not implement leases.

## How I tested it

A Python harness starts the server and several mount clients, then runs create → modify → no-op store → delete. Each replica (server + every client) is checked with CRC-32. If they do not converge before a timeout, the run fails. That is the class of bug a single-client `store` test never catches: stale cache, missed callback, lock not released.

## Limits (honest)

- One server process, userspace gRPC, insecure credentials
- Whole-file transfer, not block/stripe I/O
- No replication, no LRU cache cap, no lock lease
- Async completion-queue plumbing was course infrastructure; lock table, checksum/mtime policy, cache reconcile, and the stress harness are what I owned

## Interview (30 seconds)

> Bidirectional sync: `inotify` pushes local edits up; async long-poll callbacks pull server changes down. Whole-file cache with CRC-32 and mtime. Exclusive write locks plus atomic rename so two clients cannot publish conflicting or half-written files.

Longer notes: [docs/design.md](docs/design.md)

## Code sketches

Rewritten excerpts (not the course tree, not a drop-in solution):

- [`sketches/write_lock.hpp`](sketches/write_lock.hpp) — exclusive writer lock, idempotent for the holder
- [`sketches/reconcile.hpp`](sketches/reconcile.hpp) — CRC vs mtime decision
- [`sketches/callback_wait.hpp`](sketches/callback_wait.hpp) — version wait for async callbacks
- [`sketches/atomic_publish.hpp`](sketches/atomic_publish.hpp) — temp file + `rename`

The gRPC service, `inotify` loop, and tests stay private.

## Stack

C++17, gRPC, Protocol Buffers, Linux `inotify`, mutexes / condition variables, Python for the multi-client harness.
