#include "Parkouring.h"
#include "References.h"
#include "Util/ParkourUtility.h"
#include "Listeners/ButtonListener.h"
#include "Listeners/MenuListener.h"
#include "Util/ScaleUtility.h"
using namespace ParkourUtility;

int Parkouring::GetLedgePoint() {
    using namespace GameReferences;
    using namespace ModSettings;

    const auto player = GET_PLAYER;
    //const auto playerPos = player->GetPosition();

    RE::NiPoint3 playerDirFlat = GetPlayerDirFlat(player);

    // Perform ledge or vault checks
    int selectedLedgeType = ParkourType::NoLedge;
    RE::NiPoint3 ledgePoint;

    constexpr int vaultLength = 120;
    constexpr int maxElevationIncrease = 80;

    selectedLedgeType = VaultCheck(ledgePoint, playerDirFlat, vaultLength, maxElevationIncrease * RuntimeVariables::PlayerScale,
                                   HardCodedVariables::vaultMinHeight * RuntimeVariables::PlayerScale,
                                   HardCodedVariables::vaultMaxHeight * RuntimeVariables::PlayerScale);

    if (selectedLedgeType == ParkourType::NoLedge) {
        selectedLedgeType = ClimbCheck(ledgePoint, playerDirFlat, HardCodedVariables::climbMinHeight * RuntimeVariables::PlayerScale,
                                       HardCodedVariables::climbMaxHeight * RuntimeVariables::PlayerScale);
    }
    if (selectedLedgeType == ParkourType::NoLedge) {
        return ParkourType::NoLedge;
    }

    // Don't ever parkour into water, last check before saying this ledge is valid
    float waterLevel;
    player->GetParentCell()->GetWaterHeight(player->GetPosition(), waterLevel);  //Relative to player

    constexpr int validWaterDepth = 10;

    if (ledgePoint.z < waterLevel - validWaterDepth) {
        return ParkourType::NoLedge;
    }

    RuntimeVariables::ledgePoint = ledgePoint;
    RuntimeVariables::playerDirFlat = playerDirFlat;

    return selectedLedgeType;
}

