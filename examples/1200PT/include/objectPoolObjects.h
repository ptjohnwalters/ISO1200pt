// Object Pool Definitions for Case IH 1200PT Custom ECU
// These IDs define every UI element that appears on the InCommand 16 display

#define UNDEFINED                    65535  // 0xFFFF

// Working Set
#define WorkingSet_1200PT            0      // Root working set object

// ─── DATA MASKS (Screens) ────────────────────────────────────────────────────
#define DataMask_Home                1000   // Home screen - mode selection
#define DataMask_Fold                1001   // Fold sequence screen
#define DataMask_Unfold              1002   // Unfold sequence screen
#define DataMask_Plant               1003   // Plant mode screen
#define DataMask_FanVac              1004   // Fan and Vac control screen
#define DataMask_Alarm               1005   // Alarm/warning screen

// ─── SOFT KEY MASKS ──────────────────────────────────────────────────────────
#define SoftKeyMask_Main             4000   // Main soft key mask
#define SoftKeyMask_Sequence         4001   // Sequence soft key mask

// ─── SOFT KEYS ───────────────────────────────────────────────────────────────
#define SoftKey_Home                 5000   // Return to home
#define SoftKey_Fold                 5001   // Go to fold screen
#define SoftKey_Unfold               5002   // Go to unfold screen
#define SoftKey_Plant                5003   // Go to plant screen
#define SoftKey_FanVac               5004   // Go to fan/vac screen

// ─── HOME SCREEN BUTTONS ─────────────────────────────────────────────────────
#define Button_GoToFold              6000   // Navigate to fold screen
#define Button_GoToUnfold            6001   // Navigate to unfold screen
#define Button_GoToPlant             6002   // Navigate to plant screen
#define Button_GoToFanVac            6003   // Navigate to fan/vac screen

// ─── FOLD SEQUENCE BUTTONS ───────────────────────────────────────────────────
#define Button_FoldNext              6010   // Advance to next fold step
#define Button_FoldPrev              6011   // Go back one fold step
#define Button_FoldCancel            6012   // Cancel fold sequence

// ─── UNFOLD SEQUENCE BUTTONS ─────────────────────────────────────────────────
#define Button_UnfoldNext            6020   // Advance to next unfold step
#define Button_UnfoldPrev            6021   // Go back one unfold step
#define Button_UnfoldCancel          6022   // Cancel unfold sequence

// ─── PLANT MODE BUTTONS ──────────────────────────────────────────────────────
#define Button_PlantLimitedLift      6030   // Activate limited lift
#define Button_PlantFullLift         6031   // Activate full lift
#define Button_PlantLower            6032   // Lower planter

// ─── FAN VAC BUTTONS ─────────────────────────────────────────────────────────
#define Button_FanSpeedUp            6040   // Increase fan target RPM
#define Button_FanSpeedDown          6041   // Decrease fan target RPM
#define Button_VacPressureUp         6050   // Increase vac target pressure
#define Button_VacPressureDown       6051   // Decrease vac target pressure

// ─── OUTPUT STRINGS (Static Labels) ──────────────────────────────────────────
#define OutStr_HomeTitle             11000  // "1200PT CONTROL"
#define OutStr_FoldTitle             11001  // "FOLD SEQUENCE"
#define OutStr_UnfoldTitle           11002  // "UNFOLD SEQUENCE"
#define OutStr_PlantTitle            11003  // "PLANT MODE"
#define OutStr_FanVacTitle           11004  // "FAN / VAC CONTROL"
#define OutStr_FoldStepLabel         11005  // "STEP:"
#define OutStr_FoldInstructLabel     11006  // "ACTION:"
#define OutStr_FanRPMLabel           11007  // "FAN RPM:"
#define OutStr_FanTargetLabel        11008  // "TARGET:"
#define OutStr_VacLabel              11009  // "VAC PRESSURE:"
#define OutStr_VacTargetLabel        11010  // "TARGET:"
#define OutStr_SBinLabel             11011  // "S BIN:"
#define OutStr_AlarmTitle            11012  // "WARNING"

// ─── VARIABLE STRINGS (Dynamic Text) ─────────────────────────────────────────
#define VarStr_FoldInstruction       22000  // Current fold step instruction text
#define VarStr_UnfoldInstruction     22001  // Current unfold step instruction text
#define VarStr_PlantStatus           22002  // Current plant mode status
#define VarStr_AlarmMessage          22003  // Current alarm message text
#define VarStr_SBinStatus            22004  // "FULL" or "EMPTY"

// ─── NUMERIC VALUES ───────────────────────────────────────────────────────────
#define OutNum_FoldStep              12000  // Current fold step number (1-5)
#define OutNum_UnfoldStep            12001  // Current unfold step number (1-5)
#define OutNum_FanRPMActual          12002  // Actual fan RPM from sensor
#define OutNum_FanRPMTarget          12003  // Target fan RPM set by operator
#define OutNum_VacActual             12004  // Actual vac pressure from sensor
#define OutNum_VacTarget             12005  // Target vac pressure set by operator

// ─── VARIABLE NUMERICS ───────────────────────────────────────────────────────
#define VarNum_FoldStep              21000  // Tracks current fold step
#define VarNum_UnfoldStep            21001  // Tracks current unfold step
#define VarNum_FanRPMTarget          21002  // Operator set fan RPM target
#define VarNum_FanRPMActual          21003  // Live fan RPM reading
#define VarNum_VacTarget             21004  // Operator set vac pressure target
#define VarNum_VacActual             21005  // Live vac pressure reading

// ─── STATUS INDICATORS ───────────────────────────────────────────────────────
#define OutPict_SBinFull             20000  // Green indicator S bin full
#define OutPict_SBinEmpty            20001  // Red indicator S bin empty
#define OutPict_FoldComplete         20002  // Green check fold complete
#define OutPict_Warning              20003  // Warning icon

// ─── FONT ATTRIBUTES ─────────────────────────────────────────────────────────
#define FontAttr_Small               23000  // Small text
#define FontAttr_Medium              23001  // Medium text
#define FontAttr_Large               23002  // Large text for important values

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