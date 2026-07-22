#pragma once

#include <cstdint>
#include <array>

// ─── PLANT MODE DEFINITIONS ──────────────────────────────────────────────────

#define PLANT_SOLENOID_COUNT  18

// ─── PLANT MODE STATES ───────────────────────────────────────────────────────
enum class PlantMode
{
    IDLE,           // No plant mode active - all solenoids de-energized
    LIMITED_LIFT,   // Bar raise limited - partial lift
    FULL_LIFT,      // Bar raise full - complete lift
    LOWERED         // Planter lowered to ground - planting position
};

// ─── SOLENOID STATE ARRAYS ───────────────────────────────────────────────────
// Each column = solenoid 1-18 state (true = 12V, false = 0V)
// Index 0 = solenoid 1, Index 17 = solenoid 18

// LIMITED LIFT solenoid states
// Sol 1,2,3,4,7,12,16,17,18 = 12V
// Sol 5,6,8,9,10,11,13,14,15 = 0V
extern const bool PLANT_LIMITED_LIFT_STATES[PLANT_SOLENOID_COUNT];

// FULL LIFT solenoid states
// Sol 1,2,3,4,7,12,16,17,18 = 12V
// Sol 5,6,8,9,10,11,13,14,15 = 0V
extern const bool PLANT_FULL_LIFT_STATES[PLANT_SOLENOID_COUNT];

// LOWERED solenoid states
// All solenoids de-energized when planter is lowered
extern const bool PLANT_LOWERED_STATES[PLANT_SOLENOID_COUNT];

// ─── PLANT MODE STATE TRACKING ───────────────────────────────────────────────
// Tracks current plant mode for display and logic
struct PlantStatus
{
    PlantMode   currentMode;        // Current active plant mode
    bool        sensorSBinEmpty;    // true = S bin is empty
    bool        isTransitioning;    // true = currently changing modes
};

// ─── FUNCTION DECLARATIONS ───────────────────────────────────────────────────

// Initialize plant control module
void plant_control_init();

// Get current plant mode
PlantMode plant_control_get_mode();

// Get full plant status struct
PlantStatus plant_control_get_status();

// Activate limited lift mode
void plant_control_set_limited_lift();

// Activate full lift mode
void plant_control_set_full_lift();

// Lower planter to ground position
void plant_control_set_lowered();

// Update S bin sensor status
// Called from sensor reading loop
void plant_control_update_sbin(bool isEmpty);

// Apply solenoid states for current plant mode
void plant_control_apply_current_mode();

// De-energize all solenoids immediately
void plant_control_all_off();

// Get status text for display on InCommand
const char* plant_control_get_mode_string();

// Get S bin status text for display
const char* plant_control_get_sbin_string();