#pragma once

#include <cstdint>
#include <array>

// ─── SOLENOID PIN DEFINITIONS ────────────────────────────────────────────────
// These map solenoid tag numbers to MCP23017 output pins
// MCP23017 #1 handles solenoids 1-16 (GPA0-GPB7)
// MCP23017 #2 handles solenoids 17-18 (GPA0-GPA1)

#define SOL_1    0    // MCP23017 #1 - GPA0 - Wing Lift
#define SOL_2    1    // MCP23017 #1 - GPA1 - Raise/Lower
#define SOL_3    2    // MCP23017 #1 - GPA2 - Raise/Lower
#define SOL_4    3    // MCP23017 #1 - GPA3 - Raise/Lower
#define SOL_5    4    // MCP23017 #1 - GPA4 - Pivot/Latch
#define SOL_6    5    // MCP23017 #1 - GPA5 - Pivot/Latch
#define SOL_7    6    // MCP23017 #1 - GPA6 - Pivot/Latch
#define SOL_8    7    // MCP23017 #1 - GPA7 - Pivot/Latch
#define SOL_9    8    // MCP23017 #1 - GPB0 - Pivot/Latch
#define SOL_10   9    // MCP23017 #1 - GPB1 - Hitch Extend/Latch
#define SOL_11   10   // MCP23017 #1 - GPB2 - Hitch Extend/Latch
#define SOL_12   11   // MCP23017 #1 - GPB3 - Hitch Extend/Latch
#define SOL_13   12   // MCP23017 #1 - GPB4 - Hitch Extend/Latch
#define SOL_14   13   // MCP23017 #1 - GPB5 - Hitch Extend/Latch
#define SOL_15   14   // MCP23017 #1 - GPB6 - Inner/Outer Marker
#define SOL_16   15   // MCP23017 #1 - GPB7 - Inner/Outer Marker
#define SOL_17   16   // MCP23017 #2 - GPA0 - Inner/Outer Marker
#define SOL_18   17   // MCP23017 #2 - GPA1 - Inner/Outer Marker

// Total solenoid count
#define SOLENOID_COUNT  18

// ─── SEQUENCE DEFINITIONS ────────────────────────────────────────────────────
// Number of steps in each sequence
#define FOLD_STEPS      5
#define UNFOLD_STEPS    5

// ─── SOLENOID STATE ARRAYS ───────────────────────────────────────────────────
// Each row = one step in the sequence
// Each column = solenoid 1-18 state (true = 12V, false = 0V)
// Index 0 = solenoid 1, Index 17 = solenoid 18

// FOLD sequence solenoid states
// Step 1 - Inner Marker BACK
// Step 2 - Extend Tongue BACK
// Step 3 - Set Tongue Lock FLOAT
// Step 4 - Rotate Bar FORWARD
// Step 5 - Set Rotate Lock FORWARD
extern const bool FOLD_STATES[FOLD_STEPS][SOLENOID_COUNT];

// UNFOLD sequence solenoid states
// Step 1 - Rotate Bar BACK
// Step 2 - Set Rotate Lock BACK
// Step 3 - Retract Tongue FORWARD
// Step 4 - Set Tongue Lock FORWARD
// Step 5 - Inner Marker FORWARD
extern const bool UNFOLD_STATES[UNFOLD_STEPS][SOLENOID_COUNT];

// ─── STEP INSTRUCTIONS ───────────────────────────────────────────────────────
// Text displayed on InCommand screen for each step
extern const char* FOLD_INSTRUCTIONS[FOLD_STEPS];
extern const char* UNFOLD_INSTRUCTIONS[UNFOLD_STEPS];

// ─── SEQUENCE STATE TRACKING ─────────────────────────────────────────────────
enum class SequenceState
{
    IDLE,           // No sequence active
    FOLD_ACTIVE,    // Fold sequence in progress
    UNFOLD_ACTIVE,  // Unfold sequence in progress
    COMPLETE        // Sequence just completed
};

// ─── FUNCTION DECLARATIONS ───────────────────────────────────────────────────

// Initialize fold sequence module
void fold_sequence_init();

// Get current sequence state
SequenceState fold_sequence_get_state();

// Get current step number (1-based for display)
uint8_t fold_sequence_get_current_step();

// Start fold sequence from step 1
void fold_sequence_start_fold();

// Start unfold sequence from step 1
void fold_sequence_start_unfold();

// Advance to next step - called when operator presses NEXT
// Returns true if sequence is complete
bool fold_sequence_next_step();

// Go back one step - called when operator presses PREV
// Returns true if successfully went back
bool fold_sequence_prev_step();

// Cancel current sequence and de-energize all solenoids
void fold_sequence_cancel();

// Apply solenoid states for current step
void fold_sequence_apply_current_step();

// De-energize all solenoids immediately
void fold_sequence_all_off();

// Get instruction text for current step
const char* fold_sequence_get_instruction();