#include "stalkermp/common/FileSystemUtil.h"

#include <filesystem>
#include <cstdio>
#include <string>

#include "stalkermp/common/StringUtil.h"

namespace stalkermp::common
{
    namespace fs = std::filesystem;

    core::Expected<void> EnsureDirectory(const std::string& path)
    {
        if (path.empty())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "EnsureDirectory: empty path");
        }

        std::error_code errorCode;
        fs::create_directories(fs::path(path), errorCode);
        if (errorCode)
        {
            return core::MakeError(core::ErrorCode::IoError,
                                   Format("EnsureDirectory: cannot create '{}': {}", path, errorCode.message()));
        }
        return core::Success();
    }

    bool FileExists(const std::string& path) noexcept
    {
        std::error_code errorCode;
        return fs::is_regular_file(fs::path(path), errorCode);
    }

    core::Expected<std::string> ReadTextFile(const std::string& path)
    {
        std::FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
        {
            return core::MakeError(core::ErrorCode::IoError,
                                   Format("ReadTextFile: cannot open '{}'", path));
        }

        if (std::fseek(file, 0, SEEK_END) != 0)
        {
            std::fclose(file);
            return core::MakeError(core::ErrorCode::IoError,
                                   Format("ReadTextFile: seek failure in '{}'", path));
        }
        long long size = std::ftell(file);
        if (size < 0)
        {
            std::fclose(file);
            return core::MakeError(core::ErrorCode::IoError,
                                   Format("ReadTextFile: tell failure in '{}'", path));
        }
        if (std::fseek(file, 0, SEEK_SET) != 0)
        {
            std::fclose(file);
            return core::MakeError(core::ErrorCode::IoError,
                                   Format("ReadTextFile: seek failure in '{}'", path));
        }

        std::string content;
        if (size > 0)
        {
            content.resize(static_cast<size_t>(size));
            size_t readBytes = std::fread(&content[0], 1, static_cast<size_t>(size), file);
            if (readBytes != static_cast<size_t>(size))
            {
                std::fclose(file);
                return core::MakeError(core::ErrorCode::IoError,
                                       Format("ReadTextFile: read failure in '{}'", path));
            }
        }

        std::fclose(file);
        return content;
    }

    core::Expected<void> WriteTextFile(const std::string& path, const std::string_view content)
    {
        std::FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "wb") != 0 || !file)
        {
            return core::MakeError(core::ErrorCode::IoError,
                                   Format("WriteTextFile: cannot open '{}'", path));
        }

        if (!content.empty())
        {
            size_t written = std::fwrite(content.data(), 1, content.size(), file);
            if (written != content.size())
            {
                std::fclose(file);
                return core::MakeError(core::ErrorCode::IoError,
                                       Format("WriteTextFile: write failure in '{}'", path));
            }
        }

        if (std::fflush(file) != 0)
        {
            std::fclose(file);
            return core::MakeError(core::ErrorCode::IoError,
                                   Format("WriteTextFile: flush failure in '{}'", path));
        }

        std::fclose(file);
        return core::Success();
    }

    std::string JoinPath(const std::string_view base, const std::string_view element)
    {
        if (base.empty())
        {
            return std::string(element);
        }
        std::string joined(base);
        const char last = joined.back();
        if (last != '/' && last != '\\')
        {
            joined += static_cast<char>(fs::path::preferred_separator);
        }
        joined += element;
        return joined;
    }
} // namespace stalkermp::common
