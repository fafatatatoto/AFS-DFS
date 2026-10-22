// Publish a complete file or nothing.
// Same-directory rename is atomic on Linux: readers never see a truncated object.

#pragma once

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

inline void publish_atomically(const fs::path& dest, const std::string& bytes) {
    fs::path tmp = dest;
    tmp += ".tmp";

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("open temp file failed");
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) throw std::runtime_error("write temp file failed");
    }

    fs::rename(tmp, dest);  // atomic replace on the same filesystem
}
