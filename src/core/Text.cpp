/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <charconv>
#include <ranges>

#include "core/Env.h"
#include "core/Text.h"

namespace {

constexpr std::string_view WHITESPACE = " \t\r\n\f\v";

char lowerChar(const char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

char upperChar(const char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value - 'a' + 'A') : value;
}

bool isDigit(const char value) {
    return value >= '0' && value <= '9';
}

// Underscore counts as one as well.
bool isAlphanumeric(const char value) {
    return isDigit(value) || value == '_'
        || (value >= 'a' && value <= 'z')
        || (value >= 'A' && value <= 'Z');
}

}

namespace Text {

std::string lower(const std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), lowerChar);

    return out;
}

std::string upper(const std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), upperChar);

    return out;
}

std::string trim(const std::string_view value) {
    const size_t first = value.find_first_not_of(WHITESPACE);

    if (first == std::string_view::npos) {
        return {};
    }

    const size_t last = value.find_last_not_of(WHITESPACE);

    return std::string(value.substr(first, last - first + 1));
}

bool iequals(const std::string_view left, const std::string_view right) {
    return left.size() == right.size()
        && std::ranges::equal(left, right, [](const char one, const char two) {
               return lowerChar(one) == lowerChar(two);
           });
}

bool iendsWith(const std::string_view value, const std::string_view suffix) {
    return value.size() >= suffix.size()
        && iequals(value.substr(value.size() - suffix.size()), suffix);
}

int toInt(const std::string_view value, const int def) {
    const std::string trimmed = trim(value);
    int out = 0;

    const char *begin = trimmed.data();
    const char *end = begin + trimmed.size();
    const auto [stopped, code] = std::from_chars(begin, end, out);

    return code == std::errc() && stopped == end ? out : def;
}

bool isInt(const std::string_view value) {
    const std::string trimmed = trim(value);

    if (trimmed.empty()) {
        return false;
    }

    int out = 0;
    const char *begin = trimmed.data();
    const char *end = begin + trimmed.size();
    const auto [stopped, code] = std::from_chars(begin, end, out);

    return code == std::errc() && stopped == end;
}

std::vector<std::string> split(const std::string_view value, const char separator) {
    return value | std::views::split(separator)
        | std::ranges::to<std::vector<std::string>>();
}

std::string join(const std::vector<std::string> &parts, const std::string_view separator) {
    return parts | std::views::join_with(separator) | std::ranges::to<std::string>();
}

bool naturalLess(const std::string_view left, const std::string_view right) {
    size_t li = 0;
    size_t ri = 0;

    while (li < left.size() && ri < right.size()) {
        const bool leftDigit = isDigit(left[li]);
        const bool rightDigit = isDigit(right[ri]);

        // A run of digits is worth more than the characters it is spelled with.
        if (leftDigit && rightDigit) {
            /*
            To prevent overflow at most nine digits are read, ignoring leading
            zeroes. That is enough for WAD map names, which are limited to
            eight characters in the first place.
            */
            unsigned int leftValue = 0;
            unsigned int rightValue = 0;
            unsigned int leftDigits = 0;
            unsigned int rightDigits = 0;

            while (li < left.size() && isDigit(left[li]) && leftDigits < 9) {
                leftValue = (leftValue * 10) + static_cast<unsigned int>(left[li] - '0');

                if (leftValue != 0) {
                    leftDigits++;
                }

                ++li;
            }

            while (ri < right.size() && isDigit(right[ri]) && rightDigits < 9) {
                rightValue = (rightValue * 10) + static_cast<unsigned int>(right[ri] - '0');

                if (rightValue != 0) {
                    rightDigits++;
                }

                ++ri;
            }

            if (leftValue != rightValue) {
                return leftValue < rightValue;
            }

            continue;
        }

        // One of them being a number is itself the answer.
        if (leftDigit) {
            return true;
        }

        if (rightDigit) {
            return false;
        }

        if (left[li] != right[ri]) {
            return left[li] < right[ri];
        }

        ++li;
        ++ri;
    }

    // One of them ran out; the shorter is the lesser.
    return ri < right.size();
}

std::vector<std::string> parseArguments(const std::string_view line) {
    std::vector<std::string> arguments;
    std::string current;
    bool started = false;
    size_t index = 0;

    const auto expand = [&current](const std::string_view name) {
        if (name.empty()) {
            current.push_back('$');

            return;
        }

        current.append(Env::get(std::string(name).c_str()));
    };

    while (index < line.size()) {
        const char letter = line[index];

        if (letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r') {
            if (started) {
                arguments.push_back(current);
                current.clear();
                started = false;
            }

            ++index;

            continue;
        }

        started = true;

        if (letter == '\'') {
            // Nothing at all happens inside single quotes.
            ++index;

            while (index < line.size() && line[index] != '\'') {
                current.push_back(line[index]);
                ++index;
            }

            if (index < line.size()) {
                ++index;
            }

            continue;
        }

        if (letter == '"') {
            ++index;

            while (index < line.size() && line[index] != '"') {
                if (line[index] == '\\' && index + 1 < line.size()) {
                    ++index;
                    current.push_back(line[index]);
                    ++index;

                    continue;
                }

                if (line[index] == '$') {
                    ++index;
                    size_t start = index;

                    if (index < line.size() && line[index] == '{') {
                        start = ++index;

                        while (index < line.size() && line[index] != '}') {
                            ++index;
                        }

                        expand(line.substr(start, index - start));

                        if (index < line.size()) {
                            ++index;
                        }
                    } else {
                        while (index < line.size() && isAlphanumeric(line[index])) {
                            ++index;
                        }

                        expand(line.substr(start, index - start));
                    }

                    continue;
                }

                current.push_back(line[index]);
                ++index;
            }

            if (index < line.size()) {
                ++index;
            }

            continue;
        }

        if (letter == '\\' && index + 1 < line.size()) {
            ++index;
            current.push_back(line[index]);
            ++index;

            continue;
        }

        if (letter == '$') {
            ++index;
            size_t start = index;

            if (index < line.size() && line[index] == '{') {
                start = ++index;

                while (index < line.size() && line[index] != '}') {
                    ++index;
                }

                expand(line.substr(start, index - start));

                if (index < line.size()) {
                    ++index;
                }
            } else {
                while (index < line.size() && isAlphanumeric(line[index])) {
                    ++index;
                }

                expand(line.substr(start, index - start));
            }

            continue;
        }

        current.push_back(letter);
        ++index;
    }

    if (started) {
        arguments.push_back(current);
    }

    return arguments;
}

std::string quoteArgument(const std::string_view argument) {
    if (!argument.empty() && argument.find_first_of(" \t\n\"'\\$") == std::string_view::npos) {
        return std::string(argument);
    }

    std::string quoted = "\"";
    size_t slashes = 0;

    /*
    A backslash only means anything where it runs into a quote, and doubling
    the rest breaks the Windows paths that are full of them: what a DOS program
    is told to open is what it looks for, character for character.
    */
    for (const char letter : argument) {
        if (letter == '\\') {
            slashes++;
            quoted.push_back(letter);
            continue;
        }

        if (letter == '"') {
            quoted.append(slashes + 1, '\\');
        }

        slashes = 0;
        quoted.push_back(letter);
    }

    // These would otherwise escape the quote that closes the argument.
    quoted.append(slashes, '\\');
    quoted.push_back('"');

    return quoted;
}

}
