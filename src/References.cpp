#include "References.h"

namespace ModSettings {

    bool Use_Preset_Parkour_Key = true;
    int Preset_Parkour_Key = PARKOUR_PRESET_KEYS::kJump;
    int32_t Custom_Parkour_Key = 0;

    bool Mod_Enabled = true;
    bool Use_Indicators = true;
    float Playback_Speed = 1.15f;

    float Parkour_Delay = 0.0f;  // Set initial delay

    bool Enable_Stamina_Consumption = true;
    bool Must_Have_Stamina = true;
    float Stamina_Damage = 20.0f;

    bool Smart_Steps = true;
    bool Smart_Vault = true;
    bool Smart_Climb = true;
}  // namespace ModSettings

namespace RuntimeMethods {

    void SwapLegs() {
        RuntimeVariables::shouldUseRightStep = !RuntimeVariables::shouldUseRightStep;
        GET_PLAYER->SetGraphVariableBool(SPPF_Leg, RuntimeVariables::shouldUseRightStep);
#ifdef LOG_STEPS
        logger::info("Next Step: {}", RuntimeVariables::shouldUseRightStep ? "Right" : "Left");
#endif  // LOG_STEPS
    }

    // Things that are not handled by MCM and persistent throughout saves without being reset on game load
    void ResetRuntimeVariables() {
        RuntimeVariables::ParkourInProgress = false;
        RuntimeVariables::EnableNotifyWindow = false;
        RuntimeVariables::RecoveryFramesActive = false;
        RuntimeVariables::selectedLedgeType = ParkourType::NoLedge;
        auto player = GET_PLAYER;
        if (player) {
            player->SetGraphVariableInt(SPPF_Ledge, -1);
            player->SetGraphVariableFloat(SPPF_SPEEDMULT, ModSettings::Playback_Speed);
        }
    }
    bool CheckESPLoaded() {
        auto dh = RE::TESDataHandler::GetSingleton();
        return dh && (dh->GetSingleton()->LookupLoadedLightModByName(IniSettings::ESP_NAME) ||
                      dh->GetSingleton()->LookupLoadedModByName(IniSettings::ESP_NAME));
    }
    std::unique_ptr<CSimpleIniA> GetIniHandle() {
        constexpr const char *path = "./Data/SKSE/Plugins/SkyParkourNG.ini";

        auto ini = std::make_unique<CSimpleIniA>();
        ini->SetUnicode();

        SI_Error rc = ini->LoadFile(path);
        if (rc < 0) {
            logger::warn("SkyParkourNG.ini not found, creating with default values");

            // Set default values here
            ini->SetValue("ESP", "sEspName", "SkyParkourV2.esp");
            ini->SetValue("ESP", "iBlueMarkerRefID", "0x000014");
            ini->SetValue("ESP", "iRedMarkerRefID", "0x00000C");

            ini->SetValue("MCM", "bEnableMod", "true");
            ini->SetValue("MCM", "bSmartParkour", "true");
            ini->SetValue("MCM", "bShowIndicators", "true");

            ini->SetValue("MCM", "bEnableStaminaSystem", "true");
            ini->SetValue("MCM", "bMustHaveStamina", "true");
            ini->SetValue("MCM", "iBaseStaminaDamage", "20");

            ini->SetValue("MCM", "bUsePresetKey", "true");
            ini->SetValue("MCM", "iPresetKeyIndex", "0");
            ini->SetValue("MCM", "iCustomKeybind", "0");
            ini->SetValue("MCM", "fInputDelay", "0.0");

            if (ini->SaveFile(path) < 0) {
                logger::error("Failed to create default SkyParkourNG.ini");
                return nullptr;
            }
        }

        return ini;
    }

    bool ReadPluginConfigFromINI() {
        auto ini = GetIniHandle();
        if (!ini) {
            logger::error("INI FILE DOES NOT EXIST AND FAILED TO CREATE");
            return false;
        }

        const char *name = ini->GetValue("ESP", "sEspName");
        if (!name) {
            logger::error("EspName not found, using default name");
        }

        logger::info("ESP Name: '{}'", name);
        IniSettings::ESP_NAME = name;
        return true;
    }

    void SetupModCompatibility() {
        auto TDM = GetModuleHandleA("TrueDirectionalMovement.dll");
        if (TDM) {
            Compatibility::TrueDirectionalMovement = true;
            logger::info("Patch: True Directional Movement |360|Swim Pitch|");
        }
    }
}  // namespace RuntimeMethods

namespace Compatibility {
    bool TrueDirectionalMovement = false;
}  // namespace Compatibility

namespace HardCodedVariables {
    // Lower - upper limits for ledge - vault - grab detection.
    const float climbMaxHeight = 250.0f;
    const float climbMinHeight = 30.0f;

    const float vaultMaxHeight = 100.0f;
    const float vaultMinHeight = 40.5f;

    const float grabPlayerBelowLedgeMaxDiff = 110.0f;
    const float grabPlayerAboveLedgeMaxDiff = -35.0f;

    // These are the height ranges for parkour type selection, represent low limits.
    const float highestLedgeLimit = 220.0f;
    const float highLedgeLimit = 170.0f;
    const float medLedgeLimit = 130.0f;
    const float lowLedgeLimit = 80.0f;
    const float highStepLimit = 60.0f;

    // These are the ending heights for each animation, they are dependent on animmotion data.
    const float highestLedgeElevation = 250.0f;
    const float highLedgeElevation = 200.0f;
    const float medLedgeElevation = 153.0f;
    const float lowLedgeElevation = 110.0f;

    const float stepHighElevation = 70.0f;
    const float stepLowElevation = 50.0f;

    const float vaultElevation = 60.0f;

    const float grabElevation = 60.0f;
}  // namespace HardCodedVariables

namespace ParkourType {
    const int Highest = 8;
    const int High = 7;

    const int Medium = 6;
    const int Low = 5;
    const int StepHigh = 4;
    const int StepLow = 3;

    const int Vault = 2;

    const int Grab = 1;

    const int Failed = 0;

    const int NoLedge = -1;
}  // namespace ParkourType

namespace RuntimeVariables {
    bool IsParkourActive = true;

    float PlayerScale = 1.0f;

    int selectedLedgeType = -1;
    RE::NiPoint3 ledgePoint = {0, 0, 0};
    RE::NiPoint3 playerDirFlat = {0, 0, 0};

    RE::NiPoint3 PlayerStartPosition = {0, 0, 0};
    int ClipMoveIndex = 0;

    bool ParkourInProgress = false;
    bool EnableNotifyWindow = false;
    bool RecoveryFramesActive = false;
    bool IsMenuOpen = false;
    bool IsInMainMenu = true;

    bool shouldUseRightStep = true;
}  // namespace RuntimeVariables

namespace GameReferences {
    RE::NiPointer<RE::TESObjectREFR> indicatorRef_Blue;
    RE::NiPointer<RE::TESObjectREFR> indicatorRef_Red;

    RE::NiPointer<RE::TESObjectREFR> currentIndicatorRef;

}  // namespace GameReferences

namespace IniSettings {
    std::string ESP_NAME = "SkyParkourV2.esp";
    const char *INIPath = "./Data/SKSE/Plugins/SkyParkourNG.ini";
}  // namespace IniSettings