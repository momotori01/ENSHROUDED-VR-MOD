#include <evr/log.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace evr;
// Play-session log cap: per-name write limit after enabling, terminal/failure/session events
// always written, suppressed counts reported, default (cap 0) unchanged.
static std::vector<std::string> lines(const std::string &path)
{
    std::ifstream f(path);
    std::vector<std::string> out;
    for (std::string l; std::getline(f, l);)
        out.push_back(l);
    return out;
}
static std::map<std::string, unsigned> names(const std::vector<std::string> &rows)
{
    std::map<std::string, unsigned> out;
    for (const auto &l : rows)
    {
        const auto a = l.find("\"event\":\"") + 9;
        out[l.substr(a, l.find('"', a) - a)]++;
    }
    return out;
}
int main()
{
    unsigned n = 0;
    try
    {
        auto check = [&](bool v, const char *what) { ++n; if (!v) throw std::runtime_error(what); };
        const auto dir = std::filesystem::temp_directory_path() / "evr_log_cap_tests";
        std::filesystem::create_directories(dir);
        const auto path = (dir / "cap.jsonl").string();
        std::filesystem::remove(path);
        {
            Log log(path);
            check(log.nameCap() == 0, "cap disabled by default");
            for (int i = 0; i < 100; ++i)
                log.event("frame_sample", "\"i\":" + std::to_string(i));
            check(log.suppressed() == 0, "no suppression without a cap");
            log.setNameCap(5);
            for (int i = 0; i < 100; ++i)
                log.event("frame_sample", "\"i\":" + std::to_string(i));
            for (int i = 0; i < 3; ++i)
                log.event("rare_lifecycle");
            for (int i = 0; i < 40; ++i)
            {
                log.event("hook_removed");                 // terminal: always
                log.event("native_xr_failure", "\"k\":1"); // failure: always
                log.event("session_heartbeat");            // session_*: always
                log.event("other_sample");
            }
            check(log.suppressed() == 95 + 35, "suppressed total counts only capped names");
            const auto summary = log.suppressedSummary(1);
            check(summary.find("\"suppressed_total\":130") != std::string::npos, "summary total");
            check(summary.find("\"suppressed_names\":2") != std::string::npos, "summary distinct names");
            check(summary.find("[[\"frame_sample\",95]]") != std::string::npos, "summary top sorted by count");
            log.event("session_end", summary);
            log.setNameCap(2); // re-arm: counting restarts
            for (int i = 0; i < 5; ++i)
                log.event("frame_sample");
            log.setNameCap(0);
            for (int i = 0; i < 4; ++i)
                log.event("frame_sample");
        }
        const auto rows = lines(path);
        const auto c = names(rows);
        check(c.at("frame_sample") == 100 + 5 + 2 + 4, "pre-cap all, capped 5, re-armed 2, disabled all");
        check(c.at("rare_lifecycle") == 3 && c.at("other_sample") == 5, "per-name independent caps");
        check(c.at("hook_removed") == 40 && c.at("native_xr_failure") == 40 && c.at("session_heartbeat") == 40, "terminal/failure/session always written");
        check(c.at("session_end") == 1, "summary event written");
        check(Log::alwaysWritten("session_x") && Log::alwaysWritten("x_restored") && !Log::alwaysWritten("live_native_pair_complete"), "classification");
        std::filesystem::remove(path);
        std::cout << "PASS " << n << " log cap checks\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << " after " << n << " checks\n";
        return 1;
    }
}
