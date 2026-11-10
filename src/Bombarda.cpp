#include "WizardRumble.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/PriWrapper.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace {
wizard::Vec3 CoreBall(Vector value) { return {value.X, value.Y, value.Z}; }
Vector NativeBall(wizard::Vec3 value) { return {value.x, value.y, value.z}; }
bool Near(float a, float b) {
    return std::isfinite(a) && std::abs(a-b) <=
        4 * std::numeric_limits<float>::epsilon() * std::max(1.0f, std::abs(b));
}
bool ValidCap(float value) { return std::isfinite(value) && value >= 100 && value <= 100000; }
bool ValidScale(float value) { return std::isfinite(value) && value > 0 && value <= 100; }
bool ValidBounce(float value) { return std::isfinite(value) && value >= 0 && value <= 100; }
const char* TuningStatus(float value, bool valid) {
    return valid ? "valid" : value == 0 ? "default" : "invalid";
}
}

void WizardRumble::StopRicochet(ServerWrapper server, bool notify) {
    if (!ricochet_) return;
    const auto state = *ricochet_;
    ricochet_.reset();
    auto ball = server ? server.GetBall() : BallWrapper(0);
    const bool cleanupAuthority = server && server.memory_address == session_ &&
        server.HasAuthority() && !gameWrapper->IsInOnlineGame();
    if (cleanupAuthority && ball && ball.memory_address == state.ball) {
        // Restore only settings still owned by this spell. A replacement ball
        // or another plugin's changed physics must not inherit our old values.
        // A zero/unavailable replicated scale must not prevent a separately
        // applied native speed cap from being restored. NaN marks tuning that
        // this spell deliberately skipped, never a value to write to the game.
        const bool scaleApplied = std::isfinite(state.appliedSpeedScale);
        const bool speedOwned = std::isfinite(state.appliedMax) &&
            Near(ball.GetMaxLinearSpeed(), state.appliedMax) &&
            (!scaleApplied || Near(ball.GetReplicatedBallMaxLinearSpeedScale(), state.appliedSpeedScale));
        if (speedOwned) {
            if (scaleApplied) ball.SetBallMaxLinearSpeedScale(state.originalSpeedScale);
            ball.SetMaxLinearSpeed2(state.originalMax);
            ball.SetMaxLinearSpeed(state.originalMax);
            auto velocity = CoreBall(ball.GetVelocity());
            const float speed = velocity.Length();
            if (std::isfinite(speed) && speed > state.originalMax)
                ball.SetVelocity(NativeBall(velocity * (state.originalMax / speed)));
        }
        if (std::isfinite(state.appliedBounce) && Near(ball.GetReplicatedWorldBounceScale(), state.appliedBounce))
            ball.SetWorldBounceScale(state.originalBounce);
        ball.ForceNetUpdate2();
    }
    CancelScene(wizard::Spell::BombardaMaxima, state.caster, -1, notify, state.casterCar);
}

void WizardRumble::UpdateRicochet(ServerWrapper server) {
    if (!ricochet_ || !Authoritative(server)) return;
    auto ball = server.GetBall();
    if (!ball || ball.memory_address != ricochet_->ball || ball.GetbFrozen() || gameClock_ >= ricochet_->ends)
        StopRicochet(server);
}

bool WizardRumble::StartRicochet(ServerWrapper server, PriWrapper caster, wizard::Message& effect) {
    if (!Authoritative(server) || !caster) return false;
    StopRicochet(server);
    auto ball = server.GetBall();
    auto car = caster.GetCar();
    if (!ball || !car || ball.GetbFrozen()) return false;
    const float originalMax = ball.GetMaxLinearSpeed();
    const float originalScale = ball.GetReplicatedBallMaxLinearSpeedScale();
    const float originalBounce = ball.GetReplicatedWorldBounceScale();
    auto direction = (CoreBall(ball.GetLocation()) - CoreBall(car.GetLocation())).Normalized();
    if (direction.LengthSquared() < .5f) direction = CoreBall(RotatorToVector(car.GetRotation())).Normalized();
    // An upward component produces roof/floor ricochets instead of another
    // automatically goal-directed ground shot. Native collision chooses bounces.
    direction.z = std::max(direction.z, .35f);
    direction = direction.Normalized();
    if (!direction.IsFinite() || direction.LengthSquared() < .5f) return false;
    // These tuning fields are optional native metadata. Some matches expose
    // zero/default values; rejecting those after G kept the held Bombarda
    // forever without ever applying its hit. Invalid metadata must likewise
    // never reach a setter or block a valid target's ordinary velocity impulse.
    const float skipped = std::numeric_limits<float>::quiet_NaN();
    float maximum = skipped, scale = skipped, bounce = skipped;
    if (ValidCap(originalMax)) {
        maximum = std::max(originalMax, wizard::BombardaMaximaSpeed);
        if (ValidScale(originalScale)) {
            const float candidate = originalScale * maximum / originalMax;
            if (ValidScale(candidate)) {
                scale = candidate;
                ball.SetBallMaxLinearSpeedScale(scale);
            }
        }
        ball.SetMaxLinearSpeed2(maximum);
        ball.SetMaxLinearSpeed(maximum);
    }
    if (ValidBounce(originalBounce)) {
        bounce = std::max(originalBounce, 1.8f);
        ball.SetWorldBounceScale(bounce);
    }
    ricochet_ = Ricochet{caster.GetPlayerID(), car.memory_address, ball.memory_address,
        originalMax, originalScale, originalBounce, maximum, scale, bounce,
        gameClock_, gameClock_ + wizard::BombardaRicochetDurationSeconds};
    ball.SetVelocity(NativeBall(direction * wizard::BombardaMaximaSpeed));
    ball.ForceNetUpdate2();
    effect.durationMillis = static_cast<int>(wizard::BombardaRicochetDurationSeconds * 1000);
    if (Setting("wr_debug", 0) > .5f)
        cvarManager->log("Bombarda launch: max=" + std::to_string(originalMax) + "(" +
            TuningStatus(originalMax, ValidCap(originalMax)) + ")->" + std::to_string(maximum) +
            " scale=" + std::to_string(originalScale) + "(" + TuningStatus(originalScale, ValidScale(originalScale)) +
            ")->" + std::to_string(scale) + " bounce=" + std::to_string(originalBounce) + "(" +
            TuningStatus(originalBounce, ValidBounce(originalBounce)) + ")->" + std::to_string(bounce));
    return true;
}
