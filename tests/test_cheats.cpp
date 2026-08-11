#include <doctest/doctest.h>
#include "Cheats.h"
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

using json = nlohmann::json;
using nl::data::CheatStore;

namespace {

constexpr const char* kTwoCheats = R"([
  {
    "cheat": "cs2",
    "name": "Counter-Strike 2",
    "license": 1788003260,
    "selected_branch": "nightly",
    "branches": [
      {
        "id": "release",
        "name": "Release",
        "version": "",
        "build_date": 100,
        "last_launch": 200,
        "changelog": [
          {"date": 10, "changelog": "- a\n- b"},
          {"date": 20, "changelog": "- c"}
        ]
      },
      {
        "id": "nightly",
        "name": "Nightly",
        "version": "1.3.0",
        "build_date": 300,
        "changelog": {"date": 30, "changelog": "- single object form"}
      }
    ]
  },
  {
    "cheat": "csgo",
    "name": "CS:GO",
    "lifetime": true,
    "branches": [{"id": "release", "name": "Release"}]
  }
])";

}

TEST_CASE("ParseStoreJson: full modern shape") {
    CheatStore store;
    std::string err;
    REQUIRE(nl::data::ParseStoreJson(kTwoCheats, store, &err));
    REQUIRE(store.items.size() == 2);

    const auto& cs2 = store.items[0];
    CHECK(cs2.id == "cs2");
    CHECK(cs2.name == "Counter-Strike 2");
    CHECK(cs2.license == 1788003260);
    CHECK_FALSE(cs2.lifetime);
    CHECK(cs2.selectedBranch == "nightly");
    REQUIRE(cs2.branches.size() == 2);

    const auto& release = cs2.branches[0];
    CHECK(release.id == "release");
    CHECK(release.buildDate == 100);
    CHECK(release.lastLaunch == 200);
    REQUIRE(release.changelogs.size() == 2);
    CHECK(release.changelogs[0].date == 10);
    CHECK(release.changelogs[0].text == "- a\n- b");

    const auto& nightly = cs2.branches[1];
    CHECK(nightly.version == "1.3.0");
    REQUIRE(nightly.changelogs.size() == 1);
    CHECK(nightly.changelogs[0].text == "- single object form");

    const auto& csgo = store.items[1];
    CHECK(csgo.lifetime);
    CHECK(csgo.selectedBranch == "release");
}

TEST_CASE("ParseStoreJson: legacy flat shape becomes a default branch") {
    const char* legacy = R"([{
      "cheat": "csgo",
      "name": "CS:GO",
      "type": "Beta",
      "version": "2.1",
      "build_date": 111,
      "lastlaunch": 222,
      "changelog": {"date": 5, "changelog": "- legacy note"}
    }])";

    CheatStore store;
    REQUIRE(nl::data::ParseStoreJson(legacy, store));
    REQUIRE(store.items.size() == 1);
    const auto& info = store.items[0];
    REQUIRE(info.branches.size() == 1);
    CHECK(info.selectedBranch == "default");
    const auto& b = info.branches[0];
    CHECK(b.id == "default");
    CHECK(b.name == "Beta");
    CHECK(b.version == "2.1");
    CHECK(b.buildDate == 111);
    CHECK(b.lastLaunch == 222);
    REQUIRE(b.changelogs.size() == 1);
}

TEST_CASE("ParseStoreJson: top-level lastlaunch adopted by the active branch") {
    const char* mixed = R"([{
      "cheat": "cs2",
      "lastlaunch": 999,
      "selected_branch": "b",
      "branches": [
        {"id": "a", "last_launch": 1},
        {"id": "b"}
      ]
    }])";

    CheatStore store;
    REQUIRE(nl::data::ParseStoreJson(mixed, store));
    const auto* active = store.items[0].ActiveBranch();
    REQUIRE(active != nullptr);
    CHECK(active->id == "b");
    CHECK(active->lastLaunch == 999);
    CHECK(store.items[0].branches[0].lastLaunch == 1);
}

