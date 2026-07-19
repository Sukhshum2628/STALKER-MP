// STALKER-MP — Platform filesystem save backend (Sprint-012, Step 9)
//
// The Singular PLATFORM BOUNDARY translation unit for save/load: the ONLY place in
// the module that performs filesystem I/O (ADR-009), mirroring PlatformSockets.cpp.
// It implements the frozen Sprint-011 write seam (IPersistenceStore) — composing the
// Step-4 SaveWriter to produce bytes and writing them ATOMICALLY (temp-file +
// rename) — and the Step-8 read seam (ISaveSource). No engine headers; no
// exceptions, no RTTI, no iostream (std::filesystem via error_code overloads +
// fopen/fread/fwrite, exactly as common/FileSystemUtil.cpp).

#include "stalkermp/adapters/PlatformSaveStore.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "stalkermp/core/Error.h"
#include "stalkermp/persistence/PersistenceSnapshot.h" // persistence::PersistenceSnapshot
#include "stalkermp/saveload/SaveReader.h"              // saveload::SaveReader (enumerate metadata)
#include "stalkermp/saveload/SaveWriter.h"              // saveload::SaveWriter

// `fopen_s` is used exactly as in common/FileSystemUtil.cpp (MSVC built-in; provided
// by the build on other toolchains) — keeping OS file access exception-free.

