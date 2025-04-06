#pragma once
#include "References.h"
#include "ParkourUtility.h"
#include "AnimationListener.h"
#include "ButtonListener.h"
#include "ScaleUtility.h"

namespace Parkouring {
    int LedgeCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float minLedgeHeight, float maxLedgeHeight);
    int VaultCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float vaultLength, float maxElevationIncrease, float minVaultHeight,
                   float maxVaultHeight);

    int GetLedgePoint(float backwardOffset);
    void AdjustPlayerPosition();

    bool TryActivateParkour();
    void UpdateParkourPoint();
    void ParkourReadyRun();

    void SetParkourOnOff(bool turnOn);
    void ToggleModOnOff(bool turnOn);
}  // namespace Parkouring