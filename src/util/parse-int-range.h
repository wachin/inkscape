// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Parse a string containing number ranges.
 *
 * Copyright (C) 2022 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_UTIL_PARSE_INT_RANGE_H
#define INKSCAPE_UTIL_PARSE_INT_RANGE_H

#include <regex>
#include <string>
#include <set>

namespace Inkscape {

/**
 * Parse integer ranges out of a string using regex.
 *
 * @param input - A string containing number ranges that can either be comma
 *                separated or dash separated for non and continuous ranges.
 * @param start - Optional first number in the acceptable range.
 * @param end - The last number in the acceptable range.
 *
 * @returns a sorted set of unique numbers.
 */
inline std::set<unsigned int> parseIntRange(const std::string &input, unsigned int start=1, unsigned int end=0)
{
    // Special word based translations go here:
    if (input == "all") {
        return parseIntRange("-", start, end);
    }

    std::set<unsigned int> out;
    auto add = [=](std::set<unsigned int> &to, unsigned int val) {
        if (start <= val && (!end || val <= end))
            to.insert(val);
    };

    std::regex re("((\\d+|)\\s?(-)\\s?(\\d+|)|,?(\\d+)([^-]|$))");

    std::string::const_iterator sit = input.cbegin(), fit = input.cend();
    for (std::smatch match; std::regex_search(sit, fit, match, re); sit = match[0].second)
    {
        if (match.str(3) == "") {
            add(out, std::stoul(match.str(5)));
        } else {
            auto r1 = match.str(2).empty() ? start : std::stoul(match.str(2));
            auto r2 = match.str(4).empty() ? (end ? end : r1) : std::stoul(match.str(4));
            for (auto i = std::min(r1, r2); i <= std::max(r1, r2); i++) {
                add(out, i);
            }
        }
    }

   return out;
}

} // namespace Inkscape

#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
