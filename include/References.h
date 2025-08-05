#pragma once

namespace ModSettings {
    extern bool Use_Preset_Parkour_Key;
    extern int Preset_Parkour_Key;
    extern int32_t Custom_Parkour_Key;
    extern bool Mod_Enabled;
    extern bool Use_Indicators;
    extern float Playback_Speed;
    extern float Parkour_Delay;
    extern bool Enable_Stamina_Consumption;
    extern bool Must_Have_Stamina;
    extern float Stamina_Damage;
    extern bool Smart_Steps;
    extern bool Smart_Vault;
    extern bool Smart_Climb;
}  // namespace ModSettings

namespace RuntimeMethods {
    extern void SetupModCompatibility();
    extern void SwapLegs();
    extern void ResetRuntimeVariables();
    extern bool CheckESPLoaded();
    extern std::unique_ptr<CSimpleIniA> GetIniHandle();
    extern bool ReadPluginConfigFromINI();
}  // namespace RuntimeMethods

namespace Compatibility {
    extern bool TrueDirectionalMovement;
}  // namespace Compatibility

namespace HardCodedVariables {
    // 250
    extern const float climbMaxHeight;
    // 30
    extern const float climbMinHeight;

    // 100
    extern const float vaultMaxHeight;
    // 40
    extern const float vaultMinHeight;

    // 110
    extern const float grabPlayerBelowLedgeMaxDiff;
    // -35
    extern const float grabPlayerAboveLedgeMaxDiff;

    // 220
    extern const float highestLedgeLimit;
    // 170
    extern const float highLedgeLimit;
    // 130
    extern const float medLedgeLimit;
    // 80
    extern const float lowLedgeLimit;
    // 60
    extern const float highStepLimit;

    // 250
    extern const float highestLedgeElevation;
    // 200
    extern const float highLedgeElevation;
    // 153
    extern const float medLedgeElevation;
    // 110
    extern const float lowLedgeElevation;
    // 70
    extern const float stepHighElevation;
    // 50
    extern const float stepLowElevation;
    // 60
    extern const float vaultElevation;  // This is exception, vault needs to put player further below. Elevation is 20, plus 40 adjustment
    // 60
    extern const float grabElevation;
}  // namespace HardCodedVariables

namespace ParkourType {
    // 8
    extern const int Highest;
    // 7
    extern const int High;
    // 6
    extern const int Medium;
    // 5
    extern const int Low;
    // 4
    extern int const StepHigh;
    // 3
    extern int const StepLow;
    // 2
    extern int const Vault;
    // 1
    extern int const Grab;
    // 0
    extern int const Failed;
    // -1
    extern const int NoLedge;
}  // namespace ParkourType

namespace RuntimeVariables {
    extern bool IsParkourActive;
    extern float PlayerScale;
    extern int selectedLedgeType;
    extern RE::NiPoint3 ledgePoint;
    extern RE::NiPoint3 playerDirFlat;

    extern RE::NiPoint3 PlayerStartPosition;
    extern int ClipMoveIndex;

    extern bool ParkourInProgress;
    extern bool EnableNotifyWindow;
    extern bool RecoveryFramesActive;
    extern bool IsMenuOpen;
    extern bool IsInMainMenu;

    extern bool shouldUseRightStep;
}  // namespace RuntimeVariables

namespace GameReferences {

    extern RE::NiPointer<RE::TESObjectREFR> indicatorRef_Blue;
    extern RE::NiPointer<RE::TESObjectREFR> indicatorRef_Red;
    extern RE::NiPointer<RE::TESObjectREFR> currentIndicatorRef;
}  // namespace GameReferences

namespace IniSettings {
    extern std::string ESP_NAME;
    extern const char *INIPath;
}  // namespace IniSettings