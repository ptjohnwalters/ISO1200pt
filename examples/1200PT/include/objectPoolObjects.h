// Object Pool Definitions for Case IH 1200PT Custom ECU
// These IDs define every UI element that appears on the InCommand 16 display

#define UNDEFINED                    65535  // 0xFFFF

// Working Set
#define WorkingSet_1200PT            0      // Root working set object

// ─── DATA MASKS (Screens) ────────────────────────────────────────────────────
#define DataMask_Run                 1000   // RUN screen - main implement control
#define DataMask_Home                1000   // Alias for RUN screen
#define DataMask_Fold                1001   // Fold sequence screen
#define DataMask_Unfold              1002   // Unfold sequence screen
#define DataMask_Cal                 1003   // Calibration & diagnostics screen
#define DataMask_Plant               1003   // Alias for Plant/Cal screen
#define DataMask_FanVac              1004   // Preserved alias
#define DataMask_Alarm               1005   // Alarm/warning screen

// ─── SOFT KEY MASKS ──────────────────────────────────────────────────────────
#define SoftKeyMask_Main             4000   // Main soft key mask
#define SoftKeyMask_Sequence         4001   // Sequence soft key mask

// ─── SOFT KEYS ───────────────────────────────────────────────────────────────
#define SoftKey_Run                  5000   // Return to RUN screen
#define SoftKey_Home                 5000   // Alias for SoftKey_Run
#define SoftKey_Fold                 5001   // Go to fold screen
#define SoftKey_Unfold               5002   // Go to unfold screen
#define SoftKey_Cal                  5003   // Go to cal screen
#define SoftKey_Plant                5003   // Alias for SoftKey_Cal
#define SoftKey_FanVac               5004   // Preserved alias

// ─── NAVIGATION TAB BUTTONS ──────────────────────────────────────────────────
#define Button_TabRun                6000   // Inactive Tab: Go to RUN
#define Button_TabUnfold             6001   // Inactive Tab: Go to UNFOLD
#define Button_TabFold               6002   // Inactive Tab: Go to FOLD
#define Button_TabCal                6003   // Inactive Tab: Go to CAL
#define Button_TabRun_Active         6004   // Active Tab: RUN (red highlight)
#define Button_TabUnfold_Active      6005   // Active Tab: UNFOLD (red highlight)
#define Button_TabFold_Active        6006   // Active Tab: FOLD (red highlight)
#define Button_TabCal_Active         6007   // Active Tab: CAL (red highlight)

// Compatibility aliases
#define Button_GoToFold              6002
#define Button_GoToUnfold            6001
#define Button_GoToPlant             6003
#define Button_GoToFanVac            6000

// ─── FOLD ACTION BUTTONS ──────────────────────────────────────────────────────
#define Button_FoldAction1           6010   // Step 1: Retract Tongue
#define Button_FoldAction2           6011   // Step 2: Rotate Bar
#define Button_FoldAction3           6012   // Step 3: Latch Transport Hooks
#define Button_FoldAction4           6013   // Step 4: Fold Wings
#define Button_FoldAction5           6014   // Step 5: Raise Planter
#define Button_FoldAction6           6015   // Step 6: Latch Wings & Center Bar

// ─── UNFOLD ACTION BUTTONS ────────────────────────────────────────────────────
#define Button_UnfoldAction1         6020   // Step 1: Unlatch Wings & Wing Tilt
#define Button_UnfoldAction2         6021   // Step 2: Lower Planter
#define Button_UnfoldAction3         6022   // Step 3: Unfold Wings
#define Button_UnfoldAction4         6023   // Step 4: Unlatch Transport Hooks
#define Button_UnfoldAction5         6024   // Step 5: Rotate Bar
#define Button_UnfoldAction6         6025   // Step 6: Extend Tongue

// ─── PLANT MODE BUTTONS ──────────────────────────────────────────────────────
#define Button_PlantLimitedLift      6030   // Activate limited lift
#define Button_PlantFullLift         6031   // Activate full lift
#define Button_PlantLower            6032   // Lower planter

