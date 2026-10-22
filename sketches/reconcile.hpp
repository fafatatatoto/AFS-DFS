// Cache reconcile: skip the network when bytes match;
// if they differ, newer mtime wins.
// CRC answers "same content?"  Mtime answers "which copy is newer?"

#pragma once

#include <cstdint>

enum class SyncAction {
    None,       // checksums match — do not transfer
    Fetch,      // server copy is newer
    Store,      // local copy is newer
};

struct FileView {
    bool exists = false;
    std::uint32_t crc = 0;
    std::int64_t mtime = 0;
};

inline SyncAction decide(const FileView& local, const FileView& remote) {
    if (!local.exists && !remote.exists) return SyncAction::None;
    if (!local.exists) return SyncAction::Fetch;
    if (!remote.exists) return SyncAction::Store;
    if (local.crc == remote.crc) return SyncAction::None;
    return (remote.mtime >= local.mtime) ? SyncAction::Fetch : SyncAction::Store;
}
