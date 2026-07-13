// STALKER-MP — Filesystem utilities (Sprint-001, §7.11)
//
// Thin, error-reporting wrappers over std::filesystem so call sites get
// Expected-style failures instead of exceptions or silent error codes.

#pragma once

#include <string>

#include "stalkermp/core/Expected.h"

namespace stalkermp::common
{
    // Creates the directory (and parents) if missing. Succeeds when the
    // directory already exists.
    [[nodiscard]] core::Expected<void> EnsureDirectory(const std::string& path);

    [[nodiscard]] bool FileExists(const std::string& path) noexcept;

    [[nodiscard]] core::Expected<std::string> ReadTextFile(const std::string& path);

    // Creates or truncates the file.
    [[nodiscard]] core::Expected<void> WriteTextFile(const std::string& path, std::string_view content);

    // Joins two path segments with exactly one separator.
    [[nodiscard]] std::string JoinPath(std::string_view base, std::string_view element);
} // namespace stalkermp::common