// ─── BULK FILL (FAN) BUTTONS ─────────────────────────────────────────────────
#define Button_FanSpeedUp            6040   // Increase fan target RPM
#define Button_FanSpeedDown          6041   // Decrease fan target RPM
#define Button_FanPower              6042   // Fan power ON button (explicit ON)
#define Button_FanPowerOff           6043   // Fan power OFF button (explicit OFF)

// ─── VACUUM BUTTONS ──────────────────────────────────────────────────────────
#define Button_VacPressureUp         6050   // Increase vac target pressure
#define Button_VacPressureDown       6051   // Decrease vac target pressure
#define Button_VacPower              6052   // Vac power ON button (explicit ON)
#define Button_VacPowerOff           6053   // Vac power OFF button (explicit OFF)

// ─── MARKER CONTROL BUTTONS ──────────────────────────────────────────────────
#define Button_MarkerPrev            6060   // Marker LEFT "<" independent toggle
#define Button_MarkerToggle          6061   // Marker AUTO toggle ("MARKER")
#define Button_MarkerNext            6062   // Marker RIGHT ">" independent toggle

// ─── CALIBRATION BUTTONS ─────────────────────────────────────────────────────
#define Button_CalSetFullRaise       6070
#define Button_CalSetLimitedRaise    6071
#define Button_CalSetStartPlant      6072
#define Button_CalSetStopPlant       6073
#define Button_CalBulkHeadland       6074   // Bulk fill headland ON/OFF toggle (stored only, not wired)

// ─── DIVIDERS ────────────────────────────────────────────────────────────────
#define Divider_Top                  7000
#define Divider_Mid1                 7001
#define Divider_Mid2                 7002
#define Divider_Fold                 7003
#define Divider_Cal1                 7004
#define Divider_Cal2                 7005

// ─── OUTPUT STRINGS (Static Labels) ──────────────────────────────────────────
#define OutStr_FoldTitle             11001  // "FOLD SEQUENCE"
#define OutStr_UnfoldTitle           11002  // "UNFOLD SEQUENCE"
#define OutStr_CalTitle              11003  // "FRAME CALIBRATION"
#define OutStr_VacSection            11004  // "VACUUM"
#define OutStr_FanSection            11005  // "BULK FILL"
#define OutStr_MarkerSection         11006  // "MARKER CONTROL"
#define OutStr_VacSetpoint           11007  // "SETPOINT:"
#define OutStr_VacActual             11008  // "ACTUAL:"
#define OutStr_VacUnit               11009  // "in H2O"
#define OutStr_FanUnit               11010  // "RPM"
#define OutStr_FramePos              11011  // "FRAME POSITION:"
#define OutStr_CalFullRaise          11012
#define OutStr_CalLimitedRaise       11013
#define OutStr_CalStartPlant         11014
#define OutStr_CalStopPlant          11015
#define OutStr_Diagnostics           11016  // "SYSTEM STATUS & FAULTS:"
#define OutStr_CalPercent            11017  // "%"
#define OutStr_FanActualUnit         11018  // "RPM"
#define OutStr_VacActualUnit         11019  // "in H2O"
#define OutStr_FanSetpoint           11028  // "SETPOINT:"
#define OutStr_FanActual             11029  // "ACTUAL:"

// ─── TAB LABELS ──────────────────────────────────────────────────────────────
#define OutStr_TabRun                11020  // "RUN"
#define OutStr_TabUnfold             11021  // "UNFOLD"
#define OutStr_TabFold               11022  // "FOLD"
#define OutStr_TabCal                11023  // "CAL"
#define OutStr_TabRun_Active         11024  // "RUN" (Active)
#define OutStr_TabUnfold_Active      11025  // "UNFOLD" (Active)
#define OutStr_TabFold_Active        11026  // "FOLD" (Active)
#define OutStr_TabCal_Active         11027  // "CAL" (Active)

