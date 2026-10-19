#pragma once

#include "fan_vac_control.h"
#include "ui_state.h"

#include <cstddef>
#include <cstdint>

// Hardware-independent lockout feedback for the UNFOLD/FOLD screens.
// This only chooses and tracks the text shown while hydraulicActuationInhibited
// is true. It never decides whether an action may run: the interlock itself
// stays in main.cpp and is not changed by anything here.

namespace ui
{
enum class InhibitReason : std::uint8_t
{
    None, // Not inhibited (fan/vac RUNNING)
    Idle, // Fan/vac not running
    FanFault,
    VacFault,
    OutputBoardMissing
};

// Classify from the real fan/vac state and its fault flags. Fan takes
// precedence over vac, matching fan_vac_update().
inline InhibitReason classify_inhibit(FanVacState state, bool fanFault, bool vacFault)
{
    switch (state)
    {
        case FanVacState::RUNNING:
            return InhibitReason::None;
        case FanVacState::IDLE:
            return InhibitReason::Idle;
        case FanVacState::FAULT_FAN:
        case FanVacState::FAULT_VAC:
            break;
    }
    if (fanFault)
    {
        return InhibitReason::FanFault;
    }
    if (vacFault)
    {
        return InhibitReason::VacFault;
    }
    return (FanVacState::FAULT_FAN == state) ? InhibitReason::FanFault : InhibitReason::VacFault;
}

inline InhibitReason classify_inhibit(FanVacState state, bool fanFault, bool vacFault, bool outputBoardFault)
{
    return outputBoardFault ? InhibitReason::OutputBoardMissing :
                              classify_inhibit(state, fanFault, vacFault);
}

inline InhibitReason classify_inhibit(const FanVacStatus &status)
{
    return classify_inhibit(status.state, status.fanFault, status.vacFault);
}

inline InhibitReason classify_inhibit(const FanVacStatus &status, bool outputBoardFault)
{
    return classify_inhibit(status.state, status.fanFault, status.vacFault, outputBoardFault);
}

// Instruction text (Str_*Instruction is 240 px wide, 8 px per character).
inline const char *lockout_message(InhibitReason reason)
{
    switch (reason)
    {
        case InhibitReason::Idle:
            return "LOCKED: FAN/VAC NOT RUNNING";
        case InhibitReason::FanFault:
        case InhibitReason::VacFault:
            return "LOCKED: FAN/VAC FAULT";
        case InhibitReason::OutputBoardMissing:
            return "LOCKED: OUTPUT BOARD MISSING";
        case InhibitReason::None:
            break;
    }
    return nullptr;
}

// Reason text (Str_*LockReason is 240 px wide, 8 px per character).
// An empty reason returns a single space, which clears the field.
inline const char *lockout_reason_text(InhibitReason reason)
{
    switch (reason)
    {
        case InhibitReason::Idle:
            return "REASON: FAN/VAC IDLE";
        case InhibitReason::FanFault:
            return "REASON: FAN FAULT";
        case InhibitReason::VacFault:
            return "REASON: VAC FAULT";
        case InhibitReason::OutputBoardMissing:
            return "REASON: OUTPUT BOARD MISSING";
        case InhibitReason::None:
            break;
    }
    return " ";
}

// Short name for the serial log.
inline const char *lockout_reason_log_name(InhibitReason reason)
{
    switch (reason)
    {
        case InhibitReason::Idle:
            return "IDLE (fan/vac not running)";
        case InhibitReason::FanFault:
            return "FAN FAULT";
        case InhibitReason::VacFault:
            return "VAC FAULT";
        case InhibitReason::OutputBoardMissing:
            return "OUTPUT BOARD MISSING";
        case InhibitReason::None:
            break;
    }
    return "NONE (fan/vac running)";
}

// Text restored on a screen once its lockout message is cleared.
static constexpr const char *LOCKOUT_RESTORE_TEXT = "HYDRAULICS SAFE - IDLE";

// Tracks the current inhibit reason and which screens are showing a lockout.
class LockoutFeedback
{
public:
    struct Update
    {
        bool reasonChanged; // Log exactly once when this is true
        bool refreshUnfold; // Rewrite the screen's lockout text (reason changed while shown)
        bool refreshFold;
        bool restoreUnfold; // Lockout ended: restore the normal text
        bool restoreFold;
    };

    InhibitReason reason() const
    {
        return currentReason;
    }

    bool shown(ActionGroup group) const
    {
        return shownFlag[index_of(group)];
    }

    // An action press was ignored because of the inhibit. Returns false (and
    // shows nothing) if the reason is None.
    bool on_blocked_press(ActionGroup group, InhibitReason reason)
    {
        if (InhibitReason::None == reason)
        {
            return false;
        }
        shownFlag[index_of(group)] = true;
        return true;
    }

    // Call once per main-loop iteration with the current reason.
    Update update(InhibitReason reason)
    {
        Update result = { reason != currentReason, false, false, false, false };
        currentReason = reason;
        const bool uShown = shownFlag[index_of(ActionGroup::Unfold)];
        const bool fShown = shownFlag[index_of(ActionGroup::Fold)];
        if (InhibitReason::None == reason)
        {
            result.restoreUnfold = uShown;
            result.restoreFold = fShown;
            shownFlag[index_of(ActionGroup::Unfold)] = false;
            shownFlag[index_of(ActionGroup::Fold)] = false;
        }
        else if (InhibitReason::OutputBoardMissing == reason)
        {
            result.refreshUnfold = !uShown || result.reasonChanged;
            result.refreshFold = !fShown || result.reasonChanged;
            shownFlag[index_of(ActionGroup::Unfold)] = true;
            shownFlag[index_of(ActionGroup::Fold)] = true;
        }
        else if (result.reasonChanged)
        {
            result.refreshUnfold = uShown;
            result.refreshFold = fShown;
        }
        return result;
    }

private:
    static std::size_t index_of(ActionGroup group)
    {
        return (ActionGroup::Unfold == group) ? 0 : 1;
    }

    InhibitReason currentReason = InhibitReason::None;
    bool shownFlag[2] = { false, false };
};
} // namespace ui
