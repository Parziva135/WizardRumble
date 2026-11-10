#pragma once

#include "SpellReplay.h"
#include "SpellVisuals.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace wizard {
namespace replay_detail {

struct Quaternion { double w, x, y, z; };

inline std::optional<Quaternion> Unit(Quaternion q) {
    const double length = std::sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (!std::isfinite(length) || length < 0.000001) return std::nullopt;
    return Quaternion{q.w/length, q.x/length, q.y/length, q.z/length};
}

inline std::optional<Quaternion> Rotation(const SpellVisuals::Entity& entity) {
    const auto f = entity.forward.Normalized();
    const auto r = entity.right.Normalized();
    const auto u = entity.up.Normalized();
    if (f.LengthSquared() < .5f || r.LengthSquared() < .5f || u.LengthSquared() < .5f ||
        std::abs(f.Dot(r)) > .05f || std::abs(f.Dot(u)) > .05f || std::abs(r.Dot(u)) > .05f ||
        f.Cross(r).Dot(u) < .95f) return std::nullopt;
    // Forward, right and up are columns of the same right-handed matrix used
    // by the renderer. Quaternions avoid axis collapse at a 180-degree turn.
    const double m00=f.x, m01=r.x, m02=u.x;
    const double m10=f.y, m11=r.y, m12=u.y;
    const double m20=f.z, m21=r.z, m22=u.z;
    const double trace=m00+m11+m22;
    Quaternion q{};
    if (trace > 0) {
        const double s=std::sqrt(trace+1)*2;
        q={.25*s,(m21-m12)/s,(m02-m20)/s,(m10-m01)/s};
    } else if (m00 > m11 && m00 > m22) {
        const double s=std::sqrt(1+m00-m11-m22)*2;
        q={(m21-m12)/s,.25*s,(m01+m10)/s,(m02+m20)/s};
    } else if (m11 > m22) {
        const double s=std::sqrt(1+m11-m00-m22)*2;
        q={(m02-m20)/s,(m01+m10)/s,.25*s,(m12+m21)/s};
    } else {
        const double s=std::sqrt(1+m22-m00-m11)*2;
        q={(m10-m01)/s,(m02+m20)/s,(m12+m21)/s,.25*s};
    }
    return Unit(q);
}

inline Quaternion Slerp(Quaternion a, Quaternion b, double t) {
    double dot=a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z;
    if (dot < 0) { b={-b.w,-b.x,-b.y,-b.z}; dot=-dot; }
    dot=std::clamp(dot,-1.0,1.0);
    double x=1-t,y=t;
    if (dot < .9995) {
        const double angle=std::acos(dot), sine=std::sin(angle);
        x=std::sin((1-t)*angle)/sine; y=std::sin(t*angle)/sine;
    }
    return Unit({a.w*x+b.w*y,a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y}).value_or(a);
}

inline void Axes(SpellVisuals::Entity& entity, Quaternion q) {
    entity.forward={static_cast<float>(1-2*(q.y*q.y+q.z*q.z)),
        static_cast<float>(2*(q.x*q.y+q.w*q.z)),static_cast<float>(2*(q.x*q.z-q.w*q.y))};
    entity.right={static_cast<float>(2*(q.x*q.y-q.w*q.z)),
        static_cast<float>(1-2*(q.x*q.x+q.z*q.z)),static_cast<float>(2*(q.y*q.z+q.w*q.x))};
    entity.up={static_cast<float>(2*(q.x*q.z+q.w*q.y)),
        static_cast<float>(2*(q.y*q.z-q.w*q.x)),static_cast<float>(1-2*(q.x*q.x+q.y*q.y))};
}

inline Vec3 Lerp(Vec3 a, Vec3 b, double t) { return a+(b-a)*static_cast<float>(t); }

inline bool Continuous(Vec3 a, Vec3 b, double interval) {
    // Covers fast native balls and all spell velocities, but does not paint a
    // straight trail across a respawn, reset or discontinuous arena teleport.
    const double maximum=32+10000*interval;
    return a.IsFinite() && b.IsFinite() && (b-a).LengthSquared() <= maximum*maximum;
}

inline Vec3 Direction(Vec3 a, Vec3 b, double t) {
    a=a.Normalized(); b=b.Normalized();
    if (a.LengthSquared()<.5f || b.LengthSquared()<.5f) return a;
    const double dot=std::clamp(static_cast<double>(a.Dot(b)),-1.0,1.0);
    if (dot>.9995) return Lerp(a,b,t).Normalized();
    if (dot<-.9995) {
        auto perpendicular=a.Cross({0,0,1}).Normalized();
        if (perpendicular.LengthSquared()<.5f) perpendicular=a.Cross({0,1,0}).Normalized();
        return (a*static_cast<float>(std::cos(3.141592653589793*t))+
            perpendicular*static_cast<float>(std::sin(3.141592653589793*t))).Normalized();
    }
    const double angle=std::acos(dot), sine=std::sin(angle);
    return (a*static_cast<float>(std::sin((1-t)*angle)/sine)+
        b*static_cast<float>(std::sin(t*angle)/sine)).Normalized();
}

} // namespace replay_detail

