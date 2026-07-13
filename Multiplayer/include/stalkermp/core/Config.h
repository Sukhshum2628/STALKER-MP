// STALKER-MP — Project configuration (Sprint-001, §7.4)
//
// Why this exists:
//   Runtime behavior (log verbosity today; networking, persistence and
//   server settings in later sprints) is controlled by plain-text
//   configuration files rather than compiled constants.
//
// Responsibilities:
//   - Parse INI-style files: [section], key = value, ';' / '#' comments.
//   - Typed, optional-returning accessors (no silent defaults inside the
//     store; call sites choose fallbacks explicitly).
//   - Load-or-create semantics so a fresh install produces documented
//     default files (server.cfg, client.cfg, development.cfg).
//
// Ownership / lifetime:
//   ConfigStore is a value type owned by the Bootstrap runtime. Sprint-001
//   loads configuration once during Initialize; the file format and loader
//   are designed so a future runtime-reload service can reuse them.
//
// Threading:
//   Read-only after Initialize; safe to read from any thread.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "stalkermp/core/Expected.h"

namespace stalkermp::core
{
    // Well-known configuration file names (Sprint-001 §7.4).
    namespace config_files
    {
        inline constexpr std::string_view kServer = "server.cfg";
        inline constexpr std::string_view kClient = "client.cfg";
        inline constexpr std::string_view kDevelopment = "development.cfg";
    } // namespace config_files

    class ConfigStore
    {
    public:
        // Keys are addressed by section and key name. Keys outside any
        // [section] live in the "" (global) section.
        [[nodiscard]] std::optional<std::string> GetString(std::string_view section, std::string_view key) const;
        [[nodiscard]] std::optional<std::int64_t> GetInt(std::string_view section, std::string_view key) const;
        [[nodiscard]] std::optional<double> GetDouble(std::string_view section, std::string_view key) const;

        // Accepts true/false, yes/no, on/off, 1/0 (case-insensitive).
        [[nodiscard]] std::optional<bool> GetBool(std::string_view section, std::string_view key) const;

        [[nodiscard]] bool HasSection(std::string_view section) const;
        [[nodiscard]] std::size_t KeyCount() const noexcept;

        void Set(std::string_view section, std::string_view key, std::string_view value);

    private:
        // section -> (key -> value). std::less<> enables string_view lookups.
        using KeyValueMap = std::map<std::string, std::string, std::less<>>;
        std::map<std::string, KeyValueMap, std::less<>> m_sections;
    };

    namespace config
    {
        // Configuration schema version written to generated files as
        // [meta] config_version. Loader policy (enforced by Bootstrap):
        // missing -> assumed 1 with a warning; newer -> VersionMismatch
        // error; older -> accepted (migration arrives with Sprint-012).
        // Increment only together with a documented migration step.
        inline constexpr std::int64_t kCurrentVersion = 1;

        // Parses INI-style text. Fails with ParseError on malformed lines
        // (line number included in the error message).
        [[nodiscard]] Expected<ConfigStore> ParseText(std::string_view text, std::string_view sourceName);

        // Loads a configuration file from disk.
        [[nodiscard]] Expected<ConfigStore> LoadFile(const std::string& filePath);

        // Loads a configuration file; if it does not exist, writes
        // defaultContent to disk first and parses that.
        [[nodiscard]] Expected<ConfigStore> LoadOrCreate(const std::string& filePath, std::string_view defaultContent);
    } // namespace config
} // namespace stalkermp::core