TEST_CASE("ParseStoreJson: rejects invalid inputs with an error") {
    CheatStore store;
    std::string err;

    CHECK_FALSE(nl::data::ParseStoreJson("not json at all", store, &err));
    CHECK_FALSE(err.empty());

    err.clear();
    CHECK_FALSE(nl::data::ParseStoreJson("[]", store, &err));
    CHECK(err == "invalid store shape or empty");

    CHECK_FALSE(nl::data::ParseStoreJson("{}", store, &err));
    CHECK_FALSE(nl::data::ParseStoreJson("[42, null]", store, &err));
    CHECK_FALSE(nl::data::ParseStoreJson(R"([{"name": "no id"}])", store, &err));

    err.clear();
    CHECK_FALSE(nl::data::ParseStoreJson(R"([{"cheat": "x", "license": "abc"}])", store, &err));
    CHECK_FALSE(err.empty());
}

TEST_CASE("ParseStoreJson: empty changelog entries are dropped") {
    const char* data = R"([{
      "cheat": "x",
      "branches": [{"id": "r", "changelog": [{"date": 0, "changelog": ""}]}]
    }])";
    CheatStore store;
    REQUIRE(nl::data::ParseStoreJson(data, store));
    CHECK(store.items[0].branches[0].changelogs.empty());
}

TEST_CASE("CheatInfo: ActiveBranch fallbacks and IsActive") {
    nl::data::CheatInfo info;
    CHECK(info.ActiveBranch() == nullptr);

    info.branches.emplace_back().id = "a";
    info.branches.emplace_back().id = "b";
    info.selectedBranch = "missing";
    REQUIRE(info.ActiveBranch() != nullptr);
    CHECK(info.ActiveBranch()->id == "a");

    info.selectedBranch = "b";
    CHECK(info.ActiveBranch()->id == "b");

    info.license = 1000;
    CHECK(info.IsActive(999));
    CHECK_FALSE(info.IsActive(1000));
    info.lifetime = true;
    CHECK(info.IsActive(1000000));
}

TEST_CASE("CheatStore::Find") {
    CheatStore store;
    REQUIRE(nl::data::ParseStoreJson(kTwoCheats, store));
    REQUIRE(store.Find("csgo") != nullptr);
    CHECK(store.Find("csgo")->name == "CS:GO");
    CHECK(store.Find("nope") == nullptr);
    CHECK(std::as_const(store).Find("cs2") != nullptr);
}

TEST_CASE("UpdateLastLaunchJson") {
    json data = json::parse(kTwoCheats);
    data[0]["lastlaunch"] = 1;

    SUBCASE("updates the matching branch and erases legacy keys") {
        REQUIRE(nl::data::UpdateLastLaunchJson(data, "cs2", "nightly", 12345));
        const json& nightly = data[0]["branches"][1];
        CHECK(nightly["last_launch"] == 12345);
        CHECK_FALSE(nightly.contains("lastlaunch"));
        CHECK_FALSE(data[0].contains("lastlaunch"));
        CHECK_FALSE(data[0].contains("last_launch"));
    }

    SUBCASE("unknown branch or cheat fails") {
        CHECK_FALSE(nl::data::UpdateLastLaunchJson(data, "cs2", "nope", 1));
        CHECK_FALSE(nl::data::UpdateLastLaunchJson(data, "nope", "release", 1));
    }

    SUBCASE("branchless legacy cheat stores at the top level") {
        json legacy = json::parse(R"([{"cheat": "old", "lastlaunch": 5}])");
        REQUIRE(nl::data::UpdateLastLaunchJson(legacy, "old", "whatever", 77));
        CHECK(legacy[0]["last_launch"] == 77);
        CHECK_FALSE(legacy[0].contains("lastlaunch"));
    }
}

TEST_CASE("UpdateSelectedBranchJson") {
    json data = json::parse(kTwoCheats);

    CHECK(nl::data::UpdateSelectedBranchJson(data, "cs2", "release"));
    CHECK(data[0]["selected_branch"] == "release");

    CHECK_FALSE(nl::data::UpdateSelectedBranchJson(data, "cs2", "nope"));
    CHECK_FALSE(nl::data::UpdateSelectedBranchJson(data, "nope", "release"));

    json branchless = json::parse(R"([{"cheat": "old"}])");
    CHECK(nl::data::UpdateSelectedBranchJson(branchless, "old", "anything"));
    CHECK(branchless[0]["selected_branch"] == "anything");
}
