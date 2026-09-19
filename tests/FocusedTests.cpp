#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "DjemLogic.h"
#include "utils/Logger.h"
#include "utils/Settings.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

TEST_CASE("DJEM selects the lower-discontinuity diagonal") {
    Djem::Settings settings;
    const auto flip = Djem::EvaluateCell({0.0F, 10.0F, 10.0F, 20.0F}, 0, settings);
    CHECK(flip.candidate);
    CHECK(flip.action == Djem::CellAction::SetFlipped);

    const auto normal = Djem::EvaluateCell({0.0F, 20.0F, 0.0F, 0.0F}, Djem::kInternalFlipFlag, settings);
    CHECK(normal.candidate);
    CHECK(normal.action == Djem::CellAction::SetNormal);

    const auto alreadyFlipped = Djem::EvaluateCell({0.0F, 10.0F, 10.0F, 20.0F}, Djem::kInternalFlipFlag, settings);
    CHECK(alreadyFlipped.candidate);
    CHECK(alreadyFlipped.action == Djem::CellAction::None);
}

TEST_CASE("DJEM applies threshold, hysteresis, stale-flip, and cliff rules") {
    Djem::Settings settings;
    const auto belowThreshold = Djem::EvaluateCell({0.0F, 1.0F, 2.0F, 11.99F}, Djem::kInternalFlipFlag, settings);
    CHECK_FALSE(belowThreshold.candidate);
    CHECK(belowThreshold.action == Djem::CellAction::SetNormal);

    settings.clearNonCandidates = false;
    CHECK(Djem::EvaluateCell({0.0F, 1.0F, 2.0F, 11.99F}, Djem::kInternalFlipFlag, settings).action ==
          Djem::CellAction::None);

    settings.clearNonCandidates = true;
    const auto atThreshold = Djem::EvaluateCell({0.0F, 6.0F, 6.0F, 12.0F}, 0, settings);
    CHECK(atThreshold.candidate);
    CHECK(Djem::EvaluateCell({0.0F, 20.02F, 0.04F, 20.0F}, Djem::kInternalFlipFlag, settings).action ==
          Djem::CellAction::None);
    CHECK(Djem::EvaluateCell({0.0F, 20.02F, 0.04F, 20.0F}, 0, settings).action == Djem::CellAction::None);
    CHECK(Djem::EvaluateCell({0.0F, 20.02F, 0.04F, 20.0F}, 0, settings).action == Djem::CellAction::None);

    const auto cliff =
        Djem::EvaluateCell({0.0F, 20.0F, 20.0F, 40.0F}, Djem::kCliffManagedFlag | Djem::kInternalFlipFlag, settings);
    CHECK_FALSE(cliff.candidate);
    CHECK(cliff.action == Djem::CellAction::None);
}

TEST_CASE("dirty vertex rectangles expand and clamp to valid cells") {
    const auto middle = Djem::ComputeAffectedCellBounds({3, 4, 5, 6}, 20, 20);
    CHECK(middle.valid);
    CHECK(middle.left == 2);
    CHECK(middle.top == 3);
    CHECK(middle.right == 5);
    CHECK(middle.bottom == 6);

    const auto edge = Djem::ComputeAffectedCellBounds({-5, -3, 99, 80}, 10, 12);
    CHECK(edge.valid);
    CHECK(edge.left == 0);
    CHECK(edge.top == 0);
    CHECK(edge.right == 10);
    CHECK(edge.bottom == 12);
    CHECK_FALSE(Djem::ComputeAffectedCellBounds({15, 15, 14, 14}, 20, 20).valid);
}

TEST_CASE("flipped altitude mapping and interpolation cover both triangles") {
    Djem::WorldCell cell;
    CHECK(Djem::TryMapWorldToCell(20.0F, 36.0F, 64.0F, 64.0F, 2, 2, cell));
    CHECK(cell.x == 1);
    CHECK(cell.z == 2);
    CHECK(cell.localX == doctest::Approx(0.25F));
    CHECK(cell.localZ == doctest::Approx(0.25F));

    CHECK_FALSE(Djem::TryMapWorldToCell(-1.0F, 0.0F, 64.0F, 64.0F, 2, 2, cell));
    CHECK_FALSE(Djem::TryMapWorldToCell(64.0F, 0.0F, 64.0F, 64.0F, 2, 2, cell));
    CHECK_FALSE(Djem::TryMapWorldToCell(std::numeric_limits<float>::quiet_NaN(), 0.0F, 64.0F, 64.0F, 2, 2, cell));

    const Djem::CellHeights heights{0.0F, 10.0F, 20.0F, 40.0F};
    CHECK(Djem::InterpolateFlippedCell(heights, 0.25F, 0.25F) == doctest::Approx(7.5F));
    CHECK(Djem::InterpolateFlippedCell(heights, 0.75F, 0.75F) == doctest::Approx(27.5F));
    CHECK(Djem::InterpolateFlippedCell(heights, 0.75F, 0.25F) == doctest::Approx(12.5F));
}

namespace {
void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

struct LoggerFixture {
    LoggerFixture() { Logger::Initialize("SC4DjemFixTests", "", false); }
    ~LoggerFixture() { Logger::Shutdown(); }
};
} // namespace

TEST_CASE("settings load defaults and valid values") {
    LoggerFixture logger;
    Settings defaults;
    const auto& defaultDjem = defaults.GetDjemSettings();
    CHECK(defaults.GetLogLevel() == spdlog::level::info);
    CHECK(defaults.GetLogToFile());
    CHECK(defaultDjem.enabled);
    CHECK(defaultDjem.clearNonCandidates);
    CHECK(defaultDjem.matchHeightQueriesToFlippedCells);
    CHECK(defaultDjem.minHeightDelta == doctest::Approx(12.0F));
    CHECK(defaultDjem.diagonalHysteresis == doctest::Approx(0.05F));
    CHECK(defaultDjem.logEveryNChanges == 0);

    const auto path = std::filesystem::current_path() / "SC4DjemFix-valid-test.ini";
    WriteTextFile(
        path,
        "[SC4DjemFix]\nLogLevel=debug\nLogToFile=false\nEnabled=false\nClearNonCandidates=false\n"
        "MatchHeightQueriesToFlippedCells=false\nMinHeightDelta=18.5\nDiagonalHysteresis=0.25\nLogEveryNChanges=1\n");
    Settings valid;
    valid.Load(path);
    const auto& djem = valid.GetDjemSettings();
    CHECK(valid.GetLogLevel() == spdlog::level::debug);
    CHECK_FALSE(valid.GetLogToFile());
    CHECK_FALSE(djem.enabled);
    CHECK_FALSE(djem.clearNonCandidates);
    CHECK_FALSE(djem.matchHeightQueriesToFlippedCells);
    CHECK(djem.minHeightDelta == doctest::Approx(18.5F));
    CHECK(djem.diagonalHysteresis == doctest::Approx(0.25F));
    CHECK(djem.logEveryNChanges == 1);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST_CASE("invalid settings use safe defaults") {
    LoggerFixture logger;
    const auto path = std::filesystem::current_path() / "SC4DjemFix-invalid-test.ini";
    WriteTextFile(
        path,
        "[SC4DjemFix]\nLogLevel=loud\nLogToFile=maybe\nEnabled=perhaps\nClearNonCandidates=invalid\n"
        "MatchHeightQueriesToFlippedCells=invalid\nMinHeightDelta=-1\nDiagonalHysteresis=nan\nLogEveryNChanges=-1\n");
    Settings invalid;
    invalid.Load(path);
    const auto& djem = invalid.GetDjemSettings();
    CHECK(invalid.GetLogLevel() == spdlog::level::info);
    CHECK(invalid.GetLogToFile());
    CHECK(djem.enabled);
    CHECK(djem.clearNonCandidates);
    CHECK(djem.matchHeightQueriesToFlippedCells);
    CHECK(djem.minHeightDelta == doctest::Approx(12.0F));
    CHECK(djem.diagonalHysteresis == doctest::Approx(0.05F));
    CHECK(djem.logEveryNChanges == 0);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST_CASE("overflowing settings use safe defaults") {
    LoggerFixture logger;
    const auto path = std::filesystem::current_path() / "SC4DjemFix-overflow-test.ini";
    WriteTextFile(path, "[SC4DjemFix]\nMinHeightDelta=1e100\nDiagonalHysteresis=1e100\nLogEveryNChanges=4294967296\n");
    Settings overflowing;
    overflowing.Load(path);
    const auto& djem = overflowing.GetDjemSettings();
    CHECK(djem.minHeightDelta == doctest::Approx(12.0F));
    CHECK(djem.diagonalHysteresis == doctest::Approx(0.05F));
    CHECK(djem.logEveryNChanges == 0);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST_CASE("overflowing settings use safe defaults") {
    LoggerFixture logger;
    const auto path = std::filesystem::current_path() / "SC4DjemFix-overflow-test.ini";
    WriteTextFile(path, "[SC4DjemFix]\nMinHeightDelta=1e100\nDiagonalHysteresis=1e100\nLogEveryNChanges=4294967296\n");
    Settings overflowing;
    overflowing.Load(path);
    const auto& djem = overflowing.GetDjemSettings();
    CHECK(djem.minHeightDelta == doctest::Approx(12.0F));
    CHECK(djem.diagonalHysteresis == doctest::Approx(0.05F));
    CHECK(djem.logEveryNChanges == 0);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST_CASE("relative call guard calculations decode without memory writes") {
    constexpr Djem::RelativeCallBytes original{0xE8, 0x8E, 0xA2, 0xFF, 0xFF};
    const auto target = Djem::DecodeRelativeCallTarget(0x007498CD, original);
    REQUIRE(target);
    CHECK(*target == 0x00743B60);

    constexpr Djem::RelativeCallBytes wrongOpcode{0xE9, 0x8E, 0xA2, 0xFF, 0xFF};
    CHECK_FALSE(Djem::DecodeRelativeCallTarget(0x007498CD, wrongOpcode));

    const auto encoded = Djem::EncodeRelativeCall(0x007498CD, 0x00743B60);
    REQUIRE(encoded);
    CHECK(*encoded == original);
}