int Parkouring::ClimbCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float minLedgeHeight, float maxLedgeHeight) {
    const auto player = GET_PLAYER;
    const auto playerPos = player->GetPosition();

    // Constants adjusted for player scale
    const float startZOffset = 100 * RuntimeVariables::PlayerScale;
    const float playerHeight = 120 * RuntimeVariables::PlayerScale;
    const float minUpCheck = 100 * RuntimeVariables::PlayerScale;
    const float maxUpCheck = (maxLedgeHeight - startZOffset) + 20 * RuntimeVariables::PlayerScale;
    const float fwdCheckStep = 8 * RuntimeVariables::PlayerScale;
    const int fwdCheckIterations = 10;            // 15
    const float minLedgeFlatness = 0.5;           //0.5
    const float playerToLedgeHypotenuse = 0.85f;  // 0.75 - larger is more relaxed, lesser is more strict. Don't set 0

    // Raycast above player, is there enough room
    RE::NiPoint3 upRayStart = playerPos + RE::NiPoint3(0, 0, startZOffset);
    RE::NiPoint3 upRayDir(0, 0, 1);

    RayCastResult upRay = RayCast(upRayStart, upRayDir, maxUpCheck, COL_LAYER_EXTEND::kClimbObstruction);

    if (upRay.distance < minUpCheck) {
        return ParkourType::NoLedge;
    }

    // Forward raycast initialization
    RE::NiPoint3 fwdRayStart = upRayStart + upRayDir * (upRay.distance - 10);
    RE::NiPoint3 downRayDir(0, 0, -1);

    bool foundLedge = false;
    float normalZ = 0;

    // Incremental forward raycast to find a ledge
    for (int i = 0; i < fwdCheckIterations; i++) {
        RayCastResult fwdRay = RayCast(fwdRayStart, checkDir, fwdCheckStep * i, COL_LAYER_EXTEND::kClimbObstruction);

#ifdef LOG_CLIMB
        logger::info("Ledge FWD: {}", PRINT_LAYER(fwdRay.layer));
#endif

        if (fwdRay.distance < fwdCheckStep * i) {
            continue;
        }

        // Downward raycast to detect ledge point
        RE::NiPoint3 downRayStart = fwdRayStart + checkDir * fwdRay.distance;
        RayCastResult downRay = RayCast(downRayStart, downRayDir, startZOffset + maxUpCheck, COL_LAYER_EXTEND::kClimbLedge);

#ifdef LOG_CLIMB
        logger::info("Ledge Down: {}", PRINT_LAYER(downRay.layer));
#endif

        if (LAYERS_CLIMB_EXCLUDE.contains(downRay.layer)) {
            continue;
        }

        ledgePoint = downRayStart + downRayDir * downRay.distance;
        normalZ = downRay.normalOut.quad.m128_f32[2];

        // Validate ledge based on height and flatness
        if (ledgePoint.z < playerPos.z + minLedgeHeight || ledgePoint.z > playerPos.z + maxLedgeHeight || downRay.distance < 10 ||
            normalZ < minLedgeFlatness) {
            continue;
        }

        // Check for obstructions behind the Ledge point
        RE::NiPoint3 obstructionCheckStart = fwdRayStart + checkDir * (fwdRay.distance - 2) + RE::NiPoint3(0, 0, 5);
        const float minSpaceRequired = 15.0f * RuntimeVariables::PlayerScale;
        RayCastResult obsRay = RayCast(obstructionCheckStart, checkDir, minSpaceRequired, COL_LAYER_EXTEND::kClimbObstruction);

        if (obsRay.didHit && obsRay.distance < minSpaceRequired) {
            continue;  // Obstruction behind the ledge point
        }

        foundLedge = true;
        break;
    }

    if (!foundLedge) {
        return ParkourType::NoLedge;
    }

    // Ensure there is sufficient headroom for the player to stand
    float headroomBuffer = 10 * RuntimeVariables::PlayerScale;
    const float headroomPlayerDiff = playerHeight - headroomBuffer;
    RE::NiPoint3 headroomRayStart = ledgePoint + upRayDir * headroomBuffer;
    RayCastResult headroomRay = RayCast(headroomRayStart, upRayDir, headroomPlayerDiff, COL_LAYER_EXTEND::kClimbObstruction);

    if (headroomRay.distance < headroomPlayerDiff) {
        return ParkourType::NoLedge;
    }

    const float ledgePlayerDiff = ledgePoint.z - playerPos.z;
    if (IsSupportGroundedOrSliding(player) || PlayerIsSwimming()) {
        if (ledgePlayerDiff >= HardCodedVariables::highestLedgeLimit * RuntimeVariables::PlayerScale) {
            if (ShouldReplaceMarkerWithFailed()) {
                return ParkourType::Failed;
            }
            return ParkourType::Highest;  // Highest ledge
        }
        else if (ledgePlayerDiff >= HardCodedVariables::highLedgeLimit * RuntimeVariables::PlayerScale) {
            if (ShouldReplaceMarkerWithFailed()) {
                return ParkourType::Failed;
            }
            return ParkourType::High;  // High ledge
        }
        else if (ledgePlayerDiff >= HardCodedVariables::medLedgeLimit * RuntimeVariables::PlayerScale) {
            return ParkourType::Medium;  // Medium ledge
        }
        else if (ledgePlayerDiff >= HardCodedVariables::lowLedgeLimit * RuntimeVariables::PlayerScale) {
            if (PlayerIsSwimming()) {
                return ParkourType::Grab;  // Grab ledge out of water
            }

            return ParkourType::Low;  // Low ledge
        }
        else if (ledgePlayerDiff >= HardCodedVariables::highStepLimit * RuntimeVariables::PlayerScale) {
            if (PlayerIsSwimming()) {
                return ParkourType::Grab;  // Grab ledge out of water
            }

            if (StepsExtraChecks(player, ledgePoint, playerPos, ledgePlayerDiff, playerToLedgeHypotenuse)) {
                return ParkourType::StepHigh;  // High Step
            }
        }
        else {
            if (PlayerIsSwimming()) {
                return ParkourType::Grab;  // Grab ledge out of water, don't step out
            }

            if (!PlayerIsOnStairs() && StepsExtraChecks(player, ledgePoint, playerPos, ledgePlayerDiff, playerToLedgeHypotenuse)) {
                return ParkourType::StepLow;  // Low Step
            }
        }
    }
    else if (IsSupportUnsupported(player) && ledgePlayerDiff > HardCodedVariables::grabPlayerAboveLedgeMaxDiff &&
             ledgePlayerDiff <= HardCodedVariables::grabPlayerBelowLedgeMaxDiff * RuntimeVariables::PlayerScale) {
        return ParkourType::Grab;
    }
    return ParkourType::NoLedge;
}
int Parkouring::VaultCheck(RE::NiPoint3 &ledgePoint, RE::NiPoint3 checkDir, float vaultLength, float maxElevationIncrease,
                           float minVaultHeight, float maxVaultHeight) {
    const auto player = GET_PLAYER;

    if (!IsSupportGrounded(player)) {
        return ParkourType::NoLedge;
    }

    if (!VaultExtraChecks(player)) {
        return ParkourType::NoLedge;
    }

    const auto playerPos = player->GetPosition();
    float headHeight = 120 * RuntimeVariables::PlayerScale;

    /* Forward raycast to check if there is an obstruction at head level in vaultLength */

    RE::NiPoint3 fwdRayStart = playerPos + RE::NiPoint3(0, 0, headHeight);
    float minSpaceRequired = 2 * vaultLength * RuntimeVariables::PlayerScale;

    RayCastResult fwdRay = RayCast(fwdRayStart, checkDir, minSpaceRequired, COL_LAYER_EXTEND::kVaultForward);
#ifdef LOG_VAULT
    logger::info("Vault FWD: {}", PRINT_LAYER(fwdRay.layer));
#endif

    if (fwdRay.didHit && fwdRay.distance < minSpaceRequired && LAYERS_VAULT_FORWARD_RAY.contains(fwdRay.layer)) {
        return ParkourType::NoLedge;  // Obstruction behind the vaultable surface
    }

    /* TODO: Raycast obstruction check from the validated ledge point, to ensure not vaulting into stuff. Account backward adjustment. */

    /* Move forward by this steps, and RayCast downwards. If a valid layer is found, mark it. */
    int downIterations = 20;
    RE::NiPoint3 downRayDir(0, 0, -1);
    RayCastResult downRay;

    bool foundVaulter = false;
    float foundVaultHeight = -10000.0f;
    bool foundLanding = false;
    float foundLandingHeight = 10000.0f;
    float vaultableGap = headHeight + 100.0f * RuntimeVariables::PlayerScale;

    // Incremental downward raycasts
    for (int i = 0; i < downIterations; i++) {
        float iDist = static_cast<float>(i) * 5.0f;
        RE::NiPoint3 downRayStart = playerPos + checkDir * iDist;
        downRayStart.z = fwdRayStart.z;

        downRay = RayCast(downRayStart, downRayDir, vaultableGap, COL_LAYER_EXTEND::kVaultDown);

        float hitHeight = (fwdRayStart.z - downRay.distance) - playerPos.z;

        // Check hit height for vaultable surfaces
        if (hitHeight > maxVaultHeight) {
            return ParkourType::NoLedge;  // Too high to vault
        }
        else if (hitHeight > minVaultHeight && hitHeight < maxVaultHeight) {
            if (hitHeight >= foundVaultHeight) {
                foundVaultHeight = hitHeight;
                foundLanding = false;
            }
            ledgePoint = downRayStart + downRayDir * downRay.distance;
            foundVaulter = true;
#ifdef LOG_VAULT
            logger::info("Vault Down: {}", PRINT_LAYER(downRay.layer));
#endif
        }
        else if (foundVaulter && hitHeight < minVaultHeight) {
            foundLandingHeight = std::min(hitHeight, foundLandingHeight);
            foundLanding = true;
        }
    }

    // Final validation for vault
    if (foundVaulter && foundLanding && foundLandingHeight < maxElevationIncrease) {
        ledgePoint.z = playerPos.z + foundVaultHeight;

        return ParkourType::Vault;
    }

    return ParkourType::NoLedge;  // Vault failed
}

