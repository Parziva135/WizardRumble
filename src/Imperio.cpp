#include "WizardRumble.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/PriWrapper.h"

namespace {
bool ImperioLive(CarWrapper car) {
    return car && !car.GetbHidden() && !car.GetbDeleteMe();
}
}

void WizardRumble::ReleaseImperio(std::uintptr_t target, bool notify) {
    const auto found = imperios_.find(target);
    if (found == imperios_.end()) return;
    const auto state = found->second;
    // Remove ownership before recording/canceling cosmetics. Cleanup never
    // writes either actor's pose or replaces their native input permanently.
    imperios_.erase(found);
    CancelScene(wizard::Spell::Imperio, state.caster, state.target, notify,
        state.casterCar, state.car);
}

void WizardRumble::UpdateImperios(ServerWrapper server) {
    std::vector<std::uintptr_t> targets;
    targets.reserve(imperios_.size());
    for (const auto& entry : imperios_) targets.push_back(entry.first);
    const bool notify = Authoritative(server);
    for (const auto targetKey : targets) {
        const auto found = imperios_.find(targetKey);
        if (found == imperios_.end()) continue;
        const auto state = found->second;
        auto target = FindPlayerByAddress(server, state.pri);
        auto caster = FindPlayerByAddress(server, state.casterPri);
        auto car = target ? target.GetCar() : CarWrapper(0);
        auto source = caster ? caster.GetCar() : CarWrapper(0);
        if (gameClock_ < state.began || gameClock_ >= state.ends ||
            !target || !caster || target.IsSpectator() || caster.IsSpectator() ||
            !ImperioLive(car) || !ImperioLive(source) || targetKey != state.pri ||
            target.memory_address != state.pri || caster.memory_address != state.casterPri ||
            car.memory_address != state.car || source.memory_address != state.casterCar ||
            target.GetTeamNum() == caster.GetTeamNum())
            ReleaseImperio(targetKey, notify);
    }
}

bool WizardRumble::ApplyImperioInput(ServerWrapper server, CarWrapper car, ControllerInput& input) {
    if (goalReplay_ || !server || !Active(server) || !ImperioLive(car)) return false;
    const bool host = Authoritative(server);
    if (host ? Setting("wr_enabled", 1) < 0.5f : !hostEnabled_) return false;
    if (!host) {
        auto local = LocalPlayer();
        auto localCar = local ? local.GetCar() : CarWrapper(0);
        if (!local || local.IsSpectator() || !ImperioLive(localCar) ||
            localCar.memory_address != car.memory_address) return false;
    }
    auto target = car.GetPRI();
    if (!target) return false;
    const auto found = imperios_.find(target.memory_address);
    if (found == imperios_.end()) return false;
    const auto state = found->second;
    auto currentTarget = FindPlayerByAddress(server, state.pri);
    auto currentCar = currentTarget ? currentTarget.GetCar() : CarWrapper(0);
    auto caster = FindPlayerByAddress(server, state.casterPri);
    auto source = caster ? caster.GetCar() : CarWrapper(0);
    if (gameClock_ < state.began || gameClock_ >= state.ends ||
        target.IsSpectator() || !currentTarget || currentTarget.IsSpectator() ||
        !caster || caster.IsSpectator() || !ImperioLive(currentCar) ||
        !ImperioLive(source) || currentTarget.memory_address != state.pri ||
        currentCar.memory_address != state.car || target.memory_address != state.pri ||
        car.memory_address != state.car || caster.memory_address != state.casterPri ||
        source.memory_address != state.casterCar || target.GetTeamNum() == caster.GetTeamNum()) {
        ReleaseImperio(target.memory_address, host);
        return false;
    }
    input = wizard::MirrorControls(source.GetInput());
    return true;
}
