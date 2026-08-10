#include <doctest/doctest.h>
#include "Cheats.h"
#include "Paths.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

using json = nlohmann::json;
using nl::data::CheatStore;
namespace fs = std::filesystem;

namespace {

struct TempStoreDir {
    fs::path dir;

    TempStoreDir() {
        static int counter = 0;
        dir = fs::temp_directory_path() / ("nl-loader-tests-" + std::to_string(counter++));
        fs::remove_all(dir);
        fs::create_directories(dir);
        nl::SetUserDataDirOverride(dir);
    }
    ~TempStoreDir() {
        nl::SetUserDataDirOverride({});
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    TempStoreDir(const TempStoreDir&) = delete;
    TempStoreDir& operator=(const TempStoreDir&) = delete;

    [[nodiscard]] fs::path StorePath() const { return dir / "cheats.json"; }

    [[nodiscard]] json ReadStoreFile() const {
        std::ifstream in(StorePath(), std::ios::binary);
        REQUIRE(in.is_open());
        return json::parse(in);
    }
};

};

TEST_CASE("Load seeds the user file from the embedded catalog on first run") {
    TempStoreDir tmp;
    CheatStore store;

    REQUIRE(store.Load());
    CHECK_FALSE(store.items.empty());
    CHECK(fs::exists(tmp.StorePath()));

    std::ifstream repo(fs::path(NL_TEST_DATA_DIR) / "cheats.json", std::ios::binary);
    REQUIRE(repo.is_open());
    CHECK(tmp.ReadStoreFile() == json::parse(repo));

    const size_t count = store.items.size();
    REQUIRE(store.Load());
    CHECK(store.items.size() == count);
}

TEST_CASE("SaveLastLaunch rewrites the branch and migrates legacy keys") {
    TempStoreDir tmp;
    CheatStore store;
    REQUIRE(store.Load());
    REQUIRE(store.Find("cs2") != nullptr);

    REQUIRE(store.SaveLastLaunch("cs2", "nightly", 4242));

    const json data = tmp.ReadStoreFile();
    bool checked = false;
    for (const auto& cheat : data) {
        if (cheat.value("cheat", "") != "cs2") continue;
        for (const auto& b : cheat["branches"]) {
            if (b.value("id", "") != "nightly") continue;
            CHECK(b["last_launch"] == 4242);
            CHECK_FALSE(b.contains("lastlaunch"));
            checked = true;
        }
        CHECK_FALSE(cheat.contains("lastlaunch"));
    }
    CHECK(checked);

    CHECK_FALSE(store.SaveLastLaunch("cs2", "no-such-branch", 1));
    CHECK_FALSE(store.SaveLastLaunch("no-such-cheat", "release", 1));
}

TEST_CASE("SaveSelectedBranch persists and survives a reload") {
    TempStoreDir tmp;
    CheatStore store;
    REQUIRE(store.Load());

    REQUIRE(store.SaveSelectedBranch("csgo", "stable"));

    CheatStore reloaded;
    REQUIRE(reloaded.Load());
    const auto* csgo = reloaded.Find("csgo");
    REQUIRE(csgo != nullptr);
    CHECK(csgo->selectedBranch == "stable");
    REQUIRE(csgo->ActiveBranch() != nullptr);
    CHECK(csgo->ActiveBranch()->id == "stable");

    CHECK_FALSE(store.SaveSelectedBranch("csgo", "no-such-branch"));
}

TEST_CASE("A corrupt user file is quarantined and reseeded") {
    TempStoreDir tmp;
    {
        std::ofstream out(tmp.StorePath(), std::ios::binary);
        out << "this is not json";
    }

    CheatStore store;
    REQUIRE(store.Load());
    CHECK_FALSE(store.items.empty());

    fs::path bad = tmp.StorePath();
    bad += ".bad";
    CHECK(fs::exists(bad));
    CHECK(tmp.ReadStoreFile().is_array());
}

TEST_CASE("An oversized user file is ignored and reseeded") {
    TempStoreDir tmp;
    {
        std::ofstream out(tmp.StorePath(), std::ios::binary);
        const std::string chunk(1024 * 1024, ' ');
        for (int i = 0; i < 9; ++i) out << chunk;
    }

    CheatStore store;
    REQUIRE(store.Load());
    CHECK_FALSE(store.items.empty());
    CHECK(fs::file_size(tmp.StorePath()) < 1024 * 1024);

    fs::path bad = tmp.StorePath();
    bad += ".bad";
    CHECK(fs::exists(bad));
}