void Parkouring::OnStartStop(bool isStop) {
    // IS_START true / IS_STOP false

    auto player = GET_PLAYER;
    auto controller = player->GetCharController();

    /* Set gravity on off */
    controller->gravity = isStop;

    if (isStop) {
        StopInterpolatingRef(player);
        player->As<RE::IAnimationGraphManagerHolder>()->SetGraphVariableInt(SPPF_Ledge, ParkourType::NoLedge);
        RuntimeVariables::PlayerStartPosition = player->GetPosition();
        RuntimeVariables::RecoveryFramesActive = false;
        RuntimeVariables::ParkourInProgress = false;
    }
    else {
        /* Reset Fall Damage */
        controller->fallStartHeight = player->GetPositionZ();
        ParkourUtility::StopInteractions(*player);
    }

    //controller->context.currentState = enable ? RE::hkpCharacterStateType::kInAir : RE::hkpCharacterStateType::kClimbing; /* Crashes modded setups, not needed */
    //controller->wantState = enable ? RE::hkpCharacterStateType::kInAir : RE::hkpCharacterStateType::kClimbing;

    // Toggle common controls
    auto ctrlMap = RE::ControlMap::GetSingleton();
    ctrlMap->ToggleControls(RE::ControlMap::UEFlag::kMainFour, isStop);  // Player tab menu & equip
}

