#pragma once
#include "References.h"
//namespace Hooks {
//    /* H */
//    using namespace RE;
//    hkaAnimatedReferenceFrame;
//    hkaAnimation;
//    hkaAnimationBinding;
//    hkaAnimationControl;
//    hkaAnnotationTrack;
//    hkaDefaultAnimationControl;
//    hkaSplineCompressedAnimation;
//    hkbBehaviorGraph;
//    hkbClipGenerator;
//    hkbContext;
//    hkbGenerator;
//    hkbNode;
//    hkbStateMachine;
//    hkClass;
//    hkpCharacterMovementUtil;
//    hkQsTransform;
//    hkVector4;
//
//    ///**/
//    //hkaDefaultAnimationControlListener;
//
//    ///* M */
//    //MotionDataContainer;
//
//    /* B */
//    bhkCharacterController;
//    bhkCharacterMoveInfo;
//    bhkCharacterState;
//    bhkCharacterStateClimbing;
//    bhkCharacterStateFlying;
//    bhkCharacterStateInAir;
//    bhkCharacterStateJumping;
//    bhkCharacterStateOnGround;
//    bhkCharacterStateSwimming;
//    bhkWorld;
//}  // namespace Hooks

namespace Hooks {

    float AccumTime = 0;

    class Havok {
        public:
            static bool InstallHooks();

            inline static RE::NiPoint3 Lerp(const RE::NiPoint3& a, const RE::NiPoint3& b, float alpha) {
                return a + (b - a) * alpha;
            }

        private:
            struct ClipGenerator {
                    inline static std::vector<float> TriggerTimestamps;

