#pragma once
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
namespace evr
{
struct Config
{
    bool enabled = false, debug = false, hideLocalHead = true, headAim = true, hmdRelativeMovement = true;
    float renderScale = 1, nearClip = .05f, farClip = 1000, worldUnitsPerMeter = 1;
    static Config read(const std::string &path)
    {
        std::ifstream in(path);
        if (!in)
            throw std::runtime_error("configuration file unavailable");
        Config c;
        std::map<std::string, bool> seen;
        std::string line;
        auto trim = [](std::string s) {
            auto a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string{} : s.substr(a, b - a + 1);
        };
        auto boolean = [](const std::string &s) {
            if (s == "true")
                return true;
            if (s == "false")
                return false;
            throw std::invalid_argument("boolean must be true or false");
        };
        auto number = [](const std::string &s) {
            size_t n;
            float v = std::stof(s, &n);
            if (n != s.size() || !std::isfinite(v) || v <= 0)
                throw std::invalid_argument("invalid positive configuration number");
            return v;
        };
        while (std::getline(in, line))
        {
            line = trim(line);
            if (line.empty() || line[0] == '#')
                continue;
            auto eq = line.find('=');
            if (eq == std::string::npos)
                throw std::invalid_argument("configuration requires key=value");
            auto key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
            if (seen[key])
                throw std::invalid_argument("duplicate config key " + key);
            seen[key] = true;
            if (key == "Enabled")
                c.enabled = boolean(value);
            else if (key == "DebugMode")
                c.debug = boolean(value);
            else if (key == "HideLocalHead")
                c.hideLocalHead = boolean(value);
            else if (key == "HeadAim")
                c.headAim = boolean(value);
            else if (key == "HmdRelativeMovement")
                c.hmdRelativeMovement = boolean(value);
            else if (key == "RenderScale")
                c.renderScale = number(value);
            else if (key == "NearClipMeters")
                c.nearClip = number(value);
            else if (key == "FarClipMeters")
                c.farClip = number(value);
            else if (key == "WorldUnitsPerMeter")
                c.worldUnitsPerMeter = number(value);
            else
                throw std::invalid_argument("unknown config key " + key);
        }
        if (c.farClip <= c.nearClip)
            throw std::invalid_argument("invalid clip interval");
        return c;
    }
};
} // namespace evr
