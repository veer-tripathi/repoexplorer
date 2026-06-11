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
    static const std::array<const char*, 5> u{"B", "KB", "MB", "GB", "TB"};
    double v = double(bytes);
    std::size_t i = 0;
    while (v >= 1024.0 && i + 1 < u.size()) { v /= 1024.0; ++i; }
    char buf[64];
    if (i == 0) std::snprintf(buf, sizeof buf, "%llu %s", (unsigned long long)bytes, u[i]);
    else        std::snprintf(buf, sizeof buf, "%.1f %s", v, u[i]);
    return buf;
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
