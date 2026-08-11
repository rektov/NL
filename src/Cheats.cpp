#include "Cheats.h"
#include "Embedded.h"
#include "Log.h"
#include "Paths.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <utility>

using json = nlohmann::json;

namespace nl::data {
namespace {

time_t ReadLastLaunch(const json& obj) {
    if (obj.contains("last_launch")) return obj.value("last_launch", 0LL);
    return obj.value("lastlaunch", 0LL);
}

void WriteLastLaunch(json& obj, time_t when) {
    obj["last_launch"] = when;
    obj.erase("lastlaunch");
}

void EraseLastLaunch(json& obj) {
    obj.erase("last_launch");
    obj.erase("lastlaunch");
}

bool IsValidStoreShape(const json& data) {
    return data.is_array() && !data.empty();
}

void ParseChangelogs(const json& node, BranchInfo& out) {
    out.changelogs.clear();
    if (!node.contains("changelog")) return;

    const json& cl = node["changelog"];
    auto pushOne = [&](const json& e) {
        ChangelogEntry ent;
        ent.date = e.value("date", 0LL);
        ent.text = e.value("changelog", "");
        if (!ent.text.empty() || ent.date != 0) out.changelogs.push_back(std::move(ent));
    };

    if (cl.is_array()) {
        for (const auto& e : cl)
            if (e.is_object()) pushOne(e);
    } else if (cl.is_object()) {
        pushOne(cl);
    }
}

BranchInfo ParseBranch(const json& b) {
    BranchInfo out;
    out.id = b.value("id", "");
    out.name = b.value("name", out.id);
    out.version = b.value("version", "");
    out.buildDate = b.value("build_date", 0LL);
    out.lastLaunch = ReadLastLaunch(b);
    ParseChangelogs(b, out);
    return out;
}

void FillFromJson(CheatInfo& out, const json& cheat) {
    out = {};
    out.id = cheat.value("cheat", "");
    out.name = cheat.value("name", "");
    out.license = cheat.value("license", 0LL);
    out.lifetime = cheat.value("lifetime", false);
    out.selectedBranch = cheat.value("selected_branch", "");
    const time_t legacyLaunch = ReadLastLaunch(cheat);

    if (cheat.contains("branches") && cheat["branches"].is_array()) {
        for (const auto& b : cheat["branches"])
            out.branches.push_back(ParseBranch(b));
    } else {
        BranchInfo legacy;
        legacy.id = "default";
        legacy.name = cheat.value("type", "Release");
        legacy.version = cheat.value("version", "");
        legacy.buildDate = cheat.value("build_date", 0LL);
        legacy.lastLaunch = legacyLaunch;
        ParseChangelogs(cheat, legacy);
        out.branches.push_back(legacy);
        if (out.selectedBranch.empty()) out.selectedBranch = legacy.id;
    }

    if (out.selectedBranch.empty() && !out.branches.empty())
        out.selectedBranch = out.branches.front().id;

    if (legacyLaunch != 0) {
        if (auto* ab = out.ActiveBranch()) {
            if (ab->lastLaunch == 0) ab->lastLaunch = legacyLaunch;
        }
    }
}

bool ApplyStore(CheatStore& store, const json& data) {
    if (!data.is_array()) return false;
    store.items.clear();
    for (const auto& cheat : data) {
        if (!cheat.is_object()) continue;
        const std::string id = cheat.value("cheat", "");
        if (id.empty()) continue;
        CheatInfo info;
        FillFromJson(info, cheat);
        if (info.id.empty()) info.id = id;
        store.items.push_back(std::move(info));
    }
    return !store.items.empty();
}

constexpr std::uintmax_t kMaxStoreBytes = 8u * 1024u * 1024u;

json LoadJsonFile(const std::filesystem::path& path) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (!ec && size > kMaxStoreBytes) {
        Log("cheats.json too large ({} bytes > {} cap); ignoring", size, kMaxStoreBytes);
        return nullptr;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return nullptr;
    try {
        return json::parse(in);
    } catch (const std::exception& e) {
        Log("JSON parse failed ({}): {}", path.string(), e.what());
        return nullptr;
    }
}

json LoadEmbeddedJson() {
    const embed::Blob blob = nl::embed::CheatsJson();
    try {
        return json::parse(blob.data, blob.data + blob.size);
    } catch (const std::exception& e) {
        Log("Embedded cheats JSON parse failed: {}", e.what());
        return nullptr;
    }
}

std::filesystem::path WithSuffix(const std::filesystem::path& path, const char* suffix) {
    std::filesystem::path out = path;
    out += suffix;
    return out;
}

bool WriteJsonFile(const std::filesystem::path& path, const json& data) {
    namespace fs = std::filesystem;
    const fs::path tmp = WithSuffix(path, ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            Log("Failed to open temp JSON for write: {}", tmp.string());
            return false;
        }
        out << data.dump(4);
        out.flush();
        if (!out) {
            Log("Failed while writing temp JSON: {}", tmp.string());
            out.close();
            std::error_code ec;
            fs::remove(tmp, ec);
            return false;
        }
    }

