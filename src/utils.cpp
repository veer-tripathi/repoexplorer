#include "utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>

namespace re {

std::string_view trim(std::string_view s) {
    auto sp = [](unsigned char c) { return !std::isspace(c); };
    auto b = std::find_if(s.begin(), s.end(), sp);
    auto e = std::find_if(s.rbegin(), s.rend(), sp).base();
    if (b >= e) return {};
    return s.substr(b - s.begin(), e - b);
}

std::string toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return out;
}

std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= s.size()) {
        std::size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) { parts.emplace_back(s.substr(start)); break; }
        parts.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string humanSize(std::uintmax_t bytes) {
    // Available units
    static const std::array<const char*, 5> units = {
        "B", "KB", "MB", "GB", "TB"
    };

    // Start with the size in bytes
    double size = static_cast<double>(bytes);
    std::size_t unitIndex = 0;

    // Keep converting to the next larger unit
    // until the value becomes less than 1024
    while (size >= 1024.0 && unitIndex + 1 < units.size()) {
        size /= 1024.0;
        ++unitIndex;
    }

    char buffer[64];

    if (unitIndex == 0) {
        // For bytes, show an integer value
        // Example: "512 B"
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%llu %s",
            static_cast<unsigned long long>(bytes),
            units[unitIndex]
        );
    } else {
        // For KB, MB, GB, etc., show one decimal place
        // Example: "1.5 KB", "23.7 MB"
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%.1f %s",
            size,
            units[unitIndex]
        );
    }

    return std::string(buffer);
}

static bool g_color = true;
void colorEnabled(bool on) { g_color = on; }

std::string paint(std::string_view text, Color c) {
    if (!g_color) return std::string(text);
    const char* seq = "";
    switch (c) {
        case Color::Reset: seq = "\033[0m";  break;
        case Color::Bold:  seq = "\033[1m";  break;
        case Color::Red:   seq = "\033[31m"; break;
        case Color::Green: seq = "\033[32m"; break;
        case Color::Blue:  seq = "\033[34m"; break;
        case Color::Cyan:  seq = "\033[36m"; break;
        case Color::Gray:  seq = "\033[90m"; break;
    }
    return std::string(seq) + std::string(text) + "\033[0m";
}

std::string bar(double ratio, int width) {
    ratio = std::clamp(ratio, 0.0, 1.0);
    if (width < 1) width = 1;
    int fill = int(ratio * width + 0.5);
    return "[" + std::string(fill, '#') + std::string(width - fill, ' ') + "]";
}

} // namespace re