                    struct Install {
                            static bool Activate();
                            static bool Update();
                            static bool Deactivate();
                    };
                    struct Callback {
                            static void Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context);
                            static void Update(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context, float a_timestep);
                            static void Deactivate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context);
                    };
                    struct OG {
                            static inline REL::Relocation<decltype(Callback::Activate)> _Activate;
                            static inline REL::Relocation<decltype(Callback::Update)> _Update;
                            static inline REL::Relocation<decltype(Callback::Deactivate)> _Deactivate;
                    };
            };
    };

    bool Havok::InstallHooks() {
        bool res = false;
        res &= ClipGenerator::Install::Activate();
        res &= ClipGenerator::Install::Update();
        res &= ClipGenerator::Install::Deactivate();

        return res;
    }

    /* Install */
    bool Havok::ClipGenerator::Install::Activate() {
        REL::Relocation<uintptr_t> vtblInput{RE::VTABLE_hkbClipGenerator[0]};
        OG::_Activate = vtblInput.write_vfunc(0x4, &Callback::Activate);

        if (!OG::_Activate.address()) {
            logger::critical("ClipGenerator Activate Hook Not Installed");
            return false;
        }
        return true;
    }
    bool Havok::ClipGenerator::Install::Update() {
        REL::Relocation<uintptr_t> vtblInput{RE::VTABLE_hkbClipGenerator[0]};
        OG::_Update = vtblInput.write_vfunc(0x5, &Callback::Update);

        if (!OG::_Update.address()) {
            logger::critical("ClipGenerator Update Hook Not Installed");
            return false;
        }
        return true;
    }
    bool Havok::ClipGenerator::Install::Deactivate() {
        REL::Relocation<uintptr_t> vtblInput{RE::VTABLE_hkbClipGenerator[0]};
        OG::_Deactivate = vtblInput.write_vfunc(0x7, &Callback::Deactivate);

        if (!OG::_Deactivate.address()) {
            logger::critical("ClipGenerator Deactivate Hook Not Installed");
            return false;
        }
        return true;
    }

    /* Callbacks */
    void Havok::ClipGenerator::Callback::Activate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context) {
        /* Hook is pre-activation. Call OG to actually switch to node so triggers are available */
        OG::_Activate(a_this, a_context);
        if (!RuntimeVariables::ParkourInProgress || !TriggerTimestamps.empty()) {
            return;
        }

        if (!RuntimeVariables::IsParkourActive) {
            return;
        }

        auto clipName = a_this->animationName.c_str();
        logger::info("***{}", clipName);

        /*if (std::strcmp(clipName, "Animations\\SkyParkour\\SkyParkour_Grab.hkx") != 0) {
            return;
        }
        logger::info("TEST CLIP");*/

        // 1) Grab the trigger array
        if (auto* trigArray = a_this->triggers.get()) {
            // 2) Grab the behavior graph and its string data
            auto* graph = a_context.behavior;
            auto stringData = graph ? graph->data.get()->stringData : nullptr;

            for (auto& clipTrig: trigArray->triggers) {
                if (!clipTrig.isAnnotation)
                    continue;

                float time = clipTrig.localTime;
                std::uint32_t id = clipTrig.event.id;

                // 3) Look up the ID in the string table
                if (stringData) {
                    auto name = std::string_view(stringData->eventNames[id].c_str());
                    logger::info("Annotation @ {:.6f}s / {}", time, name);

                    if (name == SPPF_MOVE) {
                        TriggerTimestamps.push_back(time);
                    }
                    //else {
                    //    // fallback if string data isn't available
                    //    logger::info("Annotation @ {:.6f}s / {}", time, id);
                    //}
                }
            }
        }
    }
    void Havok::ClipGenerator::Callback::Update(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context, float a_timestep) {
        const int idx = std::min(RuntimeVariables::ClipMoveIndex - 1, static_cast<int>(TriggerTimestamps.size()) - 1);
        logger::info("Size:{} idx:{}", TriggerTimestamps.size(), idx);
        if (RuntimeVariables::ParkourInProgress && !TriggerTimestamps.empty()) {
            const auto startPos = RuntimeVariables::PlayerStartPosition;
            const auto endPos = currentTargetPos;

            // Time range for this segment
            const float segmentStartTime = idx <= 0 ? 0 : TriggerTimestamps[idx - 1];
            const float segmentEndTime = idx <= 0 ? 0 : TriggerTimestamps[idx];
            const float deltaTime = segmentEndTime - segmentStartTime;

            // Interpolation factor for current segment
            float alpha = deltaTime == 0 ? 0 : AccumTime / deltaTime;  // normalized [0, 1]

            // Clamp to [0, 1] just to be safe
            alpha = std::clamp(alpha, 0.0f, 1.0f);

            // Interpolate position
            RE::NiPoint3 interpPos = Lerp(startPos, endPos, alpha);

            // Apply
            GET_PLAYER->GetCharController()->context.currentState = RE::hkpCharacterStateType::kInAir;
            GET_PLAYER->SetPosition(interpPos, true);
            logger::info("{} {} {}", interpPos.x, interpPos.y, interpPos.z);
            AccumTime += a_timestep;
        }

        OG::_Update(a_this, a_context, a_timestep);
    }
    void Havok::ClipGenerator::Callback::Deactivate(RE::hkbClipGenerator* a_this, const RE::hkbContext& a_context) {
        if (!RuntimeVariables::ParkourInProgress && !TriggerTimestamps.empty()) {
            for (float x: TriggerTimestamps) {
                logger::info("{:.6f}", x);
            }

            TriggerTimestamps.clear();
            RuntimeVariables::ClipMoveIndex = 0;
            AccumTime = 0;
            logger::info("Cleaned up motion array");
        }
        OG::_Deactivate(a_this, a_context);
    }

    /*void Test() {
        const auto player = GET_PLAYER;
        BSTSmartPointer<BSAnimationGraphManager> graph;
        player->GetAnimationGraphManager(graph);
        BShkbAnimationGraphPtr activeGraph = graph->graphs[graph->GetRuntimeData().activeGraph];
        activeGraph->characterInstance.animationBindingSet;
    }*/
}  // namespace Hooks