    std::error_code renameEc;
    fs::rename(tmp, path, renameEc);
    if (!renameEc) return true;

    const fs::path bak = WithSuffix(path, ".prev");
    std::error_code existsEc;
    const bool haveDest = fs::exists(path, existsEc);
    if (haveDest) {
        std::error_code bakEc;
        fs::rename(path, bak, bakEc);
        if (bakEc) {
            Log("JSON write: failed to back up destination ({}): {}", path.string(),
                bakEc.message());
            std::error_code rmEc;
            fs::remove(tmp, rmEc);
            return false;
        }
    }
    std::error_code finalEc;
    fs::rename(tmp, path, finalEc);
    if (!finalEc) {
        std::error_code rmEc;
        fs::remove(bak, rmEc);
        return true;
    }
    if (haveDest) {
        std::error_code restoreEc;
        fs::rename(bak, path, restoreEc);
    }
    std::error_code rmEc;
    fs::remove(tmp, rmEc);
    Log("Atomic JSON rename failed ({} -> {}): {}", tmp.string(), path.string(), finalEc.message());
    return false;
}

void QuarantineBadFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return;
    const std::filesystem::path bad = WithSuffix(path, ".bad");
    std::error_code ec;
    std::filesystem::rename(path, bad, ec);
    if (ec)
        Log("Failed to quarantine bad cheats.json: {}", ec.message());
    else
        Log("Quarantined unusable cheats.json -> {}", bad.string());
}

json EnsureUserJson() {
    const std::filesystem::path path = nl::UserCheatsPath();
    std::error_code existsEc;
    const bool hadFile = std::filesystem::exists(path, existsEc);
    json data = LoadJsonFile(path);
    if (IsValidStoreShape(data)) {
        CheatStore probe;
        bool usable = false;
        try {
            usable = ApplyStore(probe, data);
        } catch (const std::exception& e) {
            Log("cheats.json has invalid field types ({}); reseeding", e.what());
        }
        if (usable) return data;
        Log("cheats.json shape OK but unusable; reseeding");
        QuarantineBadFile(path);
    } else if (hadFile) {
        QuarantineBadFile(path);
    }

    data = LoadEmbeddedJson();
    if (IsValidStoreShape(data)) {
        if (!WriteJsonFile(path, data)) Log("Failed to seed user cheats.json");
    }
    return data;
}

} // namespace

const BranchInfo* CheatInfo::ActiveBranch() const {
    const auto it = std::ranges::find(branches, selectedBranch, &BranchInfo::id);
    if (it != branches.end()) return &*it;
    return branches.empty() ? nullptr : &branches.front();
}

