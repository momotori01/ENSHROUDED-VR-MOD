#pragma once
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace evr
{
struct PatternByte
{
    uint8_t value;
    bool wildcard;
};
inline std::vector<PatternByte> parsePattern(const std::string &text)
{
    std::istringstream s(text);
    std::string token;
    std::vector<PatternByte> pattern;
    bool fixed = false;
    while (s >> token)
    {
        if (token == "?" || token == "??")
            pattern.push_back({0, true});
        else
        {
            if (token.size() != 2 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
                throw std::invalid_argument("malformed signature");
            pattern.push_back({uint8_t(std::stoul(token, nullptr, 16)), false});
            fixed = true;
        }
    }
    if (pattern.empty() || !fixed)
        throw std::invalid_argument("unconstrained signature");
    return pattern;
}
inline std::vector<size_t> scan(const uint8_t *data, size_t size, const std::vector<PatternByte> &pattern)
{
    std::vector<size_t> results;
    if (pattern.empty() || pattern.size() > size)
        return results;
    for (size_t i = 0; i <= size - pattern.size(); i++)
    {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); j++)
            if (!pattern[j].wildcard && data[i + j] != pattern[j].value)
            {
                match = false;
                break;
            }
        if (match)
            results.push_back(i);
    }
    return results;
}
inline size_t uniqueMatch(const std::vector<size_t> &matches)
{
    if (matches.size() != 1)
        throw std::runtime_error("signature missing or ambiguous; hook disabled");
    return matches[0];
}
} // namespace evr
