#pragma once

#include <cstdint>

namespace calibration
{
enum class Point
{
    FullRaise,
    LimitedRaise,
    StartPlant,
    StopPlant
};

static constexpr std::uint8_t FULL_RAISE_DEFAULT = 95;
static constexpr std::uint8_t LIMITED_RAISE_DEFAULT = 75;
static constexpr std::uint8_t START_PLANT_DEFAULT = 15;
static constexpr std::uint8_t STOP_PLANT_DEFAULT = 0;
static constexpr std::uint8_t MAX_PERCENT = 100;

struct Points
{
    std::uint8_t fullRaise = FULL_RAISE_DEFAULT;
    std::uint8_t limitedRaise = LIMITED_RAISE_DEFAULT;
    std::uint8_t startPlant = START_PLANT_DEFAULT;
    std::uint8_t stopPlant = STOP_PLANT_DEFAULT;
};

inline const char *key(Point point)
{
    switch (point)
    {
        case Point::FullRaise:
            return "full_raise";
        case Point::LimitedRaise:
            return "limited_raise";
        case Point::StartPlant:
            return "start_plant";
        case Point::StopPlant:
            return "stop_plant";
    }
    return "";
}

inline std::uint8_t &value(Points &points, Point point)
{
    switch (point)
    {
        case Point::FullRaise:
            return points.fullRaise;
        case Point::LimitedRaise:
            return points.limitedRaise;
        case Point::StartPlant:
            return points.startPlant;
        case Point::StopPlant:
            return points.stopPlant;
    }
    return points.fullRaise;
}

template <typename Reader>
std::uint8_t load_value(Reader read, const char *name, std::uint8_t defaultValue)
{
    std::uint8_t storedValue = 0;
    if (!read(name, storedValue) || (storedValue > MAX_PERCENT))
    {
        return defaultValue;
    }
    return storedValue;
}

template <typename Reader>
Points load_points(Reader read)
{
    Points points;
    points.fullRaise = load_value(read, key(Point::FullRaise), FULL_RAISE_DEFAULT);
    points.limitedRaise = load_value(read, key(Point::LimitedRaise), LIMITED_RAISE_DEFAULT);
    points.startPlant = load_value(read, key(Point::StartPlant), START_PLANT_DEFAULT);
    points.stopPlant = load_value(read, key(Point::StopPlant), STOP_PLANT_DEFAULT);
    if (points.stopPlant > points.startPlant)
    {
        points.startPlant = START_PLANT_DEFAULT;
        points.stopPlant = STOP_PLANT_DEFAULT;
    }
    return points;
}

enum class SaveResult
{
    Saved,
    InvalidValue,
    InvalidOrder,
    StorageFailure
};

template <typename Writer>
SaveResult save_point(Point point, std::uint32_t newValue, Points &points, Writer write)
{
    if (newValue > MAX_PERCENT)
    {
        return SaveResult::InvalidValue;
    }
    if ((Point::StartPlant == point) && (newValue < points.stopPlant))
    {
        return SaveResult::InvalidOrder;
    }
    if ((Point::StopPlant == point) && (newValue > points.startPlant))
    {
        return SaveResult::InvalidOrder;
    }
    if (!write(key(point), static_cast<std::uint8_t>(newValue)))
    {
        return SaveResult::StorageFailure;
    }
    value(points, point) = static_cast<std::uint8_t>(newValue);
    return SaveResult::Saved;
}
} // namespace calibration