BranchInfo* CheatInfo::ActiveBranch() {
    return const_cast<BranchInfo*>(std::as_const(*this).ActiveBranch());
}

bool ParseStoreJson(std::string_view text, CheatStore& out, std::string* err) {
    try {
        json data = json::parse(text);
        if (!ApplyStore(out, data)) {
            if (err) *err = "invalid store shape or empty";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}

bool UpdateLastLaunchJson(json& data, std::string_view cheatId, std::string_view branchId,
                          time_t when) {
    for (auto& cheat : data) {
        if (!cheat.is_object()) continue;
        if (cheat.value("cheat", "") != cheatId) continue;

        if (cheat.contains("branches") && cheat["branches"].is_array()) {
            for (auto& b : cheat["branches"]) {
                if (!b.is_object()) continue;
                if (b.value("id", "") != branchId) continue;
                WriteLastLaunch(b, when);
                EraseLastLaunch(cheat);
                return true;
            }
            return false;
        }
        WriteLastLaunch(cheat, when);
        return true;
    }
    return false;
}

bool UpdateSelectedBranchJson(json& data, std::string_view cheatId, std::string_view branchId) {
    for (auto& cheat : data) {
        if (!cheat.is_object()) continue;
        if (cheat.value("cheat", "") != cheatId) continue;

        if (cheat.contains("branches") && cheat["branches"].is_array()) {
            const auto& branches = cheat["branches"];
            const bool found = std::any_of(branches.begin(), branches.end(), [&](const json& b) {
                return b.is_object() && b.value("id", "") == branchId;
            });
            if (!found) return false;
        }
        cheat["selected_branch"] = branchId;
        return true;
    }
    return false;
}

bool CheatStore::Load() {
    try {
        json data = EnsureUserJson();
        if (!IsValidStoreShape(data)) data = LoadEmbeddedJson();
        if (!IsValidStoreShape(data)) {
            Log("CheatStore::Load: no valid store data");
            return false;
        }
        return ApplyStore(*this, data);
    } catch (const std::exception& e) {
        Log("CheatStore::Load failed: {}", e.what());
        return false;
    }
}

bool CheatStore::SaveLastLaunch(const std::string& cheatId, const std::string& branchId,
                                time_t when) try {
    json data = EnsureUserJson();
    if (!IsValidStoreShape(data)) return false;

    if (!UpdateLastLaunchJson(data, cheatId, branchId, when)) {
        Log("SaveLastLaunch: cheat/branch not found ({}/{})", cheatId, branchId);
        return false;
    }
    if (!WriteJsonFile(nl::UserCheatsPath(), data)) {
        Log("SaveLastLaunch: write failed");
        return false;
    }
    return true;
} catch (const std::exception& e) {
    Log("SaveLastLaunch failed: {}", e.what());
    return false;
}

bool CheatStore::SaveSelectedBranch(const std::string& cheatId, const std::string& branchId) try {
    json data = EnsureUserJson();
    if (!IsValidStoreShape(data)) return false;

    if (!UpdateSelectedBranchJson(data, cheatId, branchId)) {
        Log("SaveSelectedBranch: cheat/branch not found ({}/{})", cheatId, branchId);
        return false;
    }
    if (!WriteJsonFile(nl::UserCheatsPath(), data)) {
        Log("SaveSelectedBranch: write failed");
        return false;
    }
    return true;
} catch (const std::exception& e) {
    Log("SaveSelectedBranch failed: {}", e.what());
    return false;
}

CheatInfo* CheatStore::Find(std::string_view id) {
    auto it = std::ranges::find(items, id, &CheatInfo::id);
    return it == items.end() ? nullptr : std::addressof(*it);
}

const CheatInfo* CheatStore::Find(std::string_view id) const {
    auto it = std::ranges::find(items, id, &CheatInfo::id);
    return it == items.end() ? nullptr : std::addressof(*it);
}

} // namespace nl::data
