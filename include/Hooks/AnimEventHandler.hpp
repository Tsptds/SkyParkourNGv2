#include "References.h"
#include "Parkouring.h"
#include "Util/ParkourUtility.h"

namespace Hooks {
    RE::NiPoint3 currentTargetPos;
    template <class T>
    class AnimationEventHook : public T {
        public:
            using Fn_t = decltype(&T::ProcessEvent);
            static inline REL::Relocation<Fn_t> _ProcessEvent;  // 01
            inline RE::BSEventNotifyControl Hook(const RE::BSAnimationGraphEvent* a_event,
                                                 RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) {
                if (!a_event || !ModSettings::Mod_Enabled) {
                    return _ProcessEvent(this, a_event, a_eventSource);
                }

                auto actor = a_event->holder;
                if (!actor || !actor->IsPlayerRef()) {
                    return _ProcessEvent(this, a_event, a_eventSource);
                }

                if (a_event->tag == "GetUpExit") {
                    /* Reset vars on ragdoll exit */
                    RuntimeMethods::ResetRuntimeVariables();
                }

                if (RuntimeVariables::ParkourInProgress) {
                    //logger::info(">> AnimEvent: {} Payload: {}", a_event->tag.c_str(), a_event->payload.c_str());

                    if (a_event->tag == SPPF_MOVE) {
                        if (!a_event->payload.empty()) {
                            const auto player = GET_PLAYER;
                            player->GetCharController()->SetLinearVelocityImpl(ZERO_VECTOR);

                            const ParsedPayload parsed = ParsePayload(a_event->payload.c_str());

                            constexpr bool isRelative = true;
                            currentTargetPos = Parkouring::InterpolateRefToPosition(player, parsed.position, parsed.sec, isRelative);
                        }
                        RuntimeVariables::ClipMoveIndex += 1;
                    }
                    if (a_event->tag == SPPF_START) {
                        Parkouring::OnStartStop(IS_START);
                        RuntimeVariables::ClipMoveIndex = 0;
                    }
                    if (a_event->tag == SPPF_RECOVERY) {
                        RuntimeVariables::RecoveryFramesActive = true;

                        /* If not close to ground on recovery window, end early */
                        /* TODO: Maybe move this to graph, check dist and fire stop event */
                        const auto player = GET_PLAYER;

                        const RE::NiPoint3 start{player->GetPosition()};
                        constexpr RE::NiPoint3 dir{0, 0, -1};
                        constexpr float dist = 50.0f;
                        constexpr COL_LAYER_EXTEND mask{COL_LAYER_EXTEND::kClimbLedge};

                        if (!ParkourUtility::RayCast(start, dir, dist, mask).didHit) {
                            player->NotifyAnimationGraph(SPPF_STOP);
                        }
                    }
                    else if (a_event->tag == SPPF_STOP) {
                        Parkouring::OnStartStop(IS_STOP);
                    }
                }
                return _ProcessEvent(this, a_event, a_eventSource);
            }

            static bool InstallAnimEventHook() {
                // This is the Event notify hook, equivalent of an event sink. Event will go regardless. Don't return anything in this except the OG func.
                auto vtbl = REL::Relocation<std::uintptr_t>(RE::VTABLE_BSAnimationGraphManager[0]);
                constexpr std::size_t idx = 0x1;
                _ProcessEvent = vtbl.write_vfunc(idx, &Hook);

                if (!_ProcessEvent.address()) {
                    logger::critical("AnimEvent Hook Not Installed");
                    return false;
                }
                return true;
            }

        private:
            struct ParsedPayload {
                    RE::NiPoint3 position;
                    float sec;
            };

            static float parse_next(const char*& ptr, char delim) {
                char* end;
                float v = std::strtof(ptr, &end);
                if (end == ptr) {
                    return 0;
                }
                ptr = end;
                if (*ptr == delim)
                    ++ptr;
                return v;
            }

            ParsedPayload ParsePayload(const char* payload) {
                /* Expected anim event - payload format: SkyParkour_Move.x|y|z@s */
                const char* ptr = payload;

                float x = parse_next(ptr, '|');
                float y = parse_next(ptr, '|');
                float z = parse_next(ptr, '@');
                float sec = parse_next(ptr, '\0');

                return ParsedPayload(RE::NiPoint3(x, y, z), sec);
            }
    };

    class NotifyGraphHandler {
        public:
            static bool InstallGraphNotifyHook();

        private:
            // Our hook callbacks
            static bool OnTESObjectREFR(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);
            static bool OnCharacter(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);
            static bool OnPlayerCharacter(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);

            // Originals
            static inline REL::Relocation<decltype(OnTESObjectREFR)> _origTESObjectREFR;
            static inline REL::Relocation<decltype(OnCharacter)> _origCharacter;
            static inline REL::Relocation<decltype(OnPlayerCharacter)> _origPlayerCharacter;
    };
}  // namespace Hooks

bool Hooks::NotifyGraphHandler::InstallGraphNotifyHook() {
    // TESObjectREFR
    //REL::Relocation<uintptr_t> vtblTES{RE::VTABLE_TESObjectREFR[3]};
    //_origTESObjectREFR = vtblTES.write_vfunc(0x1, OnTESObjectREFR);

    // Character
    //REL::Relocation<uintptr_t> vtblChar{RE::VTABLE_Character[3]};
    //_origCharacter = vtblChar.write_vfunc(0x1, OnCharacter);

    // PlayerCharacter
    REL::Relocation<uintptr_t> vtblPlayer{RE::VTABLE_PlayerCharacter[3]};
    _origPlayerCharacter = vtblPlayer.write_vfunc(0x1, OnPlayerCharacter);

    if (!_origPlayerCharacter.address()) {
        logger::critical("Notify Hook Not Installed");
        return false;
    }
    return true;
}

bool Hooks::NotifyGraphHandler::OnTESObjectREFR(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName) {
    // pre‑hook logic...
    bool result = _origTESObjectREFR(a_this, a_eventName);
    // post‑hook logic...
    logger::info(">> Object Anim Event: {}", a_eventName.c_str());
    return result;
}

bool Hooks::NotifyGraphHandler::OnCharacter(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName) {
    bool result = _origCharacter(a_this, a_eventName);
    logger::info(">> Char Anim Event: {}", a_eventName.c_str());
    return result;
}

bool Hooks::NotifyGraphHandler::OnPlayerCharacter(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName) {
    if (a_eventName == SPPF_NOTIFY &&
        (!RuntimeVariables::IsParkourActive || (RuntimeVariables::ParkourInProgress && !RuntimeVariables::EnableNotifyWindow))) {
        return false;
    }

    else if (a_eventName == "Ragdoll") {
        if (RuntimeVariables::ParkourInProgress) {
            /*Unlock controls on ragdoll*/

            bool didRagdoll = _origPlayerCharacter(a_this, a_eventName);
            if (didRagdoll) {
                Parkouring::OnStartStop(IS_STOP);
            }
            return didRagdoll;
        }
    }
    else if (a_eventName == SPPF_STOP) {
        Parkouring::OnStartStop(IS_STOP);
    }
    return _origPlayerCharacter(a_this, a_eventName);
}