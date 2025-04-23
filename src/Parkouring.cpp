#include "Parkouring.h"

using namespace ParkourUtility;

int Parkouring::LedgeCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float minLedgeHeight, float maxLedgeHeight) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto playerPos = player->GetPosition();

    // Constants adjusted for player scale
    const float startZOffset = 100 * RuntimeVariables::PlayerScale;
    const float playerHeight = 120 * RuntimeVariables::PlayerScale;
    const float minUpCheck = 100 * RuntimeVariables::PlayerScale;
    const float maxUpCheck = (maxLedgeHeight - startZOffset) + 20 * RuntimeVariables::PlayerScale;
    const float fwdCheckStep = 8 * RuntimeVariables::PlayerScale;
    const int fwdCheckIterations = 10;   // 15
    const float minLedgeFlatness = 0.5;  //0.5
    const float ledgeHypotenuse = 1.0;   // 0.75 - larger is more relaxed, lesser is more strict. Don't set 0

    RE::hkVector4 normalOut(0, 0, 0, 0);

    // Upward raycast to check for headroom
    RE::NiPoint3 upRayStart = playerPos + RE::NiPoint3(0, 0, startZOffset);
    RE::NiPoint3 upRayDir(0, 0, 1);

    float upRayDist = RayCast(upRayStart, upRayDir, maxUpCheck, normalOut, RE::COL_LAYER::kLOS);
    if (upRayDist < minUpCheck) {
        return ParkourType::NoLedge;
    }

    // Forward raycast initialization
    RE::NiPoint3 fwdRayStart = upRayStart + upRayDir * (upRayDist - 10);
    RE::NiPoint3 downRayDir(0, 0, -1);

    bool foundLedge = false;
    float normalZ = 0;

    // Incremental forward raycast to find a ledge
    for (int i = 0; i < fwdCheckIterations; i++) {
        float fwdRayDist = RayCast(fwdRayStart, checkDir, fwdCheckStep * i, normalOut, RE::COL_LAYER::kLOS);
        if (fwdRayDist < fwdCheckStep * i) {
            continue;
        }

        // Downward raycast to detect ledge point
        RE::NiPoint3 downRayStart = fwdRayStart + checkDir * fwdRayDist;
        float downRayDist = RayCast(downRayStart, downRayDir, startZOffset + maxUpCheck, normalOut, RE::COL_LAYER::kLOS);

        ledgePoint = downRayStart + downRayDir * downRayDist;
        normalZ = normalOut.quad.m128_f32[2];

        // Validate ledge based on height and flatness
        if (ledgePoint.z < playerPos.z + minLedgeHeight || ledgePoint.z > playerPos.z + maxLedgeHeight || downRayDist < 10 ||
            normalZ < minLedgeFlatness) {
            continue;
        }

        // Backward ray to check for obstructions behind the vaultable surface
        RE::NiPoint3 backwardRayStart = fwdRayStart + checkDir * (fwdRayDist - 2) + RE::NiPoint3(0, 0, 5);
        const float maxObstructionDistance = 10.0f * RuntimeVariables::PlayerScale;
        float backwardRayDist = RayCast(backwardRayStart, checkDir, maxObstructionDistance, normalOut, RE::COL_LAYER::kLOS);

        if (backwardRayDist > 0 && backwardRayDist < maxObstructionDistance) {
            continue;  // Obstruction behind the vaultable surface
        }

        foundLedge = true;
        break;
    }

    if (!foundLedge) {
        return ParkourType::NoLedge;
    }

    // Ensure there is sufficient headroom for the player to stand
    float headroomBuffer = 10 * RuntimeVariables::PlayerScale;
    RE::NiPoint3 headroomRayStart = ledgePoint + upRayDir * headroomBuffer;
    float headroomRayDist = RayCast(headroomRayStart, upRayDir, playerHeight - headroomBuffer, normalOut, RE::COL_LAYER::kLOS);

    if (headroomRayDist < playerHeight - headroomBuffer) {
        return ParkourType::NoLedge;
    }

    float ledgePlayerDiff = ledgePoint.z - playerPos.z;
    // TODO: Move corrections into tryactivateparkour func to reduce overhead
    if (PlayerIsGroundedOrSliding() || PlayerIsSwimming()) {
        if (ledgePlayerDiff >= HardCodedVariables::highestLedgeLimit * RuntimeVariables::PlayerScale) {
            if (ShouldReplaceMarkerWithFailed()) {
                return ParkourType::Failed;
            }
            return ParkourType::Highest;  // Highest ledge

        } else if (ledgePlayerDiff >= HardCodedVariables::highLedgeLimit * RuntimeVariables::PlayerScale) {
            if (ShouldReplaceMarkerWithFailed()) {
                return ParkourType::Failed;
            }
            return ParkourType::High;  // High ledge

        } else if (ledgePlayerDiff >= HardCodedVariables::medLedgeLimit * RuntimeVariables::PlayerScale) {
            return ParkourType::Medium;  // Medium ledge

        } else if (ledgePlayerDiff >= HardCodedVariables::lowLedgeLimit * RuntimeVariables::PlayerScale) {
            if (PlayerIsSwimming()) {
                return ParkourType::Grab;  // Grab ledge out of water, don't jump out like a frog
            }

            return ParkourType::Low;  // Low ledge

        } else if (ledgePlayerDiff >= HardCodedVariables::highStepLimit * RuntimeVariables::PlayerScale) {
            if (PlayerIsSwimming()) {
                return ParkourType::Grab;  // Grab ledge out of water, don't step out
            }

            // Additional horizontal and vertical checks for low ledge
            double horizontalDistance = sqrt(pow(ledgePoint.x - playerPos.x, 2) + pow(ledgePoint.y - playerPos.y, 2));
            double verticalDistance = abs(ledgePlayerDiff);

            if (horizontalDistance < verticalDistance * ledgeHypotenuse) {
                return ParkourType::StepHigh;  // High Step
            }

        } else {
            if (PlayerIsSwimming()) {
                return ParkourType::Grab;  // Grab ledge out of water, don't step out
            }

            // Additional horizontal and vertical checks for low ledge
            double horizontalDistance = sqrt(pow(ledgePoint.x - playerPos.x, 2) + pow(ledgePoint.y - playerPos.y, 2));
            double verticalDistance = abs(ledgePlayerDiff);

            if (!PlayerIsOnStairs() && horizontalDistance < verticalDistance * ledgeHypotenuse) {
                return ParkourType::StepLow;  // Low Step
            }
        }

    } else if (PlayerIsMidairAndNotSliding() && ledgePlayerDiff > -35 && ledgePlayerDiff <= 100 * RuntimeVariables::PlayerScale) {
        if (!PlayerIsOnStairs() && player->GetCharController()->fallTime > 0.4f) {
            return ParkourType::Grab;
        }
    }
    return ParkourType::NoLedge;
}
int Parkouring::VaultCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float vaultLength, float maxElevationIncrease,
                           float minVaultHeight, float maxVaultHeight) {
    const auto player = RE::PlayerCharacter::GetSingleton();

    if (!PlayerIsGroundedOrSliding()) {
        return ParkourType::NoLedge;
    }

    const auto playerPos = player->GetPosition();

    RE::hkVector4 normalOut(0, 0, 0, 0);

    float headHeight = 120 * RuntimeVariables::PlayerScale;

    // Forward raycast to check for a vaultable surface
    RE::NiPoint3 fwdRayStart = playerPos + RE::NiPoint3(0, 0, headHeight);
    float fwdRayDist = RayCast(fwdRayStart, checkDir, vaultLength, normalOut, RE::COL_LAYER::kLOS);

    if (RuntimeVariables::lastHitObject == RE::COL_LAYER::kTerrain || fwdRayDist < vaultLength) {
        return ParkourType::NoLedge;  // Not vaultable if terrain or insufficient distance
    }

    // Backward ray to check for obstructions behind the vaultable surface
    RE::NiPoint3 backwardRayStart = fwdRayStart + checkDir * (fwdRayDist - 2) + RE::NiPoint3(0, 0, 5);
    const float maxObstructionDistance = 100.0f * RuntimeVariables::PlayerScale;
    float backwardRayDist = RayCast(backwardRayStart, checkDir, maxObstructionDistance, normalOut, RE::COL_LAYER::kLOS);

    if (backwardRayDist > 0 && backwardRayDist < maxObstructionDistance) {
        return ParkourType::NoLedge;  // Obstruction behind the vaultable surface
    }

    // Downward raycast initialization
    int downIterations = /*static_cast<int>(std::floor(vaultLength / 5.0f))*/ 20;
    RE::NiPoint3 downRayDir(0, 0, -1);

    bool foundVaulter = false;
    float foundVaultHeight = -10000.0f;
    bool foundLanding = false;
    float foundLandingHeight = 10000.0f;

    // Incremental downward raycasts
    for (int i = 0; i < downIterations; i++) {
        float iDist = static_cast<float>(i) * 5.0f;
        RE::NiPoint3 downRayStart = playerPos + checkDir * iDist;
        downRayStart.z = fwdRayStart.z;

        float downRayDist = RayCast(downRayStart, downRayDir, headHeight + 100.0f, normalOut, RE::COL_LAYER::kLOS);
        float hitHeight = (fwdRayStart.z - downRayDist) - playerPos.z;

        // Check hit height for vaultable surfaces
        if (hitHeight > maxVaultHeight) {
            return ParkourType::NoLedge;  // Too high to vault
        } else if (hitHeight > minVaultHeight && hitHeight < maxVaultHeight) {
            if (hitHeight >= foundVaultHeight) {
                foundVaultHeight = hitHeight;
                foundLanding = false;
            }
            ledgePoint = downRayStart + downRayDir * downRayDist;
            foundVaulter = true;
        } else if (foundVaulter && hitHeight < minVaultHeight) {
            foundLandingHeight = std::min(hitHeight, foundLandingHeight);
            foundLanding = true;
        }
    }

    // Final validation for vault
    if (foundVaulter && foundLanding && foundLandingHeight < maxElevationIncrease) {
        ledgePoint.z = playerPos.z + foundVaultHeight;
        if (!PlayerIsOnStairs()) {
            return ParkourType::Vault;  // Vault successful
        }
    }

    return ParkourType::NoLedge;  // Vault failed
}

