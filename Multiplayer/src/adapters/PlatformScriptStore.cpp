// STALKER-MP — Platform filesystem script source (Sprint-013, Step 8)
//
// The Singular PLATFORM BOUNDARY translation unit for Lua scripting: the ONLY script-
// side place in the module that performs filesystem I/O (ADR-009), mirroring
// PlatformSaveStore.cpp / PlatformSockets.cpp. It implements the Step-7 read seam
// (lua::IScriptSource) over a directory of `.lua` files. No engine headers, no VM; no
// exceptions, no RTTI, no iostream (std::filesystem via error_code overloads +
// fopen/fread, exactly as common/FileSystemUtil.cpp).
//
// Script ids are derived deterministically from the file name (FNV-1a 64-bit), so a
// given file always maps to the same id and enumeration is stable/ascending. The
// backend is read-only: it never creates, writes, or mutates the directory.

#include "stalkermp/adapters/PlatformScriptStore.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "stalkermp/core/Error.h"

// `fopen_s` is used exactly as in common/FileSystemUtil.cpp (MSVC built-in; provided
// by the build on other toolchains) — keeping OS file access exception-free.

namespace stalkermp::adapters
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* kScriptExtension = ".lua";

        // Deterministic 64-bit FNV-1a over the file name (0 reserved for "none").
        [[nodiscard]] std::uint64_t DeriveScriptId(const std::string& fileName) noexcept
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const char c : fileName)
            {
                hash ^= static_cast<std::uint8_t>(c);
                hash *= 1099511628211ull;
            }
            return hash == 0 ? 1ull : hash;
        }

        [[nodiscard]] bool ReadWholeFile(const std::string& path, std::vector<std::byte>& out)
        {
            std::FILE* file = nullptr;
            if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr)
            {
                return false;
            }
            bool ok = std::fseek(file, 0, SEEK_END) == 0;
            long long size = ok ? std::ftell(file) : -1;
            ok = ok && size >= 0 && std::fseek(file, 0, SEEK_SET) == 0;
            if (ok && size > 0)
            {
                out.resize(static_cast<std::size_t>(size));
                ok = std::fread(out.data(), 1, static_cast<std::size_t>(size), file) ==
                     static_cast<std::size_t>(size);
            }
            else if (ok)
            {
                out.clear();
            }
            std::fclose(file);
            return ok;
        }

        // Read-only IScriptSource over a directory of `.lua` files.
        class FilesystemScriptSource final : public lua::IScriptSource
        {
        public:
            explicit FilesystemScriptSource(std::string directory) : m_dir(std::move(directory)) {}

            [[nodiscard]] std::vector<lua::ScriptDescriptor> Enumerate() const override
            {
                std::vector<lua::ScriptDescriptor> out;
                std::error_code ec;
                fs::directory_iterator it(fs::path(m_dir), ec);
                const fs::directory_iterator end;
                for (; !ec && it != end; it.increment(ec))
                {
                    const fs::path& path = it->path();
                    if (path.extension() != kScriptExtension)
                    {
                        continue; // not a script file
                    }
                    lua::ScriptDescriptor d{};
                    d.id = lua::ScriptId{DeriveScriptId(path.filename().string())};
                    d.apiVersion = 0; // unknown at the file layer; resolved by later steps
                    d.generation = 0;
                    out.push_back(d);
                }
                // Deterministic ascending order by derived id.
                std::sort(out.begin(), out.end(),
                          [](const lua::ScriptDescriptor& a, const lua::ScriptDescriptor& b) { return a.id < b.id; });
                return out;
            }

            [[nodiscard]] core::Expected<std::vector<std::byte>> Read(const lua::ScriptId id) const override
            {
                std::string path;
                if (!FindPathFor(id, path))
                {
                    return core::MakeError(core::ErrorCode::NotFound, "script not found");
                }
                std::vector<std::byte> bytes;
                if (!ReadWholeFile(path, bytes))
                {
                    return core::MakeError(core::ErrorCode::IoError, "script could not be read");
                }
                return bytes;
            }

            [[nodiscard]] bool Exists(const lua::ScriptId id) const override
            {
                std::string path;
                return FindPathFor(id, path);
            }

        private:
            // Locate the `.lua` file whose derived id matches `id`.
            [[nodiscard]] bool FindPathFor(const lua::ScriptId id, std::string& outPath) const
            {
                std::error_code ec;
                fs::directory_iterator it(fs::path(m_dir), ec);
                const fs::directory_iterator end;
                for (; !ec && it != end; it.increment(ec))
                {
                    const fs::path& path = it->path();
                    if (path.extension() != kScriptExtension)
                    {
                        continue;
                    }
                    if (lua::ScriptId{DeriveScriptId(path.filename().string())} == id)
                    {
                        outPath = path.string();
                        return true;
                    }
                }
                return false;
            }

            std::string m_dir;
        };
    } // namespace

    std::unique_ptr<lua::IScriptSource> CreatePlatformScriptSource(const std::string& scriptDirectoryToken)
    {
        // Read-only: a missing/empty directory simply enumerates nothing (the iterator
        // fails with an error_code and yields no entries). Never throws.
        return std::unique_ptr<lua::IScriptSource>(
            std::make_unique<FilesystemScriptSource>(scriptDirectoryToken));
    }
} // namespace stalkermp::adapters