namespace stalkermp::adapters
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* kPrefix = "save_";
        constexpr const char* kSuffix = ".smps";

        [[nodiscard]] std::string FileName(std::uint64_t saveId)
        {
            return std::string(kPrefix) + std::to_string(saveId) + kSuffix;
        }

        // Parse the saveId out of "save_<id>.smps"; returns false if it does not match.
        [[nodiscard]] bool ParseSaveId(const std::string& fileName, std::uint64_t& outId)
        {
            const std::string prefix = kPrefix;
            const std::string suffix = kSuffix;
            if (fileName.size() <= prefix.size() + suffix.size())
            {
                return false;
            }
            if (fileName.compare(0, prefix.size(), prefix) != 0)
            {
                return false;
            }
            if (fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) != 0)
            {
                return false;
            }
            const std::string digits =
                fileName.substr(prefix.size(), fileName.size() - prefix.size() - suffix.size());
            if (digits.empty())
            {
                return false;
            }
            std::uint64_t value = 0;
            for (const char c : digits)
            {
                if (c < '0' || c > '9')
                {
                    return false;
                }
                value = value * 10u + static_cast<std::uint64_t>(c - '0');
            }
            outId = value;
            return true;
        }

        [[nodiscard]] bool ReadWholeFile(const std::string& path, std::vector<std::uint8_t>& out)
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

        [[nodiscard]] bool WriteWholeFile(const std::string& path, const std::vector<std::uint8_t>& bytes)
        {
            std::FILE* file = nullptr;
            if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr)
            {
                return false;
            }
            bool ok = true;
            if (!bytes.empty())
            {
                ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
            }
            ok = (std::fflush(file) == 0) && ok;
            std::fclose(file);
            return ok;
        }

        // ---------------------------------------------------------------- Backend
        class FilesystemSaveBackend final : public ISaveBackend,
                                            public persistence::IPersistenceStore,
                                            public saveload::ISaveSource
        {
        public:
            explicit FilesystemSaveBackend(std::string directory) : m_dir(std::move(directory)) {}

            // --- ISaveBackend ----------------------------------------------------
            [[nodiscard]] persistence::IPersistenceStore& Store() noexcept override { return *this; }
            [[nodiscard]] saveload::ISaveSource& Source() noexcept override { return *this; }
            [[nodiscard]] saveload::SaveLoadOutcome Delete(const std::uint64_t saveId) override
            {
                std::error_code ec;
                fs::remove(fs::path(PathFor(saveId)), ec);
                return ec ? saveload::SaveLoadOutcome::StorageUnavailable : saveload::SaveLoadOutcome::Ok;
            }

            // --- persistence::IPersistenceStore (write) --------------------------
            [[nodiscard]] core::Expected<persistence::StoreHandle>
            Begin(const persistence::SaveMetadata& metadata) override
            {
                const persistence::StoreHandle handle{++m_nextHandle};
                m_pending.push_back(Pending{handle, metadata, {}, false});
                return handle;
            }

            [[nodiscard]] persistence::PersistenceOutcome
            Write(const persistence::StoreHandle handle, const persistence::PersistenceSnapshot& snapshot) override
            {
                Pending* p = FindPending(handle);
                if (p == nullptr)
                {
                    return persistence::PersistenceOutcome::StorageUnavailable;
                }
                p->bytes = saveload::SaveWriter::Serialize(snapshot.View(), p->metadata);
                p->written = true;
                return persistence::PersistenceOutcome::Ok;
            }

            [[nodiscard]] persistence::PersistenceOutcome Commit(const persistence::StoreHandle handle) override
            {
                Pending* p = FindPending(handle);
                if (p == nullptr)
                {
                    return persistence::PersistenceOutcome::StorageUnavailable;
                }
                if (!p->written)
                {
                    return persistence::PersistenceOutcome::IncompleteSnapshot;
                }

                // Atomic replace: write a temp file, then rename over the target. A
                // partial/interrupted write never replaces the previous save.
                const std::string finalPath = PathFor(p->metadata.saveId);
                const std::string tempPath = finalPath + ".tmp";
                if (!WriteWholeFile(tempPath, p->bytes))
                {
                    std::error_code ec;
                    fs::remove(fs::path(tempPath), ec);
                    return persistence::PersistenceOutcome::StorageUnavailable;
                }
                std::error_code ec;
                fs::rename(fs::path(tempPath), fs::path(finalPath), ec);
                if (ec)
                {
                    fs::remove(fs::path(tempPath), ec);
                    return persistence::PersistenceOutcome::StorageUnavailable;
                }
                ErasePending(handle);
                return persistence::PersistenceOutcome::Ok;
            }

            void Abort(const persistence::StoreHandle handle) noexcept override { ErasePending(handle); }

            // --- saveload::ISaveSource (read) ------------------------------------
            [[nodiscard]] std::vector<saveload::SaveDescriptor> Enumerate() const override
            {
                std::vector<saveload::SaveDescriptor> out;
                std::error_code ec;
                fs::directory_iterator it(fs::path(m_dir), ec);
                const fs::directory_iterator end;
                for (; !ec && it != end; it.increment(ec))
                {
                    const fs::path& path = it->path();
                    std::uint64_t saveId = 0;
                    if (!ParseSaveId(path.filename().string(), saveId))
                    {
                        continue; // not a save file (e.g. a .tmp)
                    }
                    std::vector<std::uint8_t> bytes;
                    if (!ReadWholeFile(path.string(), bytes))
                    {
                        continue;
                    }
                    const saveload::ParseResult parsed = saveload::SaveReader::Parse(bytes);
                    if (parsed.outcome != saveload::SaveLoadOutcome::Ok)
                    {
                        continue; // skip corrupt/unparseable files
                    }
                    saveload::SaveDescriptor d{};
                    d.saveId = saveId;
                    d.metadata = parsed.save.metadata;
                    out.push_back(d);
                }
                // Deterministic ascending order by saveId.
                for (std::size_t i = 1; i < out.size(); ++i)
                {
                    saveload::SaveDescriptor key = out[i];
                    std::size_t j = i;
                    while (j > 0 && out[j - 1].saveId > key.saveId)
                    {
                        out[j] = out[j - 1];
                        --j;
                    }
                    out[j] = key;
                }
                return out;
            }

            [[nodiscard]] core::Expected<std::vector<std::uint8_t>> Read(const std::uint64_t saveId) const override
            {
                std::vector<std::uint8_t> bytes;
                if (!ReadWholeFile(PathFor(saveId), bytes))
                {
                    return core::MakeError(core::ErrorCode::NotFound, "save not found");
                }
                return bytes;
            }

            [[nodiscard]] bool Exists(const std::uint64_t saveId) const override
            {
                std::error_code ec;
                return fs::is_regular_file(fs::path(PathFor(saveId)), ec);
            }

        private:
            struct Pending
            {
                persistence::StoreHandle handle{};
                persistence::SaveMetadata metadata{};
                std::vector<std::uint8_t> bytes;
                bool written = false;
            };

            [[nodiscard]] std::string PathFor(std::uint64_t saveId) const { return m_dir + "/" + FileName(saveId); }

            [[nodiscard]] Pending* FindPending(const persistence::StoreHandle h) noexcept
            {
                for (Pending& p : m_pending)
                {
                    if (p.handle == h)
                    {
                        return &p;
                    }
                }
                return nullptr;
            }
            void ErasePending(const persistence::StoreHandle h) noexcept
            {
                for (std::size_t i = 0; i < m_pending.size(); ++i)
                {
                    if (m_pending[i].handle == h)
                    {
                        m_pending.erase(m_pending.begin() + static_cast<std::ptrdiff_t>(i));
                        return;
                    }
                }
            }

            std::string m_dir;
            std::vector<Pending> m_pending;
            std::uint64_t m_nextHandle = 0;
        };
    } // namespace

    core::Expected<std::unique_ptr<ISaveBackend>> CreatePlatformSaveBackend(const std::string& saveDirectoryToken)
    {
        if (saveDirectoryToken.empty())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "save directory token is empty");
        }
        std::error_code ec;
        fs::create_directories(fs::path(saveDirectoryToken), ec);
        if (ec)
        {
            return core::MakeError(core::ErrorCode::IoError,
                                   "cannot create save directory '" + saveDirectoryToken + "'");
        }
        return std::unique_ptr<ISaveBackend>(std::make_unique<FilesystemSaveBackend>(saveDirectoryToken));
    }
} // namespace stalkermp::adapters
