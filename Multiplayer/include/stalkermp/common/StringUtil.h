// STALKER-MP — String utilities (Sprint-001, §7.11)

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace stalkermp::common
{
    // Removes leading and trailing whitespace (space, tab, \r, \n).
    [[nodiscard]] std::string_view Trim(std::string_view text) noexcept;

    // Splits on a single character; empty pieces are preserved.
    [[nodiscard]] std::vector<std::string_view> Split(std::string_view text, char separator);

    [[nodiscard]] std::string ToLower(std::string_view text);

    // ASCII case-insensitive equality.
    [[nodiscard]] bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept;

    [[nodiscard]] bool StartsWith(std::string_view text, std::string_view prefix) noexcept;
    [[nodiscard]] bool EndsWith(std::string_view text, std::string_view suffix) noexcept;

    // Sequentially replaces each "{}" in format with the string form of the
    // corresponding argument. Surplus placeholders remain literal; surplus
    // arguments are ignored. Supports arithmetic types, string-likes, bool.
    namespace detail
    {
        void AppendFormatted(std::string& out, std::string_view value);
        void AppendFormatted(std::string& out, const char* value);
        void AppendFormatted(std::string& out, const std::string& value);
        void AppendFormatted(std::string& out, bool value);
        void AppendFormatted(std::string& out, char value);

        template <typename T>
        void AppendFormatted(std::string& out, const T& value)
        {
            static_assert(std::is_arithmetic_v<T>, "Format supports arithmetic and string-like arguments");
            out += std::to_string(value);
        }

        inline void FormatInto(std::string& out, std::string_view format)
        {
            out += format;
        }

        template <typename First, typename... Rest>
        void FormatInto(std::string& out, std::string_view format, const First& first, const Rest&... rest)
        {
            const std::size_t placeholder = format.find("{}");
            if (placeholder == std::string_view::npos)
            {
                out += format;
                return;
            }
            out += format.substr(0, placeholder);
            AppendFormatted(out, first);
            FormatInto(out, format.substr(placeholder + 2), rest...);
        }
    } // namespace detail

    template <typename... Args>
    [[nodiscard]] std::string Format(std::string_view format, const Args&... args)
    {
        std::string out;
        out.reserve(format.size() + sizeof...(Args) * 8);
        detail::FormatInto(out, format, args...);
        return out;
    }
} // namespace stalkermp::common
