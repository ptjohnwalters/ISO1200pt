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

// ─── BULK FILL (FAN) BUTTONS ─────────────────────────────────────────────────
#define Button_FanSpeedUp            6040   // Increase fan target RPM
#define Button_FanSpeedDown          6041   // Decrease fan target RPM
#define Button_FanPower              6042   // Toggle Fan On/Off

// ─── VACUUM BUTTONS ──────────────────────────────────────────────────────────
#define Button_VacPressureUp         6050   // Increase vac target pressure
#define Button_VacPressureDown       6051   // Decrease vac target pressure
#define Button_VacPower              6052   // Toggle Vac On/Off

// ─── MARKER CONTROL BUTTONS ──────────────────────────────────────────────────
#define Button_MarkerPrev            6060   // Marker cycle previous
#define Button_MarkerToggle          6061   // Marker toggle / cycle
#define Button_MarkerNext            6062   // Marker cycle next

// ─── CALIBRATION BUTTONS ─────────────────────────────────────────────────────
#define Button_CalSetTransport       6070   // Store current position as transport limit
#define Button_CalSetPlant           6071   // Store current position as plant limit
#define Button_CalZero               6072   // Zero calibration sensor

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
#define OutStr_SavedMarks            11012  // "SAVED CALIBRATION MARKS:"
#define OutStr_TransportLimit        11013  // "TRANSPORT (UP):"
#define OutStr_PlantLimit            11014  // "PLANT (DOWN):"
#define OutStr_SensorRaw             11015  // "SENSOR RAW ADC:"
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
#define OutStr_BtnVacPower           11032  // "ON"
#define OutStr_BtnFanPower           11033  // "ON"
#define OutStr_BtnNext               11034  // "NEXT"
#define OutStr_BtnPrev               11035  // "PREV"
#define OutStr_BtnCancel             11036  // "CANCEL"
#define OutStr_BtnNextStep           11037  // "NEXT STEP"
#define OutStr_BtnSetHigh            11038  // "SET TRANSPORT"
#define OutStr_BtnSetLow             11039  // "SET PLANT"
#define OutStr_BtnZero               11040  // "ZERO SENSOR"
#define OutStr_BtnLimitedLift        11041  // "LIMITED LIFT"
#define OutStr_BtnFullLift           11042  // "FULL LIFT"
#define OutStr_BtnLower              11043  // "LOWER"
#define OutStr_TransPercent          11048  // "%"
#define OutStr_PlantPercent          11049  // "%"

// ─── SOFTKEY LABELS ──────────────────────────────────────────────────────────
#define OutStr_SKRun                 11044  // "RUN"
#define OutStr_SKUnfold              11045  // "UNFOLD"
#define OutStr_SKFold                11046  // "FOLD"
#define OutStr_SKCal                 11047  // "CAL"

// ─── HYDRAULIC STEP LABELS ───────────────────────────────────────────────────
#define OutStr_UnfoldStep1           11050  // "1. Unlatch Wings & Wing Tilt"
#define OutStr_UnfoldStep2           11051  // "2. Lower Planter"
#define OutStr_UnfoldStep3           11052  // "3. Unfold Wings"
#define OutStr_UnfoldStep4           11053  // "4. Unlatch Transport Hooks"
#define OutStr_UnfoldStep5           11054  // "5. Rotate Bar"
#define OutStr_UnfoldStep6           11055  // "6. Extend Tongue"

#define OutStr_FoldStep1             11060  // "1. Retract Tongue"
#define OutStr_FoldStep2             11061  // "2. Rotate Bar"
#define OutStr_FoldStep3             11062  // "3. Latch Transport Hooks"
#define OutStr_FoldStep4             11063  // "4. Fold Wings"
#define OutStr_FoldStep5             11064  // "5. Raise Planter"
#define OutStr_FoldStep6             11065  // "6. Latch Wings & Center Bar"

// ─── VARIABLE STRINGS (Dynamic Text) ─────────────────────────────────────────
#define VarStr_FoldInstruction       22000  // Current fold step instruction text
#define VarStr_UnfoldInstruction     22001  // Current unfold step instruction text
#define VarStr_PlantStatus           22002  // Current plant mode status
#define VarStr_MarkerStatus          22010  // "MARKER: OFF", "MARKER: 1", "MARKER: 2"
#define VarStr_MarkerDetail          22011  // "1 = LEFT     2 = RIGHT     OFF"
#define VarStr_CalPositionState      22020  // "MID RANGE", "PLANT RANGE", "TRANSPORT RANGE"
#define VarStr_CalFaultMessage       22021  // "STATUS: OK - NO ACTIVE FAULTS"

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
#define VarNum_CalTransportLimit     21011  // Saved transport limit mark
#define VarNum_CalPlantLimit         21012  // Saved plant limit mark
#define VarNum_CalSensorRaw          21013  // Live sensor raw ADC counts

// ─── FONT ATTRIBUTES ─────────────────────────────────────────────────────────
#define FontAttr_Small               23000  // Small black text (16x16)
#define FontAttr_Medium              23001  // Medium black text (24x32)
#define FontAttr_Large               23002  // Large black text (32x48)
#define FontAttr_Small_White         23003  // Small white text (16x16)
#define FontAttr_Medium_White        23004  // Medium white text (24x32)
#define FontAttr_Large_White         23005  // Large white text (32x48)

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