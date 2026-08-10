#pragma once
#include <nlohmann/json_fwd.hpp>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

namespace nl::data {

struct ChangelogEntry {
    time_t date = 0;
    std::string text;
};

struct BranchInfo {
    std::string id;
    std::string name;
    std::string version;
    time_t buildDate = 0;
    time_t lastLaunch = 0;
    std::vector<ChangelogEntry> changelogs;
};

struct CheatInfo {
    std::string id;
    std::string name;
    std::string selectedBranch;
    std::vector<BranchInfo> branches;
    time_t license = 0;
    bool lifetime = false;

    BranchInfo* ActiveBranch();
    const BranchInfo* ActiveBranch() const;
    [[nodiscard]] bool IsActive(time_t now) const { return lifetime || license > now; }
};

struct CheatStore {
    std::vector<CheatInfo> items;

    [[nodiscard]] bool Load();
    [[nodiscard]] bool SaveLastLaunch(const std::string& cheatId, const std::string& branchId,
                                      time_t when);
    [[nodiscard]] bool SaveSelectedBranch(const std::string& cheatId, const std::string& branchId);
    [[nodiscard]] CheatInfo* Find(std::string_view id);
    [[nodiscard]] const CheatInfo* Find(std::string_view id) const;
};

[[nodiscard]] bool ParseStoreJson(std::string_view text, CheatStore& out,
                                  std::string* err = nullptr);

[[nodiscard]] bool UpdateLastLaunchJson(nlohmann::json& data, std::string_view cheatId,
                                        std::string_view branchId, time_t when);
[[nodiscard]] bool UpdateSelectedBranchJson(nlohmann::json& data, std::string_view cheatId,
                                            std::string_view branchId);

}
