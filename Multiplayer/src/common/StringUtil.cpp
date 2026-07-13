#include "stalkermp/common/StringUtil.h"

#include <cctype>

namespace stalkermp::common
{
    namespace
    {
        [[nodiscard]] bool IsWhitespace(const char c) noexcept
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        [[nodiscard]] char ToLowerAscii(const char c) noexcept
        {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        }
    } // namespace

    std::string_view Trim(std::string_view text) noexcept
    {
        while (!text.empty() && IsWhitespace(text.front()))
        {
            text.remove_prefix(1);
        }
        while (!text.empty() && IsWhitespace(text.back()))
        {
            text.remove_suffix(1);
        }
        return text;
    }

    std::vector<std::string_view> Split(const std::string_view text, const char separator)
    {
        std::vector<std::string_view> pieces;
        std::size_t begin = 0;
        while (true)
        {
            const std::size_t end = text.find(separator, begin);
            if (end == std::string_view::npos)
            {
                pieces.push_back(text.substr(begin));
                return pieces;
            }
            pieces.push_back(text.substr(begin, end - begin));
            begin = end + 1;
        }
    }

    std::string ToLower(const std::string_view text)
    {
        std::string result;
        result.reserve(text.size());
        for (const char c : text)
        {
            result += ToLowerAscii(c);
        }
        return result;
    }

    bool EqualsIgnoreCase(const std::string_view a, const std::string_view b) noexcept
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (ToLowerAscii(a[i]) != ToLowerAscii(b[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool StartsWith(const std::string_view text, const std::string_view prefix) noexcept
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    bool EndsWith(const std::string_view text, const std::string_view suffix) noexcept
    {
        return text.size() >= suffix.size() &&
               text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    namespace detail
    {
        void AppendFormatted(std::string& out, const std::string_view value) { out += value; }
        void AppendFormatted(std::string& out, const char* value) { out += value != nullptr ? value : "(null)"; }
        void AppendFormatted(std::string& out, const std::string& value) { out += value; }
        void AppendFormatted(std::string& out, const bool value) { out += value ? "true" : "false"; }
        void AppendFormatted(std::string& out, const char value) { out += value; }
    } // namespace detail
} // namespace stalkermp::common
