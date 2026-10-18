#include <gtest/gtest.h>

#include "calibration_settings.h"
#include "ui_state.h"

#include <map>
#include <string>

namespace
{
using namespace ui;

void expect_colours(ButtonStyle style, std::uint8_t background, std::uint8_t text)
{
    const ButtonColours colours = colours_for(style);
    EXPECT_EQ(background, colours.background);
    EXPECT_EQ(text, colours.text);
}

TEST(UiStateTest, ButtonPaletteMatchesSilverGreenRed)
{
    expect_colours(ButtonStyle::Inactive, COLOUR_SILVER, COLOUR_BLACK);
    expect_colours(ButtonStyle::ActiveGreen, COLOUR_GREEN, COLOUR_WHITE);
    expect_colours(ButtonStyle::ActiveRed, COLOUR_RED, COLOUR_WHITE);
}

TEST(UiStateTest, PowerButtonsLightExactlyOneState)
{
    EXPECT_EQ(ButtonStyle::ActiveGreen, power_on_style(true));
    EXPECT_EQ(ButtonStyle::Inactive, power_off_style(true));
    EXPECT_EQ(ButtonStyle::Inactive, power_on_style(false));
    EXPECT_EQ(ButtonStyle::ActiveRed, power_off_style(false));
}

TEST(UiStateTest, ExplicitPowerRequestsOnlyToggleWhenStateDiffers)
{
    bool power = true;
    int toggles = 0;
    auto isOn = [&power]() { return power; };
    auto toggle = [&power, &toggles]() {
        power = !power;
        toggles++;
    };

    EXPECT_TRUE(set_power_state(true, isOn, toggle));
    EXPECT_EQ(0, toggles);
    EXPECT_FALSE(set_power_state(false, isOn, toggle));
    EXPECT_EQ(1, toggles);
    EXPECT_FALSE(set_power_state(false, isOn, toggle));
    EXPECT_EQ(1, toggles);
    EXPECT_TRUE(set_power_state(true, isOn, toggle));
    EXPECT_EQ(2, toggles);
}

TEST(UiStateTest, MarkerSidesAreIndependentToggles)
{
    MarkerModel marker;
    EXPECT_STREQ("OFF", marker.detail_text());
    expect_colours(marker.left_style(), COLOUR_SILVER, COLOUR_BLACK);

    EXPECT_TRUE(marker.press_left());
    EXPECT_STREQ("LEFT", marker.detail_text());
    EXPECT_EQ(ButtonStyle::ActiveRed, marker.left_style());
    EXPECT_EQ(ButtonStyle::Inactive, marker.right_style());

    EXPECT_TRUE(marker.press_right());
    EXPECT_STREQ("LEFT + RIGHT", marker.detail_text());
    EXPECT_EQ(ButtonStyle::ActiveRed, marker.left_style());
    EXPECT_EQ(ButtonStyle::ActiveRed, marker.right_style());

    EXPECT_TRUE(marker.press_left());
    EXPECT_STREQ("RIGHT", marker.detail_text());
    EXPECT_TRUE(marker.press_right());
    EXPECT_STREQ("OFF", marker.detail_text());
}

TEST(UiStateTest, MarkerAutoClearsAndBlocksSides)
{
    MarkerModel marker;
    marker.press_left();
    marker.press_right();
    EXPECT_TRUE(marker.side_buttons_enabled());

    marker.press_auto();
    EXPECT_TRUE(marker.automatic());
    EXPECT_FALSE(marker.left());
    EXPECT_FALSE(marker.right());
    EXPECT_FALSE(marker.side_buttons_enabled());
    EXPECT_STREQ("AUTO", marker.detail_text());
    EXPECT_EQ(ButtonStyle::ActiveRed, marker.auto_style());
    EXPECT_EQ(ButtonStyle::Inactive, marker.left_style());
    EXPECT_EQ(ButtonStyle::Inactive, marker.right_style());

    // Guard: events that still arrive while AUTO is on are ignored
    EXPECT_FALSE(marker.press_left());
    EXPECT_FALSE(marker.press_right());
    EXPECT_FALSE(marker.left());
    EXPECT_FALSE(marker.right());

    marker.press_auto();
    EXPECT_FALSE(marker.automatic());
    EXPECT_TRUE(marker.side_buttons_enabled());
    EXPECT_EQ(ButtonStyle::Inactive, marker.auto_style());
    EXPECT_STREQ("OFF", marker.detail_text());
    EXPECT_TRUE(marker.press_left());
}

TEST(UiStateTest, ActionSelectionNeverLightsMoreThanOneButton)
{
    ActionSelection selection;
    const ActionGroup groups[] = { ActionGroup::Unfold, ActionGroup::Fold };

    for (ActionGroup firstGroup : groups)
    {
        for (std::uint8_t first = 1; first <= ACTION_COUNT; first++)
        {
            for (ActionGroup secondGroup : groups)
            {
                for (std::uint8_t second = 1; second <= ACTION_COUNT; second++)
                {
                    selection.clear();
                    EXPECT_EQ(ActionSelection::Result::Selected, selection.press(firstGroup, first));
                    const ActionSelection::Result result = selection.press(secondGroup, second);

                    int lit = 0;
                    for (ActionGroup group : groups)
                    {
                        for (std::uint8_t index = 1; index <= ACTION_COUNT; index++)
                        {
                            lit += selection.is_lit(group, index) ? 1 : 0;
                        }
                    }
                    const bool same = (firstGroup == secondGroup) && (first == second);
                    EXPECT_EQ(same ? ActionSelection::Result::Cleared : ActionSelection::Result::Selected, result);
                    EXPECT_EQ(same ? 0 : 1, lit);
                    EXPECT_FALSE(selection.is_lit(firstGroup, first) && !same);
                }
            }
        }
    }
}

TEST(UiStateTest, ActionButtonsUseSilverWhenUnlitAndGreenWhenLit)
{
    const ActionGroup groups[] = { ActionGroup::Unfold, ActionGroup::Fold };
    for (ActionGroup group : groups)
    {
        for (std::uint8_t index = 1; index <= ACTION_COUNT; index++)
        {
            ActionSelection selection;
            expect_colours(selection.style(group, index), COLOUR_SILVER, COLOUR_BLACK);
            selection.press(group, index);
            expect_colours(selection.style(group, index), COLOUR_GREEN, COLOUR_WHITE);
            EXPECT_EQ(index, selection.active_index());
            selection.press(group, index);
            expect_colours(selection.style(group, index), COLOUR_SILVER, COLOUR_BLACK);
            EXPECT_EQ(0, selection.active_index());
        }
    }
}

TEST(UiStateTest, ClearingSelectionUnlightsEverythingAndInvalidIndexIsIgnored)
{
    ActionSelection selection;
    EXPECT_EQ(ActionSelection::Result::Ignored, selection.press(ActionGroup::Unfold, 0));
    EXPECT_EQ(ActionSelection::Result::Ignored, selection.press(ActionGroup::Fold, ACTION_COUNT + 1));
    EXPECT_EQ(0, selection.active_index());

    selection.press(ActionGroup::Fold, 3);
    selection.clear();
    for (std::uint8_t index = 1; index <= ACTION_COUNT; index++)
    {
        EXPECT_FALSE(selection.is_lit(ActionGroup::Fold, index));
        EXPECT_FALSE(selection.is_lit(ActionGroup::Unfold, index));
    }
}

TEST(UiStateTest, HeadlandButtonShowsOffSilverAndOnGreen)
{
    expect_colours(headland_style(false), COLOUR_SILVER, COLOUR_BLACK);
    expect_colours(headland_style(true), COLOUR_GREEN, COLOUR_WHITE);
    EXPECT_STREQ("OFF", headland_text(false));
    EXPECT_STREQ("ON", headland_text(true));
}

TEST(BulkFillHeadlandSettingTest, DefaultsOffWhenMissingOrCorrupt)
{
    EXPECT_FALSE(calibration::load_bulk_fill_headland([](const char *, std::uint8_t &) { return false; }));
    EXPECT_FALSE(calibration::load_bulk_fill_headland([](const char *, std::uint8_t &value) {
        value = 2;
        return true;
    }));
}

TEST(BulkFillHeadlandSettingTest, StoredValueSurvivesReload)
{
    std::map<std::string, std::uint8_t> storage;
    auto writer = [&storage](const char *key, std::uint8_t value) {
        storage[key] = value;
        return true;
    };
    auto reader = [&storage](const char *key, std::uint8_t &value) {
        const auto found = storage.find(key);
        if (found == storage.end())
        {
            return false;
        }
        value = found->second;
        return true;
    };

    bool state = false;
    EXPECT_EQ(calibration::SaveResult::Saved, calibration::save_bulk_fill_headland(true, state, writer));
    EXPECT_TRUE(state);
    EXPECT_TRUE(calibration::load_bulk_fill_headland(reader));
    EXPECT_STREQ("bulk_headland", calibration::bulk_fill_headland_key());

    EXPECT_EQ(calibration::SaveResult::Saved, calibration::save_bulk_fill_headland(false, state, writer));
    EXPECT_FALSE(state);
    EXPECT_FALSE(calibration::load_bulk_fill_headland(reader));
}

TEST(BulkFillHeadlandSettingTest, FailedWriteKeepsPreviousState)
{
    bool state = true;
    EXPECT_EQ(calibration::SaveResult::StorageFailure,
              calibration::save_bulk_fill_headland(false, state, [](const char *, std::uint8_t) { return false; }));
    EXPECT_TRUE(state);
}
} // namespace