// Only continuous poses and animation time are blended. The earlier snapshot
// owns every list and discrete flag; future casts, demos, impacts and spawned
// actors cannot leak into playback before their recorded timestamps.
inline SpellVisuals::Frame SmoothReplayFrame(const SpellVisuals::Frame& before,
    const SpellVisuals::Frame* after, double blend, double tailSeconds=0) {
    auto frame=before;
    if (!std::isfinite(before.time)) return frame;
    if (!after) {
        if (before.replayClockRunning && std::isfinite(tailSeconds) && tailSeconds>=0 &&
            tailSeconds<=ReplayHistory<int>::MaxGap+1e-9) frame.time+=tailSeconds;
        return frame; // No motion extrapolation after a capture gap or tail.
    }
    const double interval=after->time-before.time;
    if (!std::isfinite(blend) || !std::isfinite(interval) || interval<0 || interval>.25)
        return frame;
    blend=std::clamp(blend,0.0,1.0);
    frame.time=before.time+interval*blend;
    if (blend==0 || interval==0) return frame;
    if (before.ballValid && after->ballValid && before.ballKey && before.ballKey==after->ballKey &&
        replay_detail::Continuous(before.ball,after->ball,interval))
        frame.ball=replay_detail::Lerp(before.ball,after->ball,blend);
    for (auto& car:frame.cars) {
        if (!car.key || std::count_if(before.cars.begin(),before.cars.end(),
            [&](const auto& item){return item.key==car.key;})!=1) continue;
        const SpellVisuals::Entity* next=nullptr;
        bool ambiguous=false;
        for (const auto& item:after->cars) if (item.key==car.key) {
            if (next) { ambiguous=true; break; }
            next=&item;
        }
        if (!next || ambiguous || !replay_detail::Continuous(car.position,next->position,interval)) continue;
        car.position=replay_detail::Lerp(car.position,next->position,blend);
        const auto a=replay_detail::Rotation(car), b=replay_detail::Rotation(*next);
        if (a && b) replay_detail::Axes(car,replay_detail::Slerp(*a,*b,blend));
    }
    for (auto& ghost:frame.patronums) {
        if (std::count_if(before.patronums.begin(),before.patronums.end(),
            [&](const auto& item){return item.id==ghost.id && item.began==ghost.began;})!=1) continue;
        const SpellVisuals::Patronum* next=nullptr;
        bool ambiguous=false;
        for (const auto& item:after->patronums) if (item.id==ghost.id && item.began==ghost.began) {
            if (next) { ambiguous=true; break; }
            next=&item;
        }
        if (!next || ambiguous || next->ends!=ghost.ends || next->impact!=ghost.impact ||
            !replay_detail::Continuous(ghost.position,next->position,interval)) continue;
        ghost.position=replay_detail::Lerp(ghost.position,next->position,blend);
        ghost.forward=replay_detail::Direction(ghost.forward,next->forward,blend);
    }
    return frame;
}

} // namespace wizard
