#include <gtest/gtest.h>

#include "lockout_feedback.h"

#include <cstring>

namespace
{
using namespace ui;

constexpr std::size_t INSTRUCTION_MAX_CHARS = 240 / 8; // Str_*Instruction: 240 px, Fit8x12
constexpr std::size_t REASON_MAX_CHARS = 240 / 8; // Str_*LockReason: 240 px, Fit8x12

TEST(LockoutFeedbackTest, ClassifiesIdleFanFaultAndVacFault)
{
    EXPECT_EQ(InhibitReason::None, classify_inhibit(FanVacState::RUNNING, false, false));
    EXPECT_EQ(InhibitReason::Idle, classify_inhibit(FanVacState::IDLE, false, false));
    EXPECT_EQ(InhibitReason::FanFault, classify_inhibit(FanVacState::FAULT_FAN, true, false));
    EXPECT_EQ(InhibitReason::VacFault, classify_inhibit(FanVacState::FAULT_VAC, false, true));
}

TEST(LockoutFeedbackTest, FanFaultTakesPrecedenceWhenBothFlagsSet)
{
    EXPECT_EQ(InhibitReason::FanFault, classify_inhibit(FanVacState::FAULT_FAN, true, true));
    EXPECT_EQ(InhibitReason::FanFault, classify_inhibit(FanVacState::FAULT_VAC, true, true));
}

TEST(LockoutFeedbackTest, FaultStateWithoutFlagsFallsBackToState)
{
    EXPECT_EQ(InhibitReason::FanFault, classify_inhibit(FanVacState::FAULT_FAN, false, false));
    EXPECT_EQ(InhibitReason::VacFault, classify_inhibit(FanVacState::FAULT_VAC, false, false));
}

TEST(LockoutFeedbackTest, ClassifiesFromStatusStruct)
{
    FanVacStatus status = {};
    status.state = FanVacState::FAULT_VAC;
    status.vacFault = true;
    EXPECT_EQ(InhibitReason::VacFault, classify_inhibit(status));
}

TEST(LockoutFeedbackTest, MessageSelection)
{
    EXPECT_STREQ("LOCKED: FAN/VAC FAULT", lockout_message(InhibitReason::FanFault));
    EXPECT_STREQ("LOCKED: FAN/VAC FAULT", lockout_message(InhibitReason::VacFault));
    EXPECT_STREQ("LOCKED: FAN/VAC NOT RUNNING", lockout_message(InhibitReason::Idle));
    EXPECT_EQ(nullptr, lockout_message(InhibitReason::None));
}

TEST(LockoutFeedbackTest, ReasonTextDistinguishesFanVacAndIdle)
{
    EXPECT_STREQ("REASON: FAN FAULT", lockout_reason_text(InhibitReason::FanFault));
    EXPECT_STREQ("REASON: VAC FAULT", lockout_reason_text(InhibitReason::VacFault));
    EXPECT_STREQ("REASON: FAN/VAC IDLE", lockout_reason_text(InhibitReason::Idle));
    EXPECT_STREQ(" ", lockout_reason_text(InhibitReason::None));
}

TEST(LockoutFeedbackTest, TextsFitTheirObjectWidths)
{
    const InhibitReason all[] = { InhibitReason::None, InhibitReason::Idle, InhibitReason::FanFault,
                                  InhibitReason::VacFault };
    for (InhibitReason reason : all)
    {
        const char *message = lockout_message(reason);
        if (nullptr != message)
        {
            EXPECT_LE(std::strlen(message), INSTRUCTION_MAX_CHARS);
        }
        EXPECT_LE(std::strlen(lockout_reason_text(reason)), REASON_MAX_CHARS);
    }
    EXPECT_LE(std::strlen(LOCKOUT_RESTORE_TEXT), INSTRUCTION_MAX_CHARS);
}

TEST(LockoutFeedbackTest, ReasonChangeIsReportedOnceOnly)
{
    LockoutFeedback feedback;
    EXPECT_FALSE(feedback.update(InhibitReason::None).reasonChanged);
    EXPECT_TRUE(feedback.update(InhibitReason::FanFault).reasonChanged);
    for (int i = 0; i < 10; i++)
    {
        EXPECT_FALSE(feedback.update(InhibitReason::FanFault).reasonChanged);
    }
    EXPECT_TRUE(feedback.update(InhibitReason::VacFault).reasonChanged);
    EXPECT_FALSE(feedback.update(InhibitReason::VacFault).reasonChanged);
    EXPECT_TRUE(feedback.update(InhibitReason::Idle).reasonChanged);
    EXPECT_TRUE(feedback.update(InhibitReason::None).reasonChanged);
    EXPECT_FALSE(feedback.update(InhibitReason::None).reasonChanged);
}

TEST(LockoutFeedbackTest, NothingShownUntilAPressIsBlocked)
{
    LockoutFeedback feedback;
    LockoutFeedback::Update update = feedback.update(InhibitReason::Idle);
    EXPECT_FALSE(update.refreshUnfold || update.refreshFold || update.restoreUnfold || update.restoreFold);
    update = feedback.update(InhibitReason::None);
    EXPECT_FALSE(update.restoreUnfold || update.restoreFold);
}

TEST(LockoutFeedbackTest, BlockedPressShowsOnlyThatScreenAndClearsWhenInhibitEnds)
{
    LockoutFeedback feedback;
    feedback.update(InhibitReason::FanFault);
    EXPECT_TRUE(feedback.on_blocked_press(ActionGroup::Unfold, InhibitReason::FanFault));
    EXPECT_TRUE(feedback.shown(ActionGroup::Unfold));
    EXPECT_FALSE(feedback.shown(ActionGroup::Fold));

    LockoutFeedback::Update update = feedback.update(InhibitReason::FanFault);
    EXPECT_FALSE(update.restoreUnfold);

    update = feedback.update(InhibitReason::None);
    EXPECT_TRUE(update.restoreUnfold);
    EXPECT_FALSE(update.restoreFold);
    EXPECT_FALSE(feedback.shown(ActionGroup::Unfold));

    // The restore is reported once only
    update = feedback.update(InhibitReason::None);
    EXPECT_FALSE(update.restoreUnfold);
}

TEST(LockoutFeedbackTest, ReasonChangeWhileShownRefreshesTheText)
{
    LockoutFeedback feedback;
    feedback.update(InhibitReason::Idle);
    feedback.on_blocked_press(ActionGroup::Fold, InhibitReason::Idle);
    const LockoutFeedback::Update update = feedback.update(InhibitReason::VacFault);
    EXPECT_TRUE(update.reasonChanged);
    EXPECT_TRUE(update.refreshFold);
    EXPECT_FALSE(update.refreshUnfold);
    EXPECT_FALSE(update.restoreFold);
}

TEST(LockoutFeedbackTest, PressWithoutInhibitShowsNothing)
{
    LockoutFeedback feedback;
    EXPECT_FALSE(feedback.on_blocked_press(ActionGroup::Unfold, InhibitReason::None));
    EXPECT_FALSE(feedback.shown(ActionGroup::Unfold));
}

TEST(LockoutFeedbackTest, BlockedPressNeverSelectsAnActionButton)
{
    // The lockout model is independent of ActionSelection, so a blocked press
    // cannot light a button.
    ActionSelection selection;
    LockoutFeedback feedback;
    feedback.update(InhibitReason::Idle);
    feedback.on_blocked_press(ActionGroup::Unfold, InhibitReason::Idle);
    EXPECT_EQ(0, selection.active_index());
}
} // namespace