// ─── BUTTON LABELS ───────────────────────────────────────────────────────────
#define OutStr_BtnPlus               11030  // "+"
#define OutStr_BtnMinus              11031  // "-"
#define OutStr_BtnVacPower           11032  // "ON" (static)
#define OutStr_BtnFanPower           11033  // "ON" (static)
#define OutStr_BtnNext               11034  // ">" (marker RIGHT)
#define OutStr_BtnPrev               11035  // "<" (marker LEFT)
#define OutStr_BtnCancel             11036  // "CANCEL"
#define OutStr_BtnNextStep           11037  // "NEXT STEP"
#define OutStr_BtnSetFullRaise       11038  // "SET"
#define OutStr_BtnSetLimitedRaise    11039  // "SET"
#define OutStr_BtnSetStartPlant      11040  // "SET"
#define OutStr_BtnLimitedLift        11041  // "LIMITED LIFT"
#define OutStr_BtnFullLift           11042  // "FULL LIFT"
#define OutStr_BtnLower              11043  // "LOWER"
#define OutStr_CalFullRaisePercent   11048  // "%"
#define OutStr_CalLimitedPercent     11049  // "%"
#define OutStr_CalStartPlantPercent  11066  // "%"
#define OutStr_CalStopPlantPercent   11067  // "%"
#define OutStr_BtnSetStopPlant       11068  // "SET"
#define OutStr_BtnVacPowerOff        11069  // "OFF"
#define OutStr_BtnFanPowerOff        11070  // "OFF"
#define OutStr_CalBulkHeadland       11071  // "BULK FILL HEADLAND"

// ─── SOFTKEY LABELS ──────────────────────────────────────────────────────────
#define OutStr_SKRun                 11044  // "RUN"
#define OutStr_SKUnfold              11045  // "UNFOLD"
#define OutStr_SKFold                11046  // "FOLD"
#define OutStr_SKCal                 11047  // "CAL"

// ─── HYDRAULIC STEP LABELS ───────────────────────────────────────────────────
#define OutStr_UnfoldStep1           11050  // "ROTATE BAR"
#define OutStr_UnfoldStep2           11051  // "SET ROTATE LOCK"
#define OutStr_UnfoldStep3           11052  // "RETRACT TONGUE"
#define OutStr_UnfoldStep4           11053  // "SET TONGUE LOCK"
#define OutStr_UnfoldStep5           11054  // "INNER MARKER"
#define OutStr_UnfoldStep6           11055  // "BAR RAISE"

#define OutStr_FoldStep1             11060  // "BAR RAISE"
#define OutStr_FoldStep2             11061  // "INNER MARKER"
#define OutStr_FoldStep3             11062  // "EXTEND TONGUE"
#define OutStr_FoldStep4             11063  // "SET TONGUE LOCK"
#define OutStr_FoldStep5             11064  // "ROTATE BAR"
#define OutStr_FoldStep6             11065  // "SET ROTATE LOCK"

// ─── VARIABLE STRINGS (Dynamic Text) ─────────────────────────────────────────
#define VarStr_FoldInstruction       22000  // Current fold step instruction text
#define VarStr_UnfoldInstruction     22001  // Current unfold step instruction text
#define VarStr_PlantStatus           22002  // Current plant mode status
#define VarStr_MarkerStatus          22010  // "MARKER" (static label of the AUTO button)
#define VarStr_MarkerDetail          22011  // "OFF", "LEFT", "RIGHT", "LEFT + RIGHT", "AUTO"
#define VarStr_CalPositionState      22020  // "MID RANGE", "PLANT RANGE", "TRANSPORT RANGE"
#define VarStr_CalFaultMessage       22021  // "STATUS: OK - NO ACTIVE FAULTS"
#define VarStr_CalBulkHeadland       22022  // Bulk fill headland button text "OFF" / "ON"
#define VarStr_UnfoldAction1         22030  // Static button text (see object_pool)
#define VarStr_UnfoldAction2         22031
#define VarStr_UnfoldAction3         22032
#define VarStr_UnfoldAction4         22033
#define VarStr_UnfoldAction5         22034
#define VarStr_UnfoldAction6         22035
#define VarStr_FoldAction1           22040  // Static button text (see object_pool)
#define VarStr_FoldAction2           22041
#define VarStr_FoldAction3           22042
#define VarStr_FoldAction4           22043
#define VarStr_FoldAction5           22044
#define VarStr_FoldAction6           22045

