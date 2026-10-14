#include <gtest/gtest.h>

#include "calibration_settings.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace calibration;
using Values = std::map<std::string, std::uint8_t>;

TEST(CalibrationSettingsTest, MissingValuesUseIndependentSafeDefaults)
{
    const Points points = load_points([](const char *, std::uint8_t &) { return false; });

    EXPECT_EQ(FULL_RAISE_DEFAULT, points.fullRaise);
    EXPECT_EQ(LIMITED_RAISE_DEFAULT, points.limitedRaise);
    EXPECT_EQ(START_PLANT_DEFAULT, points.startPlant);
    EXPECT_EQ(STOP_PLANT_DEFAULT, points.stopPlant);
}

TEST(CalibrationSettingsTest, OutOfRangeAndCorruptValuesUseSafeDefaults)
{
    const Values stored = {
        {"full_raise", 101},
        {"limited_raise", 70},
        {"start_plant", 20},
        {"stop_plant", 255},
    };
    const Points points = load_points([&stored](const char *name, std::uint8_t &value) {
        const auto found = stored.find(name);
        if (found == stored.end())
        {
            return false;
        }
        value = found->second;
        return true;
    });

    EXPECT_EQ(FULL_RAISE_DEFAULT, points.fullRaise);
    EXPECT_EQ(70, points.limitedRaise);
    EXPECT_EQ(20, points.startPlant);
    EXPECT_EQ(STOP_PLANT_DEFAULT, points.stopPlant);
}

TEST(CalibrationSettingsTest, InvalidStoredPlantOrderDefaultsOnlyPlantPair)
{
    const Values stored = {
        {"full_raise", 90},
        {"limited_raise", 60},
        {"start_plant", 10},
        {"stop_plant", 20},
    };
    const Points points = load_points([&stored](const char *name, std::uint8_t &value) {
        const auto found = stored.find(name);
        if (found == stored.end())
        {
            return false;
        }
        value = found->second;
        return true;
    });

    EXPECT_EQ(90, points.fullRaise);
    EXPECT_EQ(60, points.limitedRaise);
    EXPECT_EQ(START_PLANT_DEFAULT, points.startPlant);
    EXPECT_EQ(STOP_PLANT_DEFAULT, points.stopPlant);
}

TEST(CalibrationSettingsTest, LegacyKeysAreIgnoredAndNewKeysLoadIndependently)
{
    const Values stored = {
        {"transport", 91},
        {"plant", 12},
        {"full_raise", 88},
        {"limited_raise", 66},
        {"start_plant", 32},
        {"stop_plant", 10},
    };
    std::vector<std::string> requested;
    const Points points = load_points([&stored, &requested](const char *name, std::uint8_t &value) {
        requested.emplace_back(name);
        const auto found = stored.find(name);
        if (found == stored.end())
        {
            return false;
        }
        value = found->second;
        return true;
    });

    EXPECT_EQ(88, points.fullRaise);
    EXPECT_EQ(66, points.limitedRaise);
    EXPECT_EQ(32, points.startPlant);
    EXPECT_EQ(10, points.stopPlant);
    EXPECT_EQ((std::vector<std::string>{"full_raise", "limited_raise", "start_plant", "stop_plant"}), requested);
}

TEST(CalibrationSettingsTest, InvalidStartOrStopDoesNotWriteOrChangeValues)
{
    Points points;
    points.startPlant = 40;
    points.stopPlant = 20;
    const Points original = points;
    int writes = 0;
    const auto writer = [&writes](const char *, std::uint8_t) {
        ++writes;
        return true;
    };

    EXPECT_EQ(SaveResult::InvalidOrder, save_point(Point::StartPlant, 19, points, writer));
    EXPECT_EQ(SaveResult::InvalidOrder, save_point(Point::StopPlant, 41, points, writer));
    EXPECT_EQ(0, writes);
    EXPECT_EQ(original.startPlant, points.startPlant);
    EXPECT_EQ(original.stopPlant, points.stopPlant);
}

TEST(CalibrationSettingsTest, StorageFailureDoesNotChangeSavedValue)
{
    Points points;
    const SaveResult result = save_point(
        Point::FullRaise,
        80,
        points,
        [](const char *, std::uint8_t) { return false; }
    );

    EXPECT_EQ(SaveResult::StorageFailure, result);
    EXPECT_EQ(FULL_RAISE_DEFAULT, points.fullRaise);
}

TEST(CalibrationSettingsTest, SuccessfulSaveWritesOnlySelectedPoint)
{
    Points points;
    std::string writtenKey;
    std::uint8_t writtenValue = 0;
    const SaveResult result = save_point(
        Point::LimitedRaise,
        63,
        points,
        [&writtenKey, &writtenValue](const char *name, std::uint8_t value) {
            writtenKey = name;
            writtenValue = value;
            return true;
        }
    );

    EXPECT_EQ(SaveResult::Saved, result);
    EXPECT_EQ("limited_raise", writtenKey);
    EXPECT_EQ(63, writtenValue);
    EXPECT_EQ(63, points.limitedRaise);
    EXPECT_EQ(FULL_RAISE_DEFAULT, points.fullRaise);
}
} // namespace
