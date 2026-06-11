#pragma once
// Small helpers: string ops, ANSI color, ASCII bars. No project dependencies.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace re {

std::string_view trim(std::string_view s);
std::string toLower(std::string_view s);
std::vector<std::string> split(std::string_view s, char delim);
bool endsWith(std::string_view s, std::string_view suffix);
bool startsWith(std::string_view s, std::string_view prefix);
std::string humanSize(std::uintmax_t bytes);

// ANSI color. Globally toggle with colorEnabled(false) for --no-color/pipes.
enum class Color { Reset, Bold, Red, Green, Blue, Cyan, Gray };
void colorEnabled(bool on);
std::string paint(std::string_view text, Color c);

// ASCII bar like "[#####   ]", 'width' cells, 'ratio' in [0,1].
std::string bar(double ratio, int width = 20);

} // namespace re