bool Parkouring::PlaceAndShowIndicator() {
    const bool useIndicators = ModSettings::Use_Indicators;
    if (!useIndicators) {
        return false;
    }

    const bool enableStamina = ModSettings::Enable_Stamina_Consumption;
    const bool hasStamina = PlayerHasEnoughStamina();
    const auto ledgeType = RuntimeVariables::selectedLedgeType;

    const bool useRed = useIndicators && enableStamina && !hasStamina && !CheckActionRequiresLowEffort(ledgeType);

    SKSE::GetTaskInterface()->AddTask([useRed]() {
        auto blueRef = GameReferences::indicatorRef_Blue;
        auto redRef = GameReferences::indicatorRef_Red;
        if (!blueRef || !redRef) {
            return;
        }

        auto &currentRef = GameReferences::currentIndicatorRef;
        currentRef = useRed ? redRef : blueRef;

        if (useRed) {
            if (blueRef)
                blueRef->Disable();
        }
        else {
            if (redRef)
                redRef->Disable();
        }

        if (!currentRef) {
            return;
        }

        const auto player = GET_PLAYER;
        if (!player) {
            return;
        }

        if (currentRef->GetParentCell() != player->GetParentCell()) {
            currentRef->MoveTo(player->AsReference());
        }

        currentRef->data.location = RuntimeVariables::ledgePoint + RE::NiPoint3(0, 0, 8.0f);
        currentRef->data.angle = RE::NiPoint3(0, 0, std::atan2(RuntimeVariables::playerDirFlat.x, RuntimeVariables::playerDirFlat.y));
        currentRef->Update3DPosition(true);

        if (RuntimeVariables::IsParkourActive) {
            currentRef->Enable(false);
        }
        else {
            currentRef->Disable();
        }
    });

    return true;
}

RE::NiPoint3 Parkouring::InterpolateRefToPosition(const RE::Actor *movingRef, RE::NiPoint3 position, float, bool isRelative) {
    /* Calculate speed from cur pos to target dist / time. BUT Read annotations relative to start position. */
    auto curPos = movingRef->GetPosition();
    RE::NiPoint3 relativeTranslatedToWorld = position;

    if (isRelative) {
        const auto facing = RuntimeVariables::playerDirFlat;
        // playerDirFlat = forward vector (x,y,0), normalized
        const float rightX = facing.y;
        const float rightY = -facing.x;

        const auto startPos = RuntimeVariables::PlayerStartPosition;

        const float worldX =
            startPos.x + position.x * rightX * RuntimeVariables::PlayerScale + position.y * facing.x * RuntimeVariables::PlayerScale;
        const float worldY =
            startPos.y + position.x * rightY * RuntimeVariables::PlayerScale + position.y * facing.y * RuntimeVariables::PlayerScale;
        const float worldZ = startPos.z + position.z * RuntimeVariables::PlayerScale;

        relativeTranslatedToWorld = RE::NiPoint3{worldX, worldY, worldZ};
    }

    return relativeTranslatedToWorld;
    //const auto diff = relativeTranslatedToWorld - curPos;

    //float mult;
    //movingRef->GetGraphVariableFloat(SPPF_SPEEDMULT, mult);
    //if (mult == 0.0f) {
    //    mult = 1.0f;
    //}

    //auto speed = seconds <= 0 ? 5000 : diff.Length() / seconds;  // Snap to pos if 0 or negative seconds
    //speed *= mult;
}
void Parkouring::StopInterpolatingRef(const RE::Actor *actor) {
    auto movingRef = actor;
    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        return;
    }
    auto policy = vm->GetObjectHandlePolicy();
    RE::VMHandle handle = policy->GetHandleForObject(movingRef->GetFormType(), movingRef);
    if (handle == policy->EmptyHandle()) {
        return;
    }

    RE::BSFixedString scriptName = "ObjectReference";
    RE::BSFixedString functionName = "StopTranslation";

    RE::BSTSmartPointer<RE::BSScript::Object> object;
    if (!vm->FindBoundObject(handle, scriptName.c_str(), object)) {
        return;
    }

    auto args = RE::MakeFunctionArguments();

    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
    vm->DispatchMethodCall1(object,  // the Papyrus ObjectReference instance
                            functionName,
                            args,  // packed arguments
                            result);
}

void Parkouring::CalculateStartingPosition(int ledgeType) {
    const auto player = GET_PLAYER;
    float zAdjust = 0;
    float z = 0;
    float backOffset = 55.0f;
    RE::NiPoint3 backwardAdjustment;

    switch (ledgeType) {
        case 8:  // Highest Ledge
            z = HardCodedVariables::highestLedgeElevation - 5;
            break;

        case 7:  // High ledge
            z = HardCodedVariables::highLedgeElevation - 5;
            break;

        case 6:  // Medium ledge
            z = HardCodedVariables::medLedgeElevation - 5;
            break;

        case 5:  // Low ledge
            z = HardCodedVariables::lowLedgeElevation - 5;
            break;

        case 4:  // Step High
            z = HardCodedVariables::stepHighElevation - 5;
            backOffset = 15;  // Override backward offset
            break;

        case 3:  // Step Low
            z = HardCodedVariables::stepLowElevation - 5;
            backOffset = 15;  // Override backward offset
            break;

        case 2:  // Vault
            z = HardCodedVariables::vaultElevation - 5;
            break;

        case 1:  // Grab (Midair or Out of Water)
            z = HardCodedVariables::grabElevation - 5;
            backOffset = 40;  // Override backward offset
            break;

        case 0:  // Failed (Low Stamina Animation)
            break;
        default:
            logger::error(" >> START POSITION NOT SET, INVALID LEDGE TYPE {} <<", ledgeType);
            return;
    }

    zAdjust = -z * RuntimeVariables::PlayerScale;
    backwardAdjustment = RuntimeVariables::PlayerScale * RuntimeVariables::playerDirFlat * backOffset;

    RuntimeVariables::PlayerStartPosition =
        RE::NiPoint3{RuntimeVariables::ledgePoint.x - backwardAdjustment.x, RuntimeVariables::ledgePoint.y - backwardAdjustment.y,
                     ledgeType == ParkourType::Failed ? player->GetPositionZ() : RuntimeVariables::ledgePoint.z + zAdjust};
}

void Parkouring::UpdateParkourPoint() {
    if (RuntimeVariables::ParkourInProgress) {
        if (GameReferences::currentIndicatorRef)
            GameReferences::currentIndicatorRef->Disable();
        RuntimeVariables::selectedLedgeType = -1;
        return;
    }

    _THREAD_POOL.enqueue([]() {
        RuntimeVariables::IsParkourActive = IsParkourActive();
        RuntimeVariables::PlayerScale = ScaleUtility::GetScale();

        if (!RuntimeVariables::ParkourInProgress) {
            /*Avoid updating the ledge if parkour already started*/
            SKSE::GetTaskInterface()->AddTask([]() { RuntimeVariables::selectedLedgeType = GetLedgePoint(); });
        }
    });

    //logger::info("{}", RE::GetSecondsSinceLastFrame());

    // Indicator stuff
    _THREAD_POOL.enqueue([]() { PlaceAndShowIndicator(); });
}

