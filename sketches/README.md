# Illustrative sketches

These snippets are **rewritten from scratch** to show the design. They are not the course sources, not a complete file system, and will not build against the assignment skeleton.

Each file is meant to be read in two minutes:

| File | Idea |
| --- | --- |
| `write_lock.hpp` | Exclusive per-file writer lock, idempotent for the holder |
| `reconcile.hpp` | CRC-32 = did bytes change? mtime = who is newer? |
| `callback_wait.hpp` | Async long-poll: wait on a version, do not block writers |
| `atomic_publish.hpp` | Write temp file, then `rename` |

The real implementation is C++17 + gRPC and stays in a private repo.
