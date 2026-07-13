#include "stalkermp/core/Config.h"

#include <cstdlib>

#include "stalkermp/common/FileSystemUtil.h"
#include "stalkermp/common/StringUtil.h"

namespace stalkermp::core
{
    // ------------------------------------------------------------ ConfigStore

    std::optional<std::string> ConfigStore::GetString(const std::string_view section, const std::string_view key) const
    {
        const auto sectionIt = m_sections.find(section);
        if (sectionIt == m_sections.end())
        {
            return std::nullopt;
        }
        const auto keyIt = sectionIt->second.find(key);
        if (keyIt == sectionIt->second.end())
        {
            return std::nullopt;
        }
        return keyIt->second;
    }

    std::optional<std::int64_t> ConfigStore::GetInt(const std::string_view section, const std::string_view key) const
    {
        const auto text = GetString(section, key);
        if (!text)
        {
            return std::nullopt;
        }
        char* end = nullptr;
        const long long value = std::strtoll(text->c_str(), &end, 10);
        if (end == text->c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(value);
    }

    std::optional<double> ConfigStore::GetDouble(const std::string_view section, const std::string_view key) const
    {
        const auto text = GetString(section, key);
        if (!text)
        {
            return std::nullopt;
        }
        char* end = nullptr;
        const double value = std::strtod(text->c_str(), &end);
        if (end == text->c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return value;
    }

    std::optional<bool> ConfigStore::GetBool(const std::string_view section, const std::string_view key) const
    {
        const auto text = GetString(section, key);
        if (!text)
        {
            return std::nullopt;
        }
        using common::EqualsIgnoreCase;
        const std::string_view value = *text;
        if (EqualsIgnoreCase(value, "true") || EqualsIgnoreCase(value, "yes") ||
            EqualsIgnoreCase(value, "on") || value == "1")
        {
            return true;
        }
        if (EqualsIgnoreCase(value, "false") || EqualsIgnoreCase(value, "no") ||
            EqualsIgnoreCase(value, "off") || value == "0")
        {
            return false;
        }
        return std::nullopt;
    }

    bool ConfigStore::HasSection(const std::string_view section) const
    {
        return m_sections.find(section) != m_sections.end();
    }

    std::size_t ConfigStore::KeyCount() const noexcept
    {
        std::size_t count = 0;
        for (const auto& [name, keys] : m_sections)
        {
            count += keys.size();
        }
        return count;
    }

    void ConfigStore::Set(const std::string_view section, const std::string_view key, const std::string_view value)
    {
        m_sections[std::string(section)][std::string(key)] = std::string(value);
    }

    // ----------------------------------------------------------------- Parser

    namespace config
    {
        Expected<ConfigStore> ParseText(const std::string_view text, const std::string_view sourceName)
        {
            ConfigStore store;
            std::string currentSection; // "" = global section.

            std::size_t lineNumber = 0;
            for (const std::string_view rawLine : common::Split(text, '\n'))
            {
                ++lineNumber;
                const std::string_view line = common::Trim(rawLine);

                if (line.empty() || line.front() == ';' || line.front() == '#')
                {
                    continue;
                }

                if (line.front() == '[')
                {
                    if (line.back() != ']' || line.size() < 3)
                    {
                        return MakeError(ErrorCode::ParseError,
                                         common::Format("{}:{}: malformed section header '{}'",
                                                        sourceName, lineNumber, line));
                    }
                    currentSection = std::string(common::Trim(line.substr(1, line.size() - 2)));
                    if (currentSection.empty())
                    {
                        return MakeError(ErrorCode::ParseError,
                                         common::Format("{}:{}: empty section name", sourceName, lineNumber));
                    }
                    continue;
                }

                const std::size_t equals = line.find('=');
                if (equals == std::string_view::npos)
                {
                    return MakeError(ErrorCode::ParseError,
                                     common::Format("{}:{}: expected 'key = value', got '{}'",
                                                    sourceName, lineNumber, line));
                }

                const std::string_view key = common::Trim(line.substr(0, equals));
                const std::string_view value = common::Trim(line.substr(equals + 1));
                if (key.empty())
                {
                    return MakeError(ErrorCode::ParseError,
                                     common::Format("{}:{}: empty key", sourceName, lineNumber));
                }

                store.Set(currentSection, key, value);
            }

            return store;
        }

        Expected<ConfigStore> LoadFile(const std::string& filePath)
        {
            auto content = common::ReadTextFile(filePath);
            if (!content)
            {
                return content.GetError();
            }
            return ParseText(content.Value(), filePath);
        }

        Expected<ConfigStore> LoadOrCreate(const std::string& filePath, const std::string_view defaultContent)
        {
            if (!common::FileExists(filePath))
            {
                if (auto written = common::WriteTextFile(filePath, defaultContent); !written)
                {
                    return written.GetError();
                }
            }
            return LoadFile(filePath);
        }
    } // namespace config
} // namespace stalkermp::core