// ─── NUMERIC VALUES ───────────────────────────────────────────────────────────
#define OutNum_FoldStep              12000  // Current fold step number
#define OutNum_UnfoldStep            12001  // Current unfold step number
#define OutNum_FanRPMActual          12002  // Actual fan RPM
#define OutNum_FanRPMTarget          12003  // Target fan RPM
#define OutNum_VacActual             12004  // Actual vac pressure
#define OutNum_VacTarget             12005  // Target vac pressure

// ─── VARIABLE NUMERICS ───────────────────────────────────────────────────────
#define VarNum_FoldStep              21000  // Tracks current fold step
#define VarNum_UnfoldStep            21001  // Tracks current unfold step
#define VarNum_FanRPMTarget          21002  // Operator set fan RPM target
#define VarNum_FanRPMActual          21003  // Live fan RPM reading
#define VarNum_VacTarget             21004  // Operator set vac pressure target
#define VarNum_VacActual             21005  // Live vac pressure reading
#define VarNum_CalPosition           21010  // Live frame position percentage (0-100)
#define VarNum_CalFullRaise          21011
#define VarNum_CalLimitedRaise       21012
#define VarNum_CalStartPlant         21014
#define VarNum_CalStopPlant          21015

// ─── FONT ATTRIBUTES ─────────────────────────────────────────────────────────
#define FontAttr_Small               23000  // Small black text (16x16)
#define FontAttr_Medium              23001  // Medium black text (24x32)
#define FontAttr_Large               23002  // Large black text (32x48)
#define FontAttr_Small_White         23003  // Small white text (16x16)
#define FontAttr_Medium_White        23004  // Medium white text (24x32)
#define FontAttr_Large_White         23005  // Large white text (32x48)
#define FontAttr_Small_Green         23006
#define FontAttr_Small_Teal          23007
#define FontAttr_Small_Olive         23008

// Fit fonts (text must satisfy len(text) * char_width <= object width)
#define FontAttr_Fit8x12_Black       23100
#define FontAttr_Fit8x12_White       23101
#define FontAttr_Fit8x12_Green       23102
#define FontAttr_Fit8x12_Teal        23103
#define FontAttr_Fit8x12_Olive       23104
#define FontAttr_Fit12x16_Black      23105
#define FontAttr_Fit12x16_White      23106
#define FontAttr_Fit12x16_Green      23107
#define FontAttr_Fit12x16_Teal       23108
#define FontAttr_Fit12x16_Olive      23109

// Dedicated per-button fonts: colour is changed at runtime (Change Attribute,
// AID 1) so each lit/unlit button owns exactly one font object.
#define FontAttr_BtnVacOn            23110
#define FontAttr_BtnVacOff           23111
#define FontAttr_BtnFanOn            23112
#define FontAttr_BtnFanOff           23113
#define FontAttr_BtnMarkerLeft       23114  // 24x32, only for "<"
#define FontAttr_BtnMarkerAuto       23115
#define FontAttr_BtnMarkerRight      23116  // 24x32, only for ">"
#define FontAttr_BtnUnfoldAction1    23117  // ..23122 = actions 1..6
#define FontAttr_BtnFoldAction1      23123  // ..23128 = actions 1..6
#define FontAttr_BtnCalBulkHeadland  23129

// ─── LINE AND FILL ATTRIBUTES ────────────────────────────────────────────────
#define LineAttr_Black               24000  // Black border line
#define FillAttr_Black               25000  // Black fill
#define FillAttr_Green               25001  // Green fill for active states
#define FillAttr_Red                 25002  // Red fill for warnings

// ─── CONTAINERS ──────────────────────────────────────────────────────────────
#define Container_FoldStatus         3000   // Fold step display container
#define Container_FanStatus          3001   // Fan RPM display container
#define Container_VacStatus          3002   // Vac pressure display container
#define Container_SBinStatus         3003   // S bin status container