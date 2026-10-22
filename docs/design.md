# Design notes

## 1. Bidirectional synchronization

**Client → server.** A watcher thread uses Linux `inotify` on the mount. Create, modify, and delete in that folder become Store or Delete RPCs. `inotify` can drop events under burst, so explicit Store and a full-list callback are backups, not optional extras.

**Server → clients.** Each client keeps an outstanding async `CallbackList` RPC (long poll). The client does not block its watcher while waiting. On the server, a version counter and condition variable wake waiters after Store or Delete. The reply is the full file list with checksums and mtimes—not a single dirty name—so a client that missed events can still converge.

On mount and on callback reconnect, the client reconciles:

- On server, missing locally → fetch
- Local, missing on server → store (if we still have it) or delete local if the server already dropped it
- Both sides, different CRC → newer mtime wins
- Same CRC → no transfer

That last path is the no-op Store case: `ALREADY_EXISTS` / skip.

## 2. Cache consistency

Each client’s mount **is** the cache. Whole files only.

| Signal | Job |
| --- | --- |
| CRC-32 | Content equal? If yes, skip bytes. |
| mtime | If content differs, which copy is newer? |
| server file list | If a name disappeared, drop the local copy. |

Partial-file cache and block layout were out of scope. The cost is: a one-byte edit still ships the whole file. Fine for the assignment; wrong for multi-GB checkpoints.

## 3. Concurrent updates

**Write lock.** Before Store or Delete, `GetWriteLock(filename)`. The server maps filename → holder. A second writer fails. The current holder asking again is OK (idempotent). Release after a successful store, or on delete.

**Atomic publish.** Server and client write `name.tmp` then `rename` to `name`. Crash during stream does not leave a truncated canonical file.

**Thread safety on the client.** Watcher thread and callback thread both touch the mount. A mutex serializes “I am storing this name” vs “callback told me to fetch/delete this name,” so a callback-driven write does not re-enter `inotify` → Store in a loop. Comparing checksums before Store is the other half of loop prevention.

**Deadlock rule.** At most one file lock at a time. Callback waiters block on “version changed,” not on the per-file write lock, so a held write lock does not freeze notifications.

## Failure modes I would add next

1. **Lock lease + heartbeat** — crashed holder cannot pin a file forever.
2. **Callback liveness** — if the long-poll dies silently, cache never invalidates; detect and re-register.
3. **LRU eviction** — unbounded mount directories.
4. **Authenticated RPC and a real server restart story** for the lock table.

## What I did vs what the course provided

Provided: gRPC service runner, async completion-queue templates, `inotify` watcher scaffolding, CRC helper.

I implemented: lock table, Store/Fetch/Delete/List/Stat behavior under the cache model, callback-side reconcile, watcher coordination, and a multi-client checksum stress harness.
