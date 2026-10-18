#pragma once
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace evr
{
inline std::string jsonString(const std::string &in)
{
    std::ostringstream s;
    s << '"';
    for (unsigned char c : in)
    {
        switch (c)
        {
        case '\\':
            s << "\\\\";
            break;
        case '"':
            s << "\\\"";
            break;
        case '\n':
            s << "\\n";
            break;
        case '\r':
            s << "\\r";
            break;
        case '\t':
            s << "\\t";
            break;
        default:
            if (c >= 32)
                s << c;
            else
                s << '?';
        }
    }
    s << '"';
    return s.str();
}
class Log
{
    std::ofstream file;
    std::mutex lock;
    bool debug;
    // Opt-in performance measurement (EVR_LOG_BUFFERED=1 in the managed child process):
    // flush every 512 events and on destruction instead of after every event. A crash can
    // then lose the unflushed tail, so evidence runs keep the default per-event flush.
    bool buffered = false;
    unsigned pending = 0;
    long long lastFlush = 0;
    // Play-session cap (2026-10-05): once enabled, each event name is written at most `cap`
    // more times; later occurrences are only counted. Terminal/failure events and every
    // "session_*" event are always written, so the log stays bounded for unlimited play.
    unsigned cap = 0;
    unsigned long long suppressedTotal = 0;
    std::unordered_map<std::string, std::pair<unsigned, unsigned long long>> capped; // name -> written, suppressed

  public:
    explicit Log(const std::string &path, bool detailed = false) : file(path, std::ios::app), debug(detailed)
    {
        if (!file)
            throw std::runtime_error("cannot open log: " + path);
        const char *mode = std::getenv("EVR_LOG_BUFFERED");
        buffered = mode && mode[0] == '1' && !mode[1];
    }
    ~Log()
    {
        std::lock_guard<std::mutex> l(lock);
        file.flush();
    }
    bool bufferedMode() const { return buffered; }
    static bool terminalName(const std::string &name)
    {
        return name.find("removed") != std::string::npos || name.find("fail") != std::string::npos ||
               name.find("closed") != std::string::npos || name.find("hook_complete") != std::string::npos ||
               name.find("drained") != std::string::npos || name.find("restored") != std::string::npos;
    }
    // The HUD's fade decisions have their own limits (layer/native_xr.hpp; 2026-10-18: the name cap had stopped them before
    // the headset's report 12).
    static bool alwaysWritten(const std::string &name) { return terminalName(name) || name.rfind("session_", 0) == 0 || name == "native_ui_hud" || name == "native_ui_hud_kept"; }
    // Enable (or re-arm) the per-name cap; counting starts now. 0 disables it.
    void setNameCap(unsigned value)
    {
        std::lock_guard<std::mutex> l(lock);
        cap = value;
        capped.clear();
    }
    unsigned nameCap()
    {
        std::lock_guard<std::mutex> l(lock);
        return cap;
    }
    unsigned long long suppressed()
    {
        std::lock_guard<std::mutex> l(lock);
        return suppressedTotal;
    }
    // JSON fields: "suppressed_total":N,"suppressed_names":K,"suppressed_top":[["name",count],...]
    std::string suppressedSummary(size_t top = 16)
    {
        std::vector<std::pair<std::string, unsigned long long>> v;
        unsigned long long total;
        {
            std::lock_guard<std::mutex> l(lock);
            total = suppressedTotal;
            for (const auto &e : capped)
                if (e.second.second)
                    v.emplace_back(e.first, e.second.second);
        }
        std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
        std::ostringstream s;
        s << "\"suppressed_total\":" << total << ",\"suppressed_names\":" << v.size() << ",\"suppressed_top\":[";
        for (size_t i = 0; i < v.size() && i < top; ++i)
            s << (i ? "," : "") << '[' << jsonString(v[i].first) << ',' << v[i].second << ']';
        s << ']';
        return s.str();
    }
    void event(const std::string &name, const std::string &fields = "", bool detail = false)
    {
        if (detail && !debug)
            return;
        std::lock_guard<std::mutex> l(lock);
        if (cap && !alwaysWritten(name))
        {
            auto &c = capped[name];
            if (c.first >= cap)
            {
                ++c.second;
                ++suppressedTotal;
                return;
            }
            ++c.first;
        }
        auto t =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        file << "{\"us\":" << t << ",\"event\":" << jsonString(name);
        if (!fields.empty())
            file << ',' << fields;
        file << "}\n";
        // Terminal events are flushed at once: probe logs are not always destroyed before
        // the runner reads them, so hook removal/completion must never sit in the buffer.
        const bool terminal = terminalName(name);
        if (!buffered || terminal || ++pending >= 512 || t - lastFlush >= 100000)
        {
            file.flush();
            pending = 0;
            lastFlush = t;
        }
    }
};
} // namespace evr
