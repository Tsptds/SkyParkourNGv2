#pragma once

namespace Parkouring {
    int GetLedgePoint();
    int ClimbCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float minLedgeHeight, float maxLedgeHeight);
    int VaultCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float vaultLength, float maxElevationIncrease, float minVaultHeight,
                   float maxVaultHeight);
    void OnStartStop(bool isStop);
    bool PlaceAndShowIndicator();
    RE::NiPoint3 InterpolateRefToPosition(const RE::Actor *movingRef, RE::NiPoint3 position, float seconds, bool isRelative = false);
    void StopInterpolatingRef(const RE::Actor *actor);
    void CalculateStartingPosition(int ledgeType);

    bool TryActivateParkour();
    void UpdateParkourPoint();
    void ParkourReadyRun(int32_t ledgeType, bool isSwimming);
    void PostParkourStaminaDamage(RE::PlayerCharacter *player, bool isLowEffort, bool isSwimming);

    void SetParkourOnOff(bool turnOn);
}  // namespace Parkouring