int Parkouring::GetLedgePoint(float backwardOffset = 55.0f) {
    using namespace GameReferences;
    using namespace ModSettings;

    if (!indicatorRef_Blue || !indicatorRef_Red) {
        return ParkourType::NoLedge;
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    //const auto playerPos = player->GetPosition();
    const bool isMoving = player->IsMoving();

    RE::NiPoint3 playerDirFlat = GetPlayerDirFlat(player);

    // Perform ledge or vault checks
    int selectedLedgeType = ParkourType::NoLedge;
    RE::NiPoint3 ledgePoint;

    if (isMoving || !ModSettings::Smart_Parkour_Enabled) {
        selectedLedgeType = VaultCheck(ledgePoint, playerDirFlat, 85, 70 * RuntimeVariables::PlayerScale,
                                       HardCodedVariables::vaultMinHeight * RuntimeVariables::PlayerScale,
                                       HardCodedVariables::vaultMaxHeight * RuntimeVariables::PlayerScale);
    }

    if (selectedLedgeType == ParkourType::NoLedge) {
        selectedLedgeType = LedgeCheck(ledgePoint, playerDirFlat, HardCodedVariables::climbMinHeight * RuntimeVariables::PlayerScale,
                                       HardCodedVariables::climbMaxHeight * RuntimeVariables::PlayerScale);
    }
    if (selectedLedgeType == ParkourType::NoLedge || PlayerVsObjectAngle(ledgePoint) > 80) {
        return ParkourType::NoLedge;
    }

    // Don't ever parkour into water, last check before saying this ledge is valid
    float waterLevel;
    player->GetParentCell()->GetWaterHeight(player->GetPosition(), waterLevel);  //Relative to player

    if (ledgePoint.z < waterLevel - 5) {
        return ParkourType::NoLedge;
    }

    // Choose indicator depending on stamina
    GameReferences::currentIndicatorRef = indicatorRef_Blue;  // Default to blue
    if (Enable_Stamina_Consumption && PlayerHasEnoughStamina() == false && CheckIsVaultActionFromType(selectedLedgeType) == false) {
        GameReferences::currentIndicatorRef = indicatorRef_Red;
        indicatorRef_Blue->Disable();
    } else {
        indicatorRef_Red->Disable();
    }

    // Move indicator to the correct position
    if (GameReferences::currentIndicatorRef->GetParentCell() != player->GetParentCell()) {
        GameReferences::currentIndicatorRef->MoveTo(player->AsReference());
    }

    // RE::NiPoint3 cameraDirFlat = GetCameraDirFlat();

    RE::NiPoint3 backwardAdjustment = playerDirFlat * backwardOffset * RuntimeVariables::PlayerScale;
    GameReferences::currentIndicatorRef->data.location = ledgePoint + RE::NiPoint3(0, 0, 10);  // Offset upwards slightly, 5 -> 10
    GameReferences::currentIndicatorRef->Update3DPosition(true);
    GameReferences::currentIndicatorRef->data.angle = RE::NiPoint3(0, 0, atan2(playerDirFlat.x, playerDirFlat.y));

    RuntimeVariables::backwardAdjustment = backwardAdjustment;
    RuntimeVariables::ledgePoint = ledgePoint;
    RuntimeVariables::playerDirFlat = playerDirFlat;

    return selectedLedgeType;
}
void Parkouring::AdjustPlayerPosition(int ledge) {
    const auto player = RE::PlayerCharacter::GetSingleton();

    // Select appropriate ledge marker and adjustments
    // switch takes constants so can't pass these from references.h, here's the lookup table

    float zAdjust = 0;
    float z = 0;
    //const int Highest = 8;
    //const int High = 7;
    //const int Medium = 6;
    //const int Low = 5;
    //const int StepHigh = 4;
    //const int StepLow = 3;
    //const int Vault = 2;
    //const int Grab = 1;
    //const int Failed = 0;
    //const int NoLedge = -1;

    switch (ledge) {
        case 8:  // Highest Ledge
            z = HardCodedVariables::highestLedgeElevation - 3;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            break;

        case 7:  // High ledge
            z = HardCodedVariables::highLedgeElevation - 3;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            break;

        case 6:  // Medium ledge
            z = HardCodedVariables::medLedgeElevation - 3;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            break;

        case 5:  // Low ledge
            z = HardCodedVariables::lowLedgeElevation - 3;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            break;

        case 4:  // Step High
            z = HardCodedVariables::stepHighElevation - 5;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            RuntimeVariables::backwardAdjustment =
                RuntimeVariables::playerDirFlat * 30 * RuntimeVariables::PlayerScale;  // Override backward offset
            break;

        case 3:  // Step Low
            z = HardCodedVariables::stepLowElevation - 5;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            RuntimeVariables::backwardAdjustment =
                RuntimeVariables::playerDirFlat * 30 * RuntimeVariables::PlayerScale;  // Override backward offset
            break;

        case 2:  // Vault
            z = HardCodedVariables::vaultElevation - 3;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            break;

        case 1:  // Grab (Midair or Out of Water)
            z = HardCodedVariables::grabElevation - 3;
            zAdjust = -z * RuntimeVariables::PlayerScale;
            RuntimeVariables::backwardAdjustment =
                RuntimeVariables::playerDirFlat * 40 * RuntimeVariables::PlayerScale;  // Override backward offset
            break;

        case 0:  // Failed (Low Stamina Animation)
            return;
        default:
            logger::info("!!WARNING!! POSITION WAS NOT ADJUSTED, INVALID LEDGE TYPE {}", ledge);
            return;
    }

    // Check if the player will go underwater after position adjustment, and decrease ledge player diff.

    //if (player->IsInWater() && !player->AsActorState()->IsSwimming()) {
    //    float waterLevel;
    //    RE::NiPoint3 playerPos = player->GetPosition();
    //    player->GetParentCell()->GetWaterHeight(playerPos, waterLevel);
    //    auto playerWaterDiff = playerPos.z - waterLevel;
    //    auto adjustThreshold = -50.0f * RuntimeVariables::PlayerScale;

    //    if (playerWaterDiff < adjustThreshold) {
    //        zAdjust += (abs(playerWaterDiff) - 60) * RuntimeVariables::PlayerScale;
    //        //logger::info("ledgeZ: {} threshold {} zadjust:{} diff{}", RuntimeVariables::ledgePoint.z - playerPos.z, adjustThreshold, zAdjust,playerWaterDiff);
    //    }
    //}

    const auto newPosition =
        RE::NiPoint3{RuntimeVariables::ledgePoint.x - RuntimeVariables::backwardAdjustment.x,
                     RuntimeVariables::ledgePoint.y - RuntimeVariables::backwardAdjustment.y, RuntimeVariables::ledgePoint.z + zAdjust};

    player->SetPosition(newPosition, true);
}

void Parkouring::UpdateParkourPoint() {
    if (RuntimeVariables::ParkourEndQueued) {
        if (GameReferences::currentIndicatorRef)
            GameReferences::currentIndicatorRef->Disable();
        RuntimeVariables::selectedLedgeType = -1;
        return;
    }

    // Don't shut down mod if parkour is queued, or it will allow breaking stuff, yes it has to poll this. Queue is checked above already so not doing here again.
    if (!ModSettings::ModEnabled || RuntimeVariables::IsBeastForm) {
        Parkouring::SetParkourOnOff(false);
        return;
    }
    //else {
    //    // Too many things reset this, temporarily checking here.
    //    if (!AnimEventListener::Register()) {
    //        return;
    //    }
    //}

    RuntimeVariables::PlayerScale = ScaleUtility::GetScale();
    RuntimeVariables::selectedLedgeType = GetLedgePoint();

    if (!IsParkourActive()) {
        if (GameReferences::currentIndicatorRef)
            GameReferences::currentIndicatorRef->Disable();

    } else {
        if (GameReferences::currentIndicatorRef)
            GameReferences::currentIndicatorRef->Enable(false);  // Don't reset inventory
    }
}

bool Parkouring::TryActivateParkour() {
    using namespace GameReferences;
    using namespace ModSettings;
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto LedgeToProcess = RuntimeVariables::selectedLedgeType;
    if (!IsParkourActive() || RuntimeVariables::ParkourEndQueued) {
        player->SetGraphVariableInt("SkyParkourLedge", ParkourType::NoLedge);
        return false;
    }

    const bool isMoving = player->IsMoving();
    // const bool isSprinting = player->IsSprinting();

    if (Smart_Parkour_Enabled && isMoving) {
        if (CheckIsVaultActionFromType(LedgeToProcess) == false) {
            player->SetGraphVariableInt("SkyParkourLedge", ParkourType::NoLedge);
            return false;
        }
    }

    RuntimeVariables::ParkourEndQueued = true;
    player->SetGraphVariableInt("SkyParkourLedge", LedgeToProcess);
    ToggleControlsForParkour(false);

    // I pass ledge to function, cause addtask runs on the next frame. If the ledge type changes in the next frame, adjustment will be wrong.
    // But to check player swimming state, a frame must pass. So AdjustPlayerPosition is called, then parkour runs on next frame.
    // Also, ToggleControlsForParkour switches POVs, and it can crash the game if the player camera state is not updated.
    // MEANING THIS THING SHOULD RUN ON THE NEXT FRAME
    SKSE::GetTaskInterface()->AddTask([LedgeToProcess]() { ParkourReadyRun(LedgeToProcess); });

    return true;
}
void Parkouring::ParkourReadyRun(int ledge) {
    const auto player = RE::PlayerCharacter::GetSingleton();

    // Directional jumping state fails if it triggers too early, set it to standing jump
    if (ledge == ParkourType::Grab && !PlayerIsSwimming()) {
        player->NotifyAnimationGraph("JumpStandingStart");
    }

    // Lock ledge to active one throughout the action;
    RuntimeVariables::selectedLedgeType = ledge;
    // Send Event, then check if succeeded
    player->NotifyAnimationGraph("IdleLeverPushStart");
}
void Parkouring::PostParkourStaminaDamage(RE::PlayerCharacter *player, bool isVault) {
    if (ModSettings::Enable_Stamina_Consumption) {
        float cost = ParkourUtility::CalculateParkourStamina();

        if (isVault) {
            // logger::info("cost{}", cost / 2);
            DamageActorStamina(player, cost / 2);

        } else if (PlayerHasEnoughStamina()) {
            // logger::info("cost{}", cost);
            DamageActorStamina(player, cost);
        } else {
            RE::HUDMenu::FlashMeter(RE::ActorValue::kStamina);
            player->UpdateRegenDelay(RE::ActorValue::kStamina, 2.0f);
        }
    }
}

void Parkouring::SetParkourOnOff(bool turnOn) {
    if (turnOn) {
        ButtonEventListener::Register();
        //AnimEventListener::Register();

    } else {
        ButtonEventListener::Unregister();
        //AnimEventListener::Unregister();

        ParkourUtility::ToggleControlsForParkour(true);

        RuntimeVariables::selectedLedgeType = -1;
        RuntimeVariables::ParkourEndQueued = false;

        if (GameReferences::currentIndicatorRef)
            GameReferences::currentIndicatorRef->Disable();
    }
}