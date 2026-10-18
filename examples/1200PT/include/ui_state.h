#pragma once

#include <cstdint>

// Hardware-independent VT button state for the 1200PT screens.
// Nothing in this header touches outputs, the VT, NVS or any other module; it
// only tracks which on-screen button is lit and which colours that implies.

namespace ui
{
// ISO 11783-6 standard VT colour palette indices
static constexpr std::uint8_t COLOUR_BLACK = 0;
static constexpr std::uint8_t COLOUR_WHITE = 1;
static constexpr std::uint8_t COLOUR_GREEN = 2;
static constexpr std::uint8_t COLOUR_SILVER = 7;
static constexpr std::uint8_t COLOUR_RED = 12;

// Button palette: Inactive = silver / black text, ActiveGreen = green / white
// text, ActiveRed = red / white text. The border is always black (object pool).
enum class ButtonStyle : std::uint8_t
{
    Inactive,
    ActiveGreen,
    ActiveRed
};

struct ButtonColours
{
    std::uint8_t background;
    std::uint8_t text;
};

inline ButtonColours colours_for(ButtonStyle style)
{
    switch (style)
    {
        case ButtonStyle::ActiveGreen:
            return { COLOUR_GREEN, COLOUR_WHITE };
        case ButtonStyle::ActiveRed:
            return { COLOUR_RED, COLOUR_WHITE };
        case ButtonStyle::Inactive:
            break;
    }
    return { COLOUR_SILVER, COLOUR_BLACK };
}

// ─── POWER (VACUUM / BULK FILL) ──────────────────────────────────────────────
// Exactly one of the two buttons is lit: ON = green, OFF = red.
inline ButtonStyle power_on_style(bool powerIsOn)
{
    return powerIsOn ? ButtonStyle::ActiveGreen : ButtonStyle::Inactive;
}

inline ButtonStyle power_off_style(bool powerIsOn)
{
    return powerIsOn ? ButtonStyle::Inactive : ButtonStyle::ActiveRed;
}

// Request an explicit power state through an existing toggle-only function.
// The toggle is only invoked when the current state differs from the request.
// Returns the state reported after the request.
template <typename IsOn, typename Toggle>
bool set_power_state(bool requestedOn, IsOn isOn, Toggle toggle)
{
    if (isOn() != requestedOn)
    {
        toggle();
    }
    return isOn();
}

// ─── MARKER ──────────────────────────────────────────────────────────────────
// UI-only model. LEFT and RIGHT are independent toggles. AUTO clears both and
// blocks them until AUTO is switched off again.
class MarkerModel
{
public:
    bool left() const
    {
        return leftActive;
    }

    bool right() const
    {
        return rightActive;
    }

    bool automatic() const
    {
        return autoActive;
    }

    bool side_buttons_enabled() const
    {
        return !autoActive;
    }

    // Returns false (and changes nothing) while AUTO is on.
    bool press_left()
    {
        if (autoActive)
        {
            return false;
        }
        leftActive = !leftActive;
        return true;
    }

    // Returns false (and changes nothing) while AUTO is on.
    bool press_right()
    {
        if (autoActive)
        {
            return false;
        }
        rightActive = !rightActive;
        return true;
    }

    // Toggles AUTO. Switching AUTO on clears LEFT and RIGHT.
    void press_auto()
    {
        autoActive = !autoActive;
        if (autoActive)
        {
            leftActive = false;
            rightActive = false;
        }
    }

    ButtonStyle left_style() const
    {
        return leftActive ? ButtonStyle::ActiveRed : ButtonStyle::Inactive;
    }

    ButtonStyle right_style() const
    {
        return rightActive ? ButtonStyle::ActiveRed : ButtonStyle::Inactive;
    }

    ButtonStyle auto_style() const
    {
        return autoActive ? ButtonStyle::ActiveRed : ButtonStyle::Inactive;
    }

    // Text for Str_MarkerDetail (440 px wide, 8 px per character)
    const char *detail_text() const
    {
        if (autoActive)
        {
            return "AUTO";
        }
        if (leftActive && rightActive)
        {
            return "LEFT + RIGHT";
        }
        if (leftActive)
        {
            return "LEFT";
        }
        if (rightActive)
        {
            return "RIGHT";
        }
        return "OFF";
    }

private:
    bool leftActive = false;
    bool rightActive = false;
    bool autoActive = false;
};

// ─── UNFOLD / FOLD ACTION SELECTION ──────────────────────────────────────────
static constexpr std::uint8_t ACTION_COUNT = 6;

enum class ActionGroup : std::uint8_t
{
    Unfold,
    Fold
};

// At most one action button (of either screen) is lit at any time.
class ActionSelection
{
public:
    enum class Result : std::uint8_t
    {
        Selected, // The pressed button is now lit; any previously lit button was cleared
        Cleared, // The pressed button was lit and is now cleared
        Ignored // Invalid index
    };

    Result press(ActionGroup group, std::uint8_t index)
    {
        if ((index < 1) || (index > ACTION_COUNT))
        {
            return Result::Ignored;
        }
        if (is_lit(group, index))
        {
            clear();
            return Result::Cleared;
        }
        activeGroup = group;
        activeIndex = index;
        return Result::Selected;
    }

    void clear()
    {
        activeIndex = 0;
    }

    // 0 = nothing lit, otherwise 1..ACTION_COUNT
    std::uint8_t active_index() const
    {
        return activeIndex;
    }

    ActionGroup active_group() const
    {
        return activeGroup;
    }

    bool is_lit(ActionGroup group, std::uint8_t index) const
    {
        return (0 != activeIndex) && (activeGroup == group) && (activeIndex == index);
    }

    ButtonStyle style(ActionGroup group, std::uint8_t index) const
    {
        return is_lit(group, index) ? ButtonStyle::ActiveGreen : ButtonStyle::Inactive;
    }

    // Style of all ACTION_COUNT buttons of one screen, derived only from the
    // stored selection. Every re-send (press, periodic refresh, VT reconnect,
    // screen shown, mask change) uses this, so none of them can reset a lit button.
    void styles(ActionGroup group, ButtonStyle (&out)[ACTION_COUNT]) const
    {
        for (std::uint8_t index = 1; index <= ACTION_COUNT; index++)
        {
            out[index - 1] = style(group, index);
        }
    }

private:
    ActionGroup activeGroup = ActionGroup::Unfold;
    std::uint8_t activeIndex = 0;
};

// ─── BULK FILL HEADLAND (CAL) ────────────────────────────────────────────────
// ON = green / white, OFF = silver / black. Stored and displayed only.
inline ButtonStyle headland_style(bool enabled)
{
    return enabled ? ButtonStyle::ActiveGreen : ButtonStyle::Inactive;
}

inline const char *headland_text(bool enabled)
{
    return enabled ? "ON" : "OFF";
}
} // namespace ui