bool Parkouring::TryActivateParkour() {
    const auto player = GET_PLAYER;
    const auto LedgeTypeToProcess = RuntimeVariables::selectedLedgeType;

    if (LedgeTypeToProcess == ParkourType::NoLedge) {
        return false;
    }

    bool Ongoing;
    if (player->GetGraphVariableBool(SPPF_ONGOING, Ongoing) && Ongoing) {
        return false;
    }

    float turningDelta;
    player->GetGraphVariableFloat("TurnDelta", turningDelta);
    if (turningDelta > 50.0f) {
        return false;
    }

    if (!RuntimeVariables::IsParkourActive) {
        return false;
    }

    const bool isMoving = player->IsMoving();
    const bool lowEffort = CheckActionRequiresLowEffort(LedgeTypeToProcess);
    const bool isSwimming = PlayerIsSwimming();
    // const bool isSprinting = player->IsSprinting();

    const auto fallTime = player->GetCharController()->fallTime;
    const bool avoidOnGroundParkour = fallTime > 0.0f;
    const bool avoidMidairParkour = fallTime < 0.17f;
    //logger::info(">> Fall time: {}", fallTime);

    if (LedgeTypeToProcess != ParkourType::Grab) {
        if (avoidOnGroundParkour) {
            return false;
        }
    }
    else {
        if (avoidMidairParkour && !isSwimming) {
            return false;
        }
    }

    /* Cancel if moving, but allow movement during swimming */
    if (ModSettings::Smart_Climb && isMoving && !isSwimming) {
        if (!lowEffort) {
            return false;
        }
    }

    RuntimeVariables::ParkourInProgress = true;

    /* Also pass swimming state for stamina calculation logic */
    ParkourReadyRun(LedgeTypeToProcess, isSwimming);

    return true;
}
void Parkouring::ParkourReadyRun(int32_t ledgeType, bool isSwimming) {
    const auto player = GET_PLAYER;
    //auto dist = player->GetPosition().GetDistance(RuntimeVariables::ledgePoint);
    //logger::info("Dist: {}", dist);

    /* Another guard for multiple activations */
    if (RuntimeVariables::EnableNotifyWindow) {
        return;
    }

    RuntimeVariables::EnableNotifyWindow = true;
    player->SetGraphVariableInt(SPPF_Ledge, ledgeType);

    /* Reset ground support check, also fixes TDM pitch bug */
    player->GetCharController()->wantState = RE::hkpCharacterStateType::kInAir;

    Parkouring::CalculateStartingPosition(ledgeType);
    InterpolateRefToPosition(player, RuntimeVariables::PlayerStartPosition, 0.1f);
    _THREAD_POOL.enqueue([player, ledgeType, isSwimming] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        SKSE::GetTaskInterface()->AddTask([player, ledgeType, isSwimming] {
            player->GetCharController()->SetLinearVelocityImpl(ZERO_VECTOR);
            bool success = player->NotifyAnimationGraph(SPPF_NOTIFY);
            RuntimeVariables::EnableNotifyWindow = false;
            if (success) {
                /* Swap last leg (Step animations) */
                if (ledgeType == ParkourType::StepHigh || ledgeType == ParkourType::StepLow) {
                    RuntimeMethods::SwapLegs();
                }
                /* Steps don't consume stamina anymore */
                else {
                    const bool lowEffort = ParkourUtility::CheckActionRequiresLowEffort(ledgeType);
                    Parkouring::PostParkourStaminaDamage(player, lowEffort, isSwimming);
                }
            }
            else {
                RuntimeVariables::ParkourInProgress = false;
            }
        });
    });
}
void Parkouring::PostParkourStaminaDamage(RE::PlayerCharacter *player, bool isLowEffort, bool isSwimming) {
    if (ModSettings::Enable_Stamina_Consumption) {
        if (player->IsGodMode()) {
            return;
        }

        float cost = ParkourUtility::CalculateParkourStamina(player);

        /* If swimming, fail animation won't play. So no need to flash the bar. Just consume half the stamina cost like low effort. */
        if (isLowEffort || isSwimming) {
            // logger::info("cost{}", cost / 2);
            DamageActorStamina(player, cost / 2);
        }
        else if (PlayerHasEnoughStamina()) {
            // logger::info("cost{}", cost);
            DamageActorStamina(player, cost);
        }
        else {
            RE::HUDMenu::FlashMeter(RE::ActorValue::kStamina);
        }
        player->UpdateRegenDelay(RE::ActorValue::kStamina, 2.0f);
    }
}

void Parkouring::SetParkourOnOff(bool turnOn) {
    if (turnOn) {
        if (!ButtonEventListener::GetSingleton()->SinkRegistered) {
            ButtonEventListener::Register();
            logger::info("Processing On");
        }
    }
    else {
        if (ButtonEventListener::GetSingleton()->SinkRegistered) {
            ButtonEventListener::Unregister();
            logger::info("Processing Off");
        }

        RuntimeMethods::ResetRuntimeVariables();

        if (GameReferences::currentIndicatorRef) {
            GameReferences::currentIndicatorRef->Disable();
        }
    }
}