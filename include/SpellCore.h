#pragma once

// Game-independent spell rules. The adapter owns the active-match clock and
// calls these rules only on the authoritative host.
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace wizard {

enum class Spell : int {
    None = 0, Accio = 1, AvadaKedavra = 2, Leviosa = 3,
    BombardaMaxima = 4, Depulso = BombardaMaxima, Protego = 6, Stupefy = 8,
    PetrificusTotalus = 9, ArrestoMomentum = PetrificusTotalus,
    Obliviate = 13, Confundo = Obliviate, EatSlugs = 14, ExpectoPatronum = 15,
    Imperio = 16
};
constexpr int SpellCount = 11;
constexpr int MaxSpellId = 16;
constexpr int MaxPlayerId = 65535;
constexpr float BombardaMaximaSpeed = 12000.0f;
constexpr double BombardaRicochetDurationSeconds = 5.0;
constexpr float DepulsoSpeed = BombardaMaximaSpeed; // Legacy source alias for spell ID 4.
constexpr double PetrificusDurationSeconds = 2.0;
constexpr double PetrificusTotalusDurationSeconds = PetrificusDurationSeconds;
constexpr double ArrestoMomentumDurationSeconds = PetrificusDurationSeconds;
constexpr double ObliviateDurationSeconds = 5.0;
constexpr double ConfundoDurationSeconds = ObliviateDurationSeconds; // Legacy source alias for spell ID 13.
constexpr double ImperioDurationSeconds = 3.5;
constexpr double AvadaChargeSeconds = 2.0;
constexpr double LeviosaChargeSeconds = 2.0;
constexpr double EatSlugsChargeSeconds = 1.5;
constexpr double StupefyChargeSeconds = 0.65;
constexpr double PetrificusChargeSeconds = 1.5;
constexpr double ProtegoChargeSeconds = 1.0;
constexpr double BombardaChargeSeconds = 2.0;
constexpr double PatronumChargeSeconds = 3.0;
constexpr double ObliviateChargeSeconds = 0.5;
constexpr double AccioChargeSeconds = 0.5;
constexpr double ImperioChargeSeconds = 1.0;
constexpr double EatSlugsDurationSeconds = 3.75;
constexpr double SlugFlightSeconds = 0.35;
constexpr double AccioDurationSeconds = 3.0;
constexpr double AccioSettleSeconds = 0.35;
constexpr float AccioMaxSpeed = 6000.0f;
constexpr float AccioMaxAcceleration = 18000.0f;
constexpr float PatronumSpeed = 1700.0f;
constexpr float PatronumHitRadius = 85.0f;
constexpr float PatronumShotSpeed = 2400.0f;
constexpr double PatronumTimeoutSeconds = 2.5;
constexpr float LegacySpellRange = 4500.0f;
constexpr float DefaultSpellRange = 5500.0f;
constexpr float MinSpellRange = 500.0f;
constexpr float MaxSpellRange = 10000.0f;
constexpr std::array<Spell, SpellCount> ActiveSpells{{
    Spell::Accio, Spell::AvadaKedavra, Spell::Leviosa, Spell::BombardaMaxima,
    Spell::Protego, Spell::Stupefy, Spell::PetrificusTotalus,
    Spell::Obliviate, Spell::EatSlugs, Spell::ExpectoPatronum, Spell::Imperio
}};

struct SpellInfo {
    const char* identifier;
    const char* name;
    const char* description;
    std::array<int, 3> color;
};

inline bool IsCastableSpell(Spell spell) {
    return std::find(ActiveSpells.begin(), ActiveSpells.end(), spell) != ActiveSpells.end();
}
inline bool IsValidSpell(Spell spell) { return spell == Spell::None || IsCastableSpell(spell); }
inline bool IsPlayerTargetSpell(Spell spell) {
    return spell == Spell::AvadaKedavra || spell == Spell::Leviosa || spell == Spell::Stupefy ||
        spell == Spell::PetrificusTotalus || spell == Spell::Obliviate || spell == Spell::EatSlugs ||
        spell == Spell::Imperio;
}
inline bool IsBallTargetSpell(Spell spell) {
    return spell == Spell::Accio || spell == Spell::BombardaMaxima || spell == Spell::Protego || spell == Spell::ExpectoPatronum;
}
inline constexpr bool IsChargedSpell(Spell spell) {
    return spell == Spell::AvadaKedavra || spell == Spell::Leviosa || spell == Spell::EatSlugs ||
        spell == Spell::Stupefy || spell == Spell::PetrificusTotalus || spell == Spell::Protego ||
        spell == Spell::BombardaMaxima || spell == Spell::ExpectoPatronum || spell == Spell::Obliviate ||
        spell == Spell::Accio || spell == Spell::Imperio;
}
inline constexpr double SpellChargeSeconds(Spell spell) {
    return spell == Spell::AvadaKedavra ? AvadaChargeSeconds :
        spell == Spell::Leviosa ? LeviosaChargeSeconds :
        spell == Spell::EatSlugs ? EatSlugsChargeSeconds :
        spell == Spell::Stupefy ? StupefyChargeSeconds :
        spell == Spell::PetrificusTotalus ? PetrificusChargeSeconds :
        spell == Spell::Protego ? ProtegoChargeSeconds :
        spell == Spell::BombardaMaxima ? BombardaChargeSeconds :
        spell == Spell::ExpectoPatronum ? PatronumChargeSeconds :
        spell == Spell::Obliviate ? ObliviateChargeSeconds :
        spell == Spell::Accio ? AccioChargeSeconds :
        spell == Spell::Imperio ? ImperioChargeSeconds : 0.0;
}

// G/J charge events distinguish the original ball actor from opponent PRIs.
// A ball charge never borrows a player's ID, while opponent charges cannot
// use the ball's -1 sentinel or an out-of-range roster identifier.
inline bool IsValidChargeTarget(Spell spell, int targetId) {
    if (!IsChargedSpell(spell)) return false;
    return IsBallTargetSpell(spell) ? targetId == -1 :
        IsPlayerTargetSpell(spell) && targetId >= 0 && targetId <= MaxPlayerId;
}

// Keyboard and controller directional actions reach the same normalized
// engine axes, including aerial and dodge input. Buttons remain untouched.
template <typename DirectionalInput>
inline void InvertDirectionalControls(DirectionalInput& input) {
    const auto invert = [](float axis) {
        return std::isfinite(axis) ? -std::clamp(axis, -1.0f, 1.0f) : 0.0f;
    };
    input.Throttle = invert(input.Throttle);
    input.Steer = invert(input.Steer);
    input.Pitch = invert(input.Pitch);
    input.Yaw = invert(input.Yaw);
    input.Roll = invert(input.Roll);
    input.DodgeForward = invert(input.DodgeForward);
    input.DodgeStrafe = invert(input.DodgeStrafe);
}

// Imperio copies normalized control commands, not positions or actor state.
// Constructing a fresh value avoids forwarding native input padding, while
// clamping keeps corrupt/nonfinite axes out of the victim's input path.
template <typename Input>
inline Input MirrorControls(const Input& source) {
    const auto axis = [](float value) {
        return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
    };
    Input result{};
    result.Throttle = axis(source.Throttle);
    result.Steer = axis(source.Steer);
    result.Pitch = axis(source.Pitch);
    result.Yaw = axis(source.Yaw);
    result.Roll = axis(source.Roll);
    result.DodgeForward = axis(source.DodgeForward);
    result.DodgeStrafe = axis(source.DodgeStrafe);
    result.Handbrake = source.Handbrake ? 1 : 0;
    result.Jump = source.Jump ? 1 : 0;
    result.ActivateBoost = source.ActivateBoost ? 1 : 0;
    result.HoldingBoost = source.HoldingBoost ? 1 : 0;
    result.Jumped = source.Jumped ? 1 : 0;
    return result;
}

// Effects use elapsed monotonic time while a round stays active. Advancing the
// whole elapsed interval lets deadlines expire after a hitch or stopped input.
inline double AdvanceActiveClock(double current, double elapsed, bool wasActive, bool active) {
    if (!std::isfinite(current) || !std::isfinite(elapsed) || elapsed < 0 || !wasActive || !active)
        return current;
    const double advanced = current + elapsed;
    return std::isfinite(advanced) ? advanced : current;
}

inline const SpellInfo& GetSpellInfo(Spell spell) {
    static const std::array<SpellInfo, MaxSpellId + 1> infos{{
        {"none", "Charging", "Your next spell is charging.", {170, 180, 195}},
        {"accio", "Accio", "Charge for half a second, then guide the ball to a controlled stop in front of your car.", {85, 195, 255}},
        {"avada_kedavra", "Avada Kedavra", "Charge for two seconds, then demolish the targeted opponent.", {85, 255, 120}},
        {"leviosa", "Wingardium Leviosa", "Charge for two seconds, lift the target to the roof, then let it fall.", {200, 145, 255}},
        {"bombarda_maxima", "Bombarda Maxima", "Charge for two seconds, then send the ball ricocheting around the arena.", {255, 155, 80}},
        {"none", "Charging", "Your next spell is charging.", {170, 180, 195}},
        {"protego", "Protego", "Charge for one second, then raise a thin barrier ahead of the moving ball.", {255, 220, 100}},
        {"none", "Charging", "Your next spell is charging.", {170, 180, 195}},
        {"stupefy", "Stupefy", "Charge for 0.65 seconds, then knock the targeted opponent away.", {255, 100, 100}},
        {"petrificus_totalus", "Petrificus Totalus", "Charge for 1.5 seconds, then bind the targeted opponent for two seconds.", {185, 165, 255}},
        {"none", "Charging", "Your next spell is charging.", {170, 180, 195}},
        {"none", "Charging", "Your next spell is charging.", {170, 180, 195}},
        {"none", "Charging", "Your next spell is charging.", {170, 180, 195}},
        {"obliviate", "Obliviate", "Charge for half a second, then scramble the target's driving controls for five seconds.", {255, 225, 75}},
        {"eat_slugs", "Eat Slugs", "Charge for 1.5 seconds, then shoot slugs that cover the opponent's view for 3.75 seconds.", {178, 220, 88}},
        {"expecto_patronum", "Expecto Patronum", "Charge for three seconds, then summon a ghostly Octane to strike the ball once toward the opposing goal.", {155, 240, 255}},
        {"imperio", "Imperio", "Charge for one second, then make the targeted opponent mirror your controls for 3.5 seconds.", {188, 145, 255}}
    }};
    return infos[IsValidSpell(spell) ? static_cast<int>(spell) : 0];
}

struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;

    Vec3 operator+(const Vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    Vec3 operator-(const Vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    Vec3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    Vec3 operator/(float scale) const { return {x / scale, y / scale, z / scale}; }
    float Dot(const Vec3& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }
    Vec3 Cross(const Vec3& rhs) const {
        return {y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x};
    }
    float LengthSquared() const { return Dot(*this); }
    float Length() const { return std::sqrt(LengthSquared()); }
    bool IsFinite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
    Vec3 Normalized() const {
        const float length = Length();
        return IsFinite() && std::isfinite(length) && length > 0.00001f ? *this / length : Vec3{};
    }
};

inline std::optional<Vec3> PatronumSpawn(Vec3 casterPosition, Vec3 casterForward) {
    if (!casterPosition.IsFinite() || !casterForward.IsFinite()) return std::nullopt;
    auto forward = casterForward.Normalized();
    if (forward.LengthSquared() < 0.5f) forward = {1, 0, 0};
    auto position = casterPosition + forward * 140.0f + Vec3{0, 0, 70};
    position.z = std::max(60.0f, position.z);
    return position.IsFinite() ? std::optional<Vec3>(position) : std::nullopt;
}

struct PatronumStep {
    Vec3 position;
    Vec3 forward;
    bool hit = false;
};

// The ghost has no independent rigid body. Advance a bounded homing path and
// test its segment against the moving ball in relative coordinates. Dividing
// a hitch into small steps prevents tunneling without applying repeated hits;
// the adapter removes its owned flight after the first returned hit.
inline std::optional<PatronumStep> AdvancePatronum(Vec3 position, Vec3 previousBall,
    Vec3 currentBall, double deltaSeconds, float ballRadius = 93.0f) {
    if (!position.IsFinite() || !previousBall.IsFinite() || !currentBall.IsFinite() ||
        !std::isfinite(deltaSeconds) || deltaSeconds <= 0 || deltaSeconds > PatronumTimeoutSeconds ||
        !std::isfinite(ballRadius) || ballRadius < 10 || ballRadius > 1000) return std::nullopt;
    const auto ballDelta = currentBall - previousBall;
    if (!ballDelta.IsFinite() || !std::isfinite(ballDelta.LengthSquared())) return std::nullopt;
    const int steps = std::clamp(static_cast<int>(std::ceil(deltaSeconds * 60.0)), 1, 150);
    const float stepSeconds = static_cast<float>(deltaSeconds / steps);
    const float contact = ballRadius + PatronumHitRadius;
    PatronumStep result{position, (currentBall - position).Normalized(), false};
    for (int i = 0; i < steps; ++i) {
        const auto ballBefore = previousBall + ballDelta * (static_cast<float>(i) / steps);
        const auto ballAfter = previousBall + ballDelta * (static_cast<float>(i + 1) / steps);
        const auto toBall = ballAfter - result.position;
        const float distance = toBall.Length();
        if (!toBall.IsFinite() || !std::isfinite(distance)) return std::nullopt;
        const auto direction = toBall.Normalized();
        if (direction.LengthSquared() > 0.5f) result.forward = direction;
        const auto next = result.position + direction * std::min(distance, PatronumSpeed * stepSeconds);
        const auto relativeStart = result.position - ballBefore;
        const auto relativeDelta = (next - result.position) - (ballAfter - ballBefore);
        const float squaredDelta = relativeDelta.LengthSquared();
        if (!std::isfinite(squaredDelta)) return std::nullopt;
        const float fraction = squaredDelta > 0.00001f ?
            std::clamp(-relativeStart.Dot(relativeDelta) / squaredDelta, 0.0f, 1.0f) : 0.0f;
        const auto separation = relativeStart + relativeDelta * fraction;
        if (separation.LengthSquared() <= contact * contact) {
            result.position = result.position + (next - result.position) * fraction;
            result.hit = true;
            return result;
        }
        result.position = next;
    }
    return result.position.IsFinite() && result.forward.IsFinite() ?
        std::optional<PatronumStep>(result) : std::nullopt;
}

inline std::optional<Vec3> PatronumShotVelocity(Vec3 ball, Vec3 opponentGoal,
    float speed = PatronumShotSpeed) {
    if (!ball.IsFinite() || !opponentGoal.IsFinite() || !std::isfinite(speed) || speed <= 0)
        return std::nullopt;
    const auto direction = (opponentGoal - ball).Normalized();
    if (direction.LengthSquared() < 0.5f) return std::nullopt;
    const auto velocity = direction * std::min(speed, PatronumShotSpeed);
    return velocity.IsFinite() ? std::optional<Vec3>(velocity) : std::nullopt;
}

inline double AccioApproachSeconds(float distance) {
    if (!std::isfinite(distance)) return 2.8;
    return std::clamp(1.2 + static_cast<double>(std::max(0.0f, distance)) / 3300.0, 1.2, 2.8);
}

// Keep the ball clear of the car's collision hull, even while its nose pitches
// vertically. The adapter recomputes this point as the caster drives or turns.
inline Vec3 AccioTarget(Vec3 casterPosition, Vec3 casterForward, float ballRadius = 93) {
    if (!casterPosition.IsFinite() || !casterForward.IsFinite() || !std::isfinite(ballRadius)) return {};
    ballRadius = std::clamp(ballRadius, 10.0f, 1000.0f);
    Vec3 forward{casterForward.x, casterForward.y, 0};
    forward = forward.Normalized();
    if (forward.LengthSquared() < 0.5f) forward = {1, 0, 0};
    const float ahead = std::max(260.0f, ballRadius + 167.0f);
    Vec3 target = casterPosition + forward * ahead;
    target.z = std::max(ballRadius, casterPosition.z + ballRadius);
    return target;
}

struct AccioGuidance {
    Vec3 velocity;
    bool arrived = false;
};

// A critically damped velocity command in the moving target's frame. Exact
// damping over the bounded step avoids an unstable Euler acceleration spring.
// Speed/acceleration limits preserve physical travel; positions are never set.
// Gravity is excluded here: the adapter compensates the engine's gravity when
// applying the returned command. It owns settlement time and a finite timeout.
inline std::optional<AccioGuidance> GuideAccio(Vec3 ballPosition, Vec3 ballVelocity,
    Vec3 target, Vec3 targetVelocity, double deltaSeconds, double approachSeconds) {
    if (!ballPosition.IsFinite() || !ballVelocity.IsFinite() || !target.IsFinite() ||
        !targetVelocity.IsFinite() || !std::isfinite(deltaSeconds) || deltaSeconds <= 0 ||
        !std::isfinite(approachSeconds) || approachSeconds <= 0) return std::nullopt;
    const Vec3 offset = ballPosition - target;
    const Vec3 relative = ballVelocity - targetVelocity;
    if (!offset.IsFinite() || !relative.IsFinite()) return std::nullopt;
    const float dt = static_cast<float>(std::min(deltaSeconds, 0.1));
    const float omega = 8.0f / static_cast<float>(std::clamp(approachSeconds, 1.2, 2.8));
    const float decay = std::exp(-omega * dt);
    Vec3 desired = targetVelocity + (relative - (relative + offset * omega) * (omega * dt)) * decay;
    Vec3 change = desired - ballVelocity;
    const float changeLength = change.Length();
    if (!std::isfinite(changeLength)) return std::nullopt;
    const float maximumChange = AccioMaxAcceleration * dt;
    if (changeLength > maximumChange) desired = ballVelocity + change * (maximumChange / changeLength);
    const float speed = desired.Length();
    if (!std::isfinite(speed)) return std::nullopt;
    if (speed > AccioMaxSpeed) desired = desired * (AccioMaxSpeed / speed);
    return AccioGuidance{desired, offset.LengthSquared() <= 35.0f * 35.0f &&
        relative.LengthSquared() <= 120.0f * 120.0f};
}

// The cast and its targeting indicator share this inclusive, finite range rule.
// Coincident ball targets remain valid; SelectTarget excludes coincident cars.
inline bool InSpellRange(Vec3 origin, Vec3 target, float range = DefaultSpellRange) {
    if (!origin.IsFinite() || !target.IsFinite() || !std::isfinite(range) || range <= 0) return false;
    const float distance = (target - origin).Length();
    return std::isfinite(distance) && distance <= range;
}

struct Barrier {
    Vec3 center;
    Vec3 normal{1, 0, 0};
    Vec3 right{0, 1, 0};
    Vec3 up{0, 0, 1};
    float radius = 350;
};

// Protego is a stationary circular disk perpendicular to the ball's travel.
// A stable perpendicular basis also supports nearly vertical ball trajectories.
inline std::optional<Barrier> PlanBarrier(Vec3 ball, Vec3 velocity, Vec3 fallbackForward,
    float ahead = 450, float radius = 350) {
    if (!ball.IsFinite() || !velocity.IsFinite() || !fallbackForward.IsFinite() ||
        !std::isfinite(ahead) || ahead < 0 || !std::isfinite(radius) || radius <= 0) return std::nullopt;
    Vec3 normal = velocity.LengthSquared() > 1 ? velocity.Normalized() : fallbackForward.Normalized();
    if (normal.LengthSquared() < 0.5f) normal = {1, 0, 0};
    const Vec3 reference = std::abs(normal.z) > 0.95f ? Vec3{0, 1, 0} : Vec3{0, 0, 1};
    const Vec3 right = reference.Cross(normal).Normalized();
    const Vec3 up = normal.Cross(right).Normalized();
    Vec3 center = ball + normal * ahead;
    const float verticalRadius = radius * std::sqrt(right.z * right.z + up.z * up.z);
    const float bottom = center.z - verticalRadius;
    if (up.z > 0.1f && bottom < 20) center = center + up * ((20 - bottom) / up.z);
    return Barrier{center, normal, right, up, radius};
}

struct BarrierHit {
    Vec3 position; // Corrected sphere center, outside the struck side by 2 units.
    Vec3 velocity;
    Vec3 contact;  // Point on the disk face or circular rim, for an impact ripple.
    Vec3 normal;   // Outward contact normal; includes glancing rim contacts.
    float fraction = 0;
};

inline bool IsValidBarrier(const Barrier& wall) {
    return wall.center.IsFinite() && wall.normal.IsFinite() && wall.right.IsFinite() && wall.up.IsFinite() &&
        std::isfinite(wall.radius) && wall.radius > 0 &&
        std::abs(wall.normal.LengthSquared() - 1) < 0.01f && std::abs(wall.right.LengthSquared() - 1) < 0.01f &&
        std::abs(wall.up.LengthSquared() - 1) < 0.01f && std::abs(wall.normal.Dot(wall.right)) < 0.01f &&
        std::abs(wall.normal.Dot(wall.up)) < 0.01f && std::abs(wall.right.Dot(wall.up)) < 0.01f;
}

inline Vec3 ClosestBarrierPoint(const Barrier& wall, Vec3 point) {
    const Vec3 relative = point - wall.center;
    const float y = relative.Dot(wall.right), z = relative.Dot(wall.up);
    const float radial = std::hypot(y, z);
    const float scale = radial > wall.radius ? wall.radius / radial : 1.0f;
    return wall.center + wall.right * (y * scale) + wall.up * (z * scale);
}

// Continuous sphere sweep against a finite disk, including its circular rim.
// The squared distance to a convex disk is convex along the travel segment.
// Find its minimum, then bisect the first sphere contact instead of sampling
// frames or using an invisible rectangular/square collision proxy.
inline std::optional<BarrierHit> SweepBarrier(Vec3 previous, Vec3 current, Vec3 velocity,
    const Barrier& wall, float radius = 93, float restitution = 0.95f) {
    if (!previous.IsFinite() || !current.IsFinite() || !velocity.IsFinite() || !IsValidBarrier(wall) ||
        !std::isfinite(radius) || radius <= 0 || !std::isfinite(restitution) || restitution < 0 || restitution > 1)
        return std::nullopt;
    const auto dot = [](const std::array<double, 3>& v, Vec3 axis) {
        return v[0] * axis.x + v[1] * axis.y + v[2] * axis.z;
    };
    const std::array<double, 3> from{{static_cast<double>(previous.x) - wall.center.x,
        static_cast<double>(previous.y) - wall.center.y, static_cast<double>(previous.z) - wall.center.z}};
    const std::array<double, 3> delta{{static_cast<double>(current.x) - previous.x,
        static_cast<double>(current.y) - previous.y, static_cast<double>(current.z) - previous.z}};
    const std::array<double, 3> p{{dot(from, wall.normal), dot(from, wall.right), dot(from, wall.up)}};
    const std::array<double, 3> d{{dot(delta, wall.normal), dot(delta, wall.right), dot(delta, wall.up)}};
    const double sphereRadiusSquared = static_cast<double>(radius) * radius;
    const auto distanceSquared = [&](double t) {
        const double x = p[0] + d[0] * t, y = p[1] + d[1] * t, z = p[2] + d[2] * t;
        const double outside = std::max(0.0, std::hypot(y, z) - wall.radius);
        return x * x + outside * outside;
    };
    const auto slope = [&](double t) {
        const double x = p[0] + d[0] * t, y = p[1] + d[1] * t, z = p[2] + d[2] * t;
        const double radial = std::hypot(y, z);
        return x * d[0] + (radial > wall.radius ?
            (1.0 - wall.radius / radial) * (y * d[1] + z * d[2]) : 0.0);
    };
    double fraction = 0;
    if (distanceSquared(0) > sphereRadiusSquared) {
        if (slope(0) >= 0) return std::nullopt;
        double minimum = 1;
        if (slope(1) > 0) {
            double low = 0, high = 1;
            for (int i = 0; i < 64; ++i) {
                const double middle = (low + high) * 0.5;
                if (slope(middle) < 0) low = middle; else high = middle;
            }
            minimum = (low + high) * 0.5;
        }
        const double clearance = distanceSquared(minimum) - sphereRadiusSquared;
        const double tolerance = 1e-10 * std::max(1.0, sphereRadiusSquared);
        // A pure tangent has no entering contact and must not kick the ball.
        if (clearance > tolerance || (minimum < 1 && clearance >= -tolerance)) return std::nullopt;
        double low = 0, high = minimum;
        for (int i = 0; i < 64; ++i) {
            const double middle = (low + high) * 0.5;
            if (distanceSquared(middle) > sphereRadiusSquared) low = middle; else high = middle;
        }
        fraction = high;
    }
    const double x = p[0] + d[0] * fraction, y = p[1] + d[1] * fraction, z = p[2] + d[2] * fraction;
    const double radial = std::hypot(y, z), scale = radial > wall.radius ? wall.radius / radial : 1.0;
    const double contactY = y * scale, contactZ = z * scale;
    const Vec3 contact = wall.center + wall.right * static_cast<float>(contactY) + wall.up * static_cast<float>(contactZ);
    const double offsetY = y - contactY, offsetZ = z - contactZ;
    const double distance = std::sqrt(x * x + offsetY * offsetY + offsetZ * offsetZ);
    Vec3 normal;
    if (distance > 1e-8) {
        normal = wall.normal * static_cast<float>(x / distance) + wall.right * static_cast<float>(offsetY / distance) +
            wall.up * static_cast<float>(offsetZ / distance);
        normal = normal.Normalized();
    } else {
        const float normalSpeed = velocity.Dot(wall.normal);
        if (std::abs(normalSpeed) < 0.0001f) return std::nullopt;
        normal = wall.normal * (normalSpeed > 0 ? -1.0f : 1.0f);
    }
    const float approaching = velocity.Dot(normal);
    const double travelApproach = dot(delta, normal);
    if (approaching >= -0.0001f || (fraction > 0 && travelApproach >= -0.0001)) return std::nullopt;
    const Vec3 position = contact + normal * (radius + 2);
    const Vec3 reflected = velocity - normal * ((1 + restitution) * approaching);
    if (!position.IsFinite() || !reflected.IsFinite()) return std::nullopt;
    return BarrierHit{position, reflected, contact, normal, static_cast<float>(fraction)};
}

constexpr float LeviosaRiseSpeed = 700.0f;
constexpr float LeviosaRoofDropSpeed = 150.0f;
constexpr double LeviosaSafetySeconds = 8.0;

// This free-space position is diagnostic only. The live Leviosa adapter writes
// velocity and lets native world collision stop the body at the actual roof.
// Petrificus supplies zero rise and keeps its original pose.
inline Vec3 SuspensionPosition(Vec3 start, float riseSpeed, double elapsed) {
    if (!start.IsFinite() || !std::isfinite(riseSpeed) || !std::isfinite(elapsed) ||
        riseSpeed <= 0 || elapsed <= 0) return start;
    Vec3 position = start;
    position.z += riseSpeed * static_cast<float>(elapsed);
    return position.IsFinite() ? position : start;
}

// The SDK exposes world-contact points but no public world line/sweep trace.
// Requiring a nearby point above the car and a downward-facing surface rejects
// floor/side-wall/stale contacts without assuming an arena roof height. Orient
// the normal toward the car because the SDK's normal orientation is undocumented.
inline bool IsLeviosaRoofContact(Vec3 carPosition, bool hasContact, Vec3 contactPosition,
    Vec3 contactNormal, float bodyReach = 130.0f) {
    if (!hasContact || !carPosition.IsFinite() || !contactPosition.IsFinite() ||
        !contactNormal.IsFinite() || !std::isfinite(bodyReach) || bodyReach <= 0 || bodyReach > 2000)
        return false;
    const auto offset = contactPosition - carPosition;
    auto normal = contactNormal.Normalized();
    if (normal.Dot(offset) > 0) normal = normal * -1;
    return offset.IsFinite() && offset.z >= 2 && offset.LengthSquared() <= bodyReach * bodyReach &&
        normal.z <= -0.15f;
}

struct TargetCandidate {
    int id;
    Vec3 location;
    bool eligible = true;
};

// Prefer aim alignment, then distance, then the lower player ID. Independent of
// network array order. Ineligible candidates must include the caster, allies,
// spectators, and demolished cars, as determined by the host adapter.
inline std::optional<int> SelectTarget(const Vec3& origin, const Vec3& forward,
    const std::vector<TargetCandidate>& candidates, float range = DefaultSpellRange,
    float minimumCosine = 0.82f) {
    if (!origin.IsFinite() || !forward.IsFinite() || !std::isfinite(range) || range <= 0 ||
        !std::isfinite(minimumCosine) || minimumCosine < -1 || minimumCosine > 1) return std::nullopt;
    const Vec3 direction = forward.Normalized();
    if (direction.LengthSquared() < 0.5f) return std::nullopt;
    std::optional<int> selected;
    float bestAlignment = -2;
    float bestDistance = std::numeric_limits<float>::max();
    for (const auto& candidate : candidates) {
        if (!candidate.eligible || candidate.id < 0 || !candidate.location.IsFinite()) continue;
        const Vec3 offset = candidate.location - origin;
        const float distance = offset.Length();
        if (distance <= 0.00001f || !InSpellRange(origin, candidate.location, range)) continue;
        const float alignment = direction.Dot(offset / distance);
        if (alignment < minimumCosine) continue;
        const bool sameAlignment = alignment == bestAlignment;
        if (!selected || alignment > bestAlignment ||
            (sameAlignment && (distance < bestDistance ||
                (distance == bestDistance && candidate.id < *selected)))) {
            selected = candidate.id;
            bestAlignment = alignment;
            bestDistance = distance;
        }
    }
    return selected;
}

class SpellDeck {
public:
    explicit SpellDeck(std::uint32_t seed, double cooldown = 10.0) : random_(seed) { SetCooldown(cooldown); }

    // Changing the setting affects new cooldowns; existing deadlines remain.
    void SetCooldown(double seconds) {
        cooldown_ = std::isfinite(seconds) ? std::clamp(seconds, 1.0, 60.0) : 10.0;
    }
    double Cooldown() const { return cooldown_; }
    double CooldownFor(int id) const {
        const auto found = players_.find(id);
        return found == players_.end() ? cooldown_ : found->second.duration;
    }
    void EnsurePlayer(int id, double now) {
        if (id >= 0 && std::isfinite(now)) players_.try_emplace(id, Entry{Spell::None, now + cooldown_, cooldown_});
    }
    void Update(double now) {
        if (!std::isfinite(now)) return;
        std::uniform_int_distribution<int> distribution(0, SpellCount - 1);
        for (auto& player : players_) {
            if (player.second.spell == Spell::None && now >= player.second.readyAt)
                player.second.spell = ActiveSpells[distribution(random_)];
        }
    }
    Spell ReadyFor(int id) const {
        const auto found = players_.find(id);
        return found == players_.end() ? Spell::None : found->second.spell;
    }
    double Remaining(int id, double now) const {
        const auto found = players_.find(id);
        if (found == players_.end() || found->second.spell != Spell::None || !std::isfinite(now)) return 0;
        return std::max(0.0, found->second.readyAt - now);
    }
    // Call only after the adapter successfully applies a spell. Failed targeting
    // and blocked effects leave the held spell untouched by simply not consuming.
    bool Consume(int id, double now) {
        const auto found = players_.find(id);
        if (!std::isfinite(now) || found == players_.end() || found->second.spell == Spell::None) return false;
        found->second = {Spell::None, now + cooldown_, cooldown_};
        return true;
    }
    // Removing a held spell does not restart
    // the cooldown of an opponent who is still charging.
    bool Disarm(int id, double now) { return Consume(id, now); }
    // The game adapter exposes this only in local Freeplay practice.
    bool Grant(int id, Spell spell, double now) {
        if (id < 0 || id > MaxPlayerId || !IsCastableSpell(spell) || !std::isfinite(now)) return false;
        players_.insert_or_assign(id, Entry{spell, now, cooldown_});
        return true;
    }
    void RemoveMissing(const std::vector<int>& activeIds) {
        for (auto it = players_.begin(); it != players_.end();) {
            if (std::find(activeIds.begin(), activeIds.end(), it->first) == activeIds.end()) it = players_.erase(it);
            else ++it;
        }
    }
    void Clear() { players_.clear(); }
    std::size_t Size() const { return players_.size(); }

private:
    struct Entry { Spell spell; double readyAt; double duration; };
    std::map<int, Entry> players_;
    std::mt19937 random_;
    double cooldown_ = 10.0;
};

enum class MessageKind { Cast, Hello, State, Feedback, Effect, EffectSnapshot, Cancel, Clear, Impact, Charge, ChargeSnapshot, PatronumImpact, ProtegoChargeCancel };
enum class Feedback : int {
    CastOk = 0, NoTarget = 1, Shielded = 2, NotReady = 3, Disabled = 4, Unavailable = 5, Charging = 6
};

struct Message {
    MessageKind kind = MessageKind::Hello;
    int playerId = -1;
    Spell spell = Spell::None;
    int remainingDeciseconds = 0;
    int cooldownDeciseconds = 100;
    float spellRange = DefaultSpellRange;
    bool enabled = false;
    Feedback feedback = Feedback::CastOk;
    int casterId = -1;
    int targetId = -1;
    int recipientId = -1; // -1 broadcasts; snapshots address the joining client.
    Vec3 source;
    Vec3 destination;
    Vec3 normal{1, 0, 0};
    int durationMillis = 600;
};

constexpr std::size_t MaxMessageLength = 110;
constexpr int MaxRemainingDeciseconds = 600;
constexpr int MaxEffectDurationMillis = 10000;
constexpr int MaxWorldCoordinate = 99999;

namespace detail {
inline bool ParseUnsigned(std::string_view token, int maximum, int& value) {
    if (token.empty() || token.front() < '0' || token.front() > '9') return false;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    return result.ec == std::errc{} && result.ptr == token.data() + token.size() && value >= 0 && value <= maximum;
}
inline bool ParseSigned(std::string_view token, int minimum, int maximum, int& value) {
    if (token.empty() || token.front() == '+' ||
        (token.front() != '-' && (token.front() < '0' || token.front() > '9'))) return false;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    return result.ec == std::errc{} && result.ptr == token.data() + token.size() && value >= minimum && value <= maximum;
}
inline bool ParseOptionalPlayer(std::string_view token, int& value) {
    if (token == "-1") { value = -1; return true; }
    return ParseUnsigned(token, MaxPlayerId, value);
}
inline bool ParseSpellRange(std::string_view token, float& value) {
    if (token.empty() || token.front() < '0' || token.front() > '9') return false;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value, std::chars_format::general);
    return result.ec == std::errc{} && result.ptr == token.data() + token.size() &&
        std::isfinite(value) && value >= MinSpellRange && value <= MaxSpellRange;
}
inline bool ParseVector(const std::vector<std::string_view>& tokens, std::size_t index,
    int limit, float scale, Vec3& result) {
    int x, y, z;
    if (!ParseSigned(tokens[index], -limit, limit, x) || !ParseSigned(tokens[index + 1], -limit, limit, y) ||
        !ParseSigned(tokens[index + 2], -limit, limit, z)) return false;
    result = {x / scale, y / scale, z / scale};
    return true;
}
inline bool ValidOptionalPlayer(int value) { return value >= -1 && value <= MaxPlayerId; }
inline bool EncodeVector(Vec3 vector, int limit, float scale, std::string& wire) {
    if (!vector.IsFinite()) return false;
    for (const float value : {vector.x, vector.y, vector.z}) {
        const double scaled = static_cast<double>(value) * scale;
        if (scaled < -limit - 0.49 || scaled > limit + 0.49) return false;
        const long encoded = std::lround(scaled);
        if (encoded < -limit || encoded > limit) return false;
        wire += " " + std::to_string(encoded);
    }
    return true;
}
inline bool Tokenize(const std::string& wire, std::vector<std::string_view>& tokens) {
    if (wire.empty() || wire.size() > MaxMessageLength || wire.front() == ' ' || wire.back() == ' ') return false;
    std::size_t start = 0;
    for (std::size_t i = 0; i < wire.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(wire[i]);
        if (c < 32 || c > 126) return false;
        if (c == ' ') {
            if (i == start) return false;
            tokens.emplace_back(wire.data() + start, i - start);
            start = i + 1;
        }
    }
    tokens.emplace_back(wire.data() + start, wire.size() - start);
    return true;
}
} // namespace detail

// The wire never provides the caster identity. The engine-supplied sender PRI
// must be validated by the host before accepting Cast or Hello messages.
inline std::optional<Message> ParseMessage(const std::string& wire) {
    std::vector<std::string_view> tokens;
    if (!detail::Tokenize(wire, tokens) || tokens.size() < 2 || tokens[0] != "W2") return std::nullopt;
    Message message;
    if (tokens[1] == "C" || tokens[1] == "H") {
        if (tokens.size() != 2) return std::nullopt;
        message.kind = tokens[1] == "C" ? MessageKind::Cast : MessageKind::Hello;
        return message;
    }
    if (tokens[1] == "S") {
        // Previous W2 states omit range and use the original 4,500-unit default.
        if (tokens.size() != 7 && tokens.size() != 8) return std::nullopt;
        message.spellRange = LegacySpellRange;
        int spell, enabled;
        if (!detail::ParseUnsigned(tokens[2], MaxPlayerId, message.playerId) ||
            !detail::ParseUnsigned(tokens[3], MaxSpellId, spell) || !IsValidSpell(static_cast<Spell>(spell)) ||
            !detail::ParseUnsigned(tokens[4], MaxRemainingDeciseconds, message.remainingDeciseconds) ||
            !detail::ParseUnsigned(tokens[5], MaxRemainingDeciseconds, message.cooldownDeciseconds) ||
            message.cooldownDeciseconds < 1 || message.remainingDeciseconds > message.cooldownDeciseconds ||
            !detail::ParseUnsigned(tokens[6], 1, enabled) ||
            (tokens.size() == 8 && !detail::ParseSpellRange(tokens[7], message.spellRange))) return std::nullopt;
        // A held spell is ready, so a non-zero cooldown would be contradictory.
        if (spell != 0 && message.remainingDeciseconds != 0) return std::nullopt;
        message.kind = MessageKind::State;
        message.spell = static_cast<Spell>(spell);
        message.enabled = enabled != 0;
        return message;
    }
    if (tokens[1] == "F") {
        if (tokens.size() != 4) return std::nullopt;
        int feedback;
        if (!detail::ParseUnsigned(tokens[2], MaxPlayerId, message.playerId) ||
            !detail::ParseUnsigned(tokens[3], static_cast<int>(Feedback::Charging), feedback)) return std::nullopt;
        message.kind = MessageKind::Feedback;
        message.feedback = static_cast<Feedback>(feedback);
        return message;
    }
    if (tokens[1] == "E" || tokens[1] == "P" || tokens[1] == "G" || tokens[1] == "J") {
        if (tokens.size() != 16) return std::nullopt;
        int spell;
        if (!detail::ParseUnsigned(tokens[2], MaxSpellId, spell) || !IsCastableSpell(static_cast<Spell>(spell)) ||
            !detail::ParseUnsigned(tokens[3], MaxPlayerId, message.casterId) ||
            !detail::ParseOptionalPlayer(tokens[4], message.targetId) ||
            !detail::ParseOptionalPlayer(tokens[5], message.recipientId) ||
            !detail::ParseVector(tokens, 6, MaxWorldCoordinate, 1, message.source) ||
            !detail::ParseVector(tokens, 9, MaxWorldCoordinate, 1, message.destination) ||
            !detail::ParseVector(tokens, 12, 1000, 1000, message.normal) ||
            message.normal.LengthSquared() > 1.01f ||
            !detail::ParseUnsigned(tokens[15], MaxEffectDurationMillis, message.durationMillis) || message.durationMillis < 1)
            return std::nullopt;
        const bool charging = tokens[1] == "G" || tokens[1] == "J";
        if (charging && (!IsValidChargeTarget(static_cast<Spell>(spell), message.targetId) ||
            message.durationMillis > static_cast<int>(SpellChargeSeconds(static_cast<Spell>(spell)) * 1000))) return std::nullopt;
        message.kind = tokens[1] == "E" ? MessageKind::Effect : tokens[1] == "P" ? MessageKind::EffectSnapshot :
            tokens[1] == "G" ? MessageKind::Charge : MessageKind::ChargeSnapshot;
        message.spell = static_cast<Spell>(spell);
        return message;
    }
    if (tokens[1] == "X") {
        if (tokens.size() != 6) return std::nullopt;
        int spell;
        if (!detail::ParseUnsigned(tokens[2], MaxSpellId, spell) || !IsCastableSpell(static_cast<Spell>(spell)) ||
            !detail::ParseUnsigned(tokens[3], MaxPlayerId, message.casterId) ||
            !detail::ParseOptionalPlayer(tokens[4], message.targetId) ||
            !detail::ParseOptionalPlayer(tokens[5], message.recipientId)) return std::nullopt;
        message.kind = MessageKind::Cancel; message.spell = static_cast<Spell>(spell);
        return message;
    }
    if (tokens[1] == "R") {
        if (tokens.size() != 3 || !detail::ParseOptionalPlayer(tokens[2], message.recipientId)) return std::nullopt;
        message.kind = MessageKind::Clear; return message;
    }
    if (tokens[1] == "B") {
        if (tokens.size() != 6 || !detail::ParseUnsigned(tokens[2], MaxPlayerId, message.casterId) ||
            !detail::ParseVector(tokens, 3, MaxWorldCoordinate, 1, message.source)) return std::nullopt;
        message.kind = MessageKind::Impact; return message;
    }
    if (tokens[1] == "V") {
        if (tokens.size() != 9 || !detail::ParseUnsigned(tokens[2], MaxPlayerId, message.casterId) ||
            !detail::ParseVector(tokens, 3, MaxWorldCoordinate, 1, message.source) ||
            !detail::ParseVector(tokens, 6, 1000, 1000, message.normal) ||
            message.normal.LengthSquared() < 0.5f || message.normal.LengthSquared() > 1.01f)
            return std::nullopt;
        message.kind = MessageKind::PatronumImpact; message.spell = Spell::ExpectoPatronum;
        return message;
    }
    if (tokens[1] == "U") {
        if (tokens.size() != 3 || !detail::ParseUnsigned(tokens[2], MaxPlayerId, message.casterId))
            return std::nullopt;
        message.kind = MessageKind::ProtegoChargeCancel; message.spell = Spell::Protego;
        return message;
    }
    return std::nullopt;
}

// Invalid outgoing states produce an empty string rather than malformed traffic.
inline std::string SerializeMessage(const Message& message) {
    if (message.kind == MessageKind::Cast) return "W2 C";
    if (message.kind == MessageKind::Hello) return "W2 H";
    if (message.kind == MessageKind::Effect || message.kind == MessageKind::EffectSnapshot ||
        message.kind == MessageKind::Charge || message.kind == MessageKind::ChargeSnapshot) {
        const bool charging = message.kind == MessageKind::Charge || message.kind == MessageKind::ChargeSnapshot;
        if (!IsCastableSpell(message.spell) ||
            message.casterId < 0 || message.casterId > MaxPlayerId ||
            !detail::ValidOptionalPlayer(message.targetId) || !detail::ValidOptionalPlayer(message.recipientId) ||
            !message.normal.IsFinite() || message.normal.LengthSquared() > 1.01f ||
            message.durationMillis < 1 || message.durationMillis > MaxEffectDurationMillis ||
            (charging && (!IsValidChargeTarget(message.spell, message.targetId) ||
                message.durationMillis > static_cast<int>(SpellChargeSeconds(message.spell) * 1000)))) return {};
        std::string wire = message.kind == MessageKind::Effect ? "W2 E " :
            message.kind == MessageKind::EffectSnapshot ? "W2 P " :
            message.kind == MessageKind::Charge ? "W2 G " : "W2 J ";
        wire += std::to_string(static_cast<int>(message.spell)) + " " + std::to_string(message.casterId) + " " +
            std::to_string(message.targetId) + " " + std::to_string(message.recipientId);
        if (!detail::EncodeVector(message.source, MaxWorldCoordinate, 1, wire) ||
            !detail::EncodeVector(message.destination, MaxWorldCoordinate, 1, wire) ||
            !detail::EncodeVector(message.normal, 1000, 1000, wire)) return {};
        wire += " " + std::to_string(message.durationMillis);
        return wire.size() <= MaxMessageLength ? wire : std::string{};
    }
    if (message.kind == MessageKind::Cancel) {
        if (!IsCastableSpell(message.spell) || message.casterId < 0 || message.casterId > MaxPlayerId ||
            !detail::ValidOptionalPlayer(message.targetId) || !detail::ValidOptionalPlayer(message.recipientId)) return {};
        return "W2 X " + std::to_string(static_cast<int>(message.spell)) + " " + std::to_string(message.casterId) + " " +
            std::to_string(message.targetId) + " " + std::to_string(message.recipientId);
    }
    if (message.kind == MessageKind::Clear) {
        return detail::ValidOptionalPlayer(message.recipientId) ? "W2 R " + std::to_string(message.recipientId) : std::string{};
    }
    if (message.kind == MessageKind::Impact) {
        if (message.casterId < 0 || message.casterId > MaxPlayerId) return {};
        std::string wire = "W2 B " + std::to_string(message.casterId);
        return detail::EncodeVector(message.source, MaxWorldCoordinate, 1, wire) ? wire : std::string{};
    }
    if (message.kind == MessageKind::PatronumImpact) {
        if (message.casterId < 0 || message.casterId > MaxPlayerId || !message.normal.IsFinite() ||
            message.normal.LengthSquared() < 0.5f || message.normal.LengthSquared() > 1.01f) return {};
        std::string wire = "W2 V " + std::to_string(message.casterId);
        if (!detail::EncodeVector(message.source, MaxWorldCoordinate, 1, wire) ||
            !detail::EncodeVector(message.normal, 1000, 1000, wire)) return {};
        return wire.size() <= MaxMessageLength ? wire : std::string{};
    }
    if (message.kind == MessageKind::ProtegoChargeCancel) {
        if (message.casterId < 0 || message.casterId > MaxPlayerId) return {};
        return "W2 U " + std::to_string(message.casterId);
    }
    if (message.playerId < 0 || message.playerId > MaxPlayerId) return {};
    if (message.kind == MessageKind::State) {
        if (!IsValidSpell(message.spell) || message.remainingDeciseconds < 0 ||
            message.remainingDeciseconds > MaxRemainingDeciseconds || message.cooldownDeciseconds < 1 ||
            message.cooldownDeciseconds > MaxRemainingDeciseconds || message.remainingDeciseconds > message.cooldownDeciseconds ||
            (message.spell != Spell::None && message.remainingDeciseconds != 0) ||
            !std::isfinite(message.spellRange) || message.spellRange < MinSpellRange || message.spellRange > MaxSpellRange) return {};
        std::array<char, 64> rangeText{};
        const auto rangeResult = std::to_chars(rangeText.data(), rangeText.data() + rangeText.size(),
            message.spellRange, std::chars_format::general);
        if (rangeResult.ec != std::errc{}) return {};
        return "W2 S " + std::to_string(message.playerId) + " " + std::to_string(static_cast<int>(message.spell)) +
            " " + std::to_string(message.remainingDeciseconds) + " " + std::to_string(message.cooldownDeciseconds) +
            " " + (message.enabled ? "1" : "0") + " " + std::string(rangeText.data(), rangeResult.ptr);
    }
    if (message.kind == MessageKind::Feedback) {
        const int feedback = static_cast<int>(message.feedback);
        if (feedback < 0 || feedback > static_cast<int>(Feedback::Charging)) return {};
        return "W2 F " + std::to_string(message.playerId) + " " + std::to_string(feedback);
    }
    return {};
}

} // namespace wizard
