#include "SpellCore.h"
#include "SpellReplay.h"
#include "SpellReplayAudio.h"
#include "SpellReplayScene.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

namespace {
int checks = 0;
void Check(bool condition, const char* description) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        std::exit(1);
    }
}
bool Near(double a, double b) { return std::abs(a - b) < 0.0001; }

void TestReplayAudio() {
    using wizard::ReplayAudioTrack;
    using wizard::SpellCue;
    ReplayAudioTrack track;
    const auto play = [](const std::vector<ReplayAudioTrack::Action>& actions, SpellCue cue, double offset) {
        return std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
            return action.cue == cue && action.play && Near(action.offsetSeconds, offset);
        });
    };
    const auto stop = [](const std::vector<ReplayAudioTrack::Action>& actions, SpellCue cue) {
        return std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
            return action.cue == cue && !action.play;
        });
    };
    Check(track.EventCount() == 0 && track.Advance(0, true).empty(), "empty replay audio is silent");
    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -1.0}) {
        Check(!track.Record(bad, SpellCue::Avada, 3), "invalid audio event timeline rejected");
        Check(!track.Record(0, SpellCue::Avada, bad), "invalid audio clip duration rejected");
    }
    Check(!track.Record(0, SpellCue::Count, 1) && !track.Record(0, static_cast<SpellCue>(-1), 1) &&
        !track.Record(0, SpellCue::Stupefy, 0) && !track.Record(0, SpellCue::Stupefy, 31),
        "invalid cue and unbounded or empty clips rejected");
    Check(track.Record(1, SpellCue::Avada, 3, 10, 20), "fresh voice event is recorded");
    Check(track.Advance(0.99, true).empty(), "replay audio never starts before cast time");
    Check(play(track.Advance(1, true), SpellCue::Avada, 0), "exact cast boundary starts voice");
    Check(track.Advance(1, true).empty() && track.Advance(1.1, true).empty(),
        "repeated and ordinary replay frames never restart a playing clip");
    Check(stop(track.Advance(1.2, false), SpellCue::Avada), "muting stops only active output");
    Check(track.Advance(1.3, false).empty(), "muted frames do not repeat stop actions");
    Check(play(track.Advance(1.4, true), SpellCue::Avada, 0.4), "unmuting rejoins the current voice at its offset");
    Check(stop(track.Advance(1.4, true, true), SpellCue::Avada), "game pause stops current output");
    Check(track.Advance(1.4, true, true).empty(), "held pause does not repeat stop actions");
    Check(play(track.Advance(1.4, true), SpellCue::Avada, 0.4), "resume at same timeline restarts only the active tail");
    auto actions = track.Advance(2.5, true);
    Check(actions.size() == 2 && !actions[0].play && play(actions, SpellCue::Avada, 1.5),
        "large forward seek stops output and joins destination offset without a cast burst");
    actions = track.Advance(1.5, true);
    Check(stop(actions, SpellCue::Avada) && play(actions, SpellCue::Avada, 0.5),
        "backward seek makes the earlier voice audible once again");
    Check(track.Advance(1.5, true).empty(), "repeated backward destination poll is silent");
    Check(stop(track.Advance(4, true), SpellCue::Avada) && track.Advance(4.1, true).empty(),
        "exact clip end stops and expired clips never restart");
    track.ResetPlayback();
    Check(play(track.Advance(2, true), SpellCue::Avada, 1), "goal replay beginning inside a clip joins its remaining audio");
    Check(stop(track.Advance(std::numeric_limits<double>::quiet_NaN(), true), SpellCue::Avada),
        "invalid playback clock stops active output safely");
    Check(play(track.Advance(2, true), SpellCue::Avada, 1), "valid clock after invalid interval rejoins correctly");

    track.Clear();
    track.Record(0, SpellCue::LeviosaCast, 2, 100, 200);
    track.Record(2, SpellCue::LeviosaFloat, 2, 100, 200);
    track.Record(2, SpellCue::Stupefy, 0.7, 300, 400);
    actions = track.Advance(2, true);
    Check(actions.size() == 2 && play(actions, SpellCue::LeviosaFloat, 0) && play(actions, SpellCue::Stupefy, 0),
        "float sound begins at lift start and can overlap the independent Stupefy channel");
    Check(track.Advance(2.01, true).empty(), "overlapping cue channels do not retrigger each render");
    track.Stop(2.1, SpellCue::LeviosaFloat, 100, 999);
    Check(track.Advance(2.1, true).empty(), "another car cannot cancel the current floating sound");
    track.Stop(2.2, SpellCue::LeviosaFloat, 100, 200);
    actions = track.Advance(2.2, true);
    Check(stop(actions, SpellCue::LeviosaFloat) && !stop(actions, SpellCue::Stupefy),
        "matching cancellation stops only its affected cue");
    track.ResetPlayback();
    Check(play(track.Advance(2.15, true), SpellCue::LeviosaFloat, 0.15),
        "cancel preserves the original audible past for replay seeks");
    track.Stop(2.3);
    actions = track.Advance(2.3, true);
    Check(stop(actions, SpellCue::LeviosaFloat) && stop(actions, SpellCue::Stupefy),
        "round clear truncates all active audio cue histories");

    track.Clear();
    track.Record(0, SpellCue::EatSlugs, 2, 1, 10);
    Check(play(track.Advance(0, true), SpellCue::EatSlugs, 0), "first same-channel voice starts");
    track.Record(0.2, SpellCue::EatSlugs, 2, 1, 20);
    actions = track.Advance(0.2, true);
    Check(actions.size() == 2 && !actions.front().play && actions.back().play,
        "new same-cue voice replaces the previous buffer in stop-before-play order");
    track.Stop(0.3, SpellCue::EatSlugs, 1, 10);
    Check(track.Advance(0.3, true).empty(), "old same-ID victim expiry leaves the new voice intact");
    track.ResetPlayback();
    Check(play(track.Advance(0.1, true), SpellCue::EatSlugs, 0.1), "replaced voice retains its earlier audible interval");
    actions = track.Advance(0.4, true);
    Check(play(actions, SpellCue::EatSlugs, 0.2), "replacement can be reached again after an earlier seek");
    Check(!track.Record(0.29, SpellCue::Stupefy, 1), "small backwards recording jitter does not reorder events");
    Check(track.Record(0, SpellCue::Stupefy, 1) && track.EventCount() == 1,
        "new recording timeline resets old match audio and playback bookkeeping");
    Check(play(track.Advance(0, true), SpellCue::Stupefy, 0), "fresh match reset does not inherit stale playback IDs");

    track.Clear();
    for (int i = 0; i < 2000; ++i) {
        track.Record(i / 100.0, SpellCue::Stupefy, 0.2);
        Check(track.EventCount() <= ReplayAudioTrack::MaxEvents, "audio event history stays bounded under cast spam");
    }
    track.ResetPlayback();
    Check(track.Advance(0.1, true).empty(), "evicted events never play when seeking outside retained audio history");
    track.Record(60, SpellCue::Avada, 3);
    Check(track.EventCount() == 1 && track.Advance(19.9, true).empty(),
        "long silent intervals age old audio out without filling the whole match");
    Check(play(track.Advance(60.1, true), SpellCue::Avada, 0.1), "latest retained voice remains seekable");
    track.Clear();
    Check(track.EventCount() == 0 && track.Advance(60.2, true).empty(), "session clear removes audio archive and cursor");
    track.Record(10, SpellCue::Avada, 3, 1, 2);
    track.Record(10, SpellCue::PetrificusCast, 1.5, 3, 4);
    actions = track.Advance(10.25, true);
    Check(actions.size() == 2 && play(actions, SpellCue::PetrificusCast, 0.25) &&
        play(actions, SpellCue::Avada, 0.25), "Petrificus has an independent replay sound channel");
    track.Stop(10.5, SpellCue::PetrificusCast, 3, 4);
    actions = track.Advance(10.5, true);
    Check(stop(actions, SpellCue::PetrificusCast) && !stop(actions, SpellCue::Avada),
        "canceling the Petrificus wind-up cannot stop another spell's voice");
    Check(track.Advance(10.5, true).empty(), "Petrificus cancellation does not retrigger on repeated replay frames");

    // Goal replays retain each locally independent voice, including the new
    // Patronum, Bombarda and Obliviate cast voices at their recorded timing.
    track.Clear();
    Check(static_cast<int>(SpellCue::Count) == 12, "all twelve spell audio buffers have replay channels");
    for (int i = 0; i < static_cast<int>(SpellCue::Count); ++i)
        Check(track.Record(20, static_cast<SpellCue>(i), 3, 1000 + i, 2000 + i),
            "each audio channel can record an overlapping cast independently");
    actions = track.Advance(20.25, true);
    Check(actions.size() == 12, "a replay beginning inside overlapping clips joins all twelve channels once");
    for (int i = 0; i < static_cast<int>(SpellCue::Count); ++i)
        Check(play(actions, static_cast<SpellCue>(i), 0.25), "every overlapping voice joins at the same recorded offset");
    Check(track.Advance(20.26, true).empty(), "overlapping new and retained voices do not repeat each frame");
    const auto bombardaChannel = static_cast<int>(SpellCue::BombardaCast);
    track.Stop(20.3, SpellCue::BombardaCast, 1000 + bombardaChannel, 9999);
    Check(track.Advance(20.3, true).empty(), "a different actor cannot stop Bombarda's charge voice");
    track.Stop(20.4, SpellCue::BombardaCast, 1000 + bombardaChannel, 2000 + bombardaChannel);
    actions = track.Advance(20.4, true);
    Check(actions.size() == 1 && stop(actions, SpellCue::BombardaCast) && !stop(actions, SpellCue::PatronumCast),
        "canceling Bombarda's exact charge leaves Patronum and every other voice playing");
    actions = track.Advance(20.5, false);
    Check(actions.size() == 11 && std::none_of(actions.begin(), actions.end(), [](const auto& action) { return action.play; }),
        "muting a replay stops all eleven remaining channels without restarting the canceled cast");
    actions = track.Advance(20.75, true);
    Check(actions.size() == 11 && play(actions, SpellCue::PatronumCast, 0.75) &&
        !play(actions, SpellCue::BombardaCast, 0.75),
        "unmuting resumes Patronum at the recorded offset and respects Bombarda cancellation");
    track.ResetPlayback();
    actions = track.Advance(20.2, true);
    Check(actions.size() == 12 && play(actions, SpellCue::BombardaCast, 0.2) && play(actions, SpellCue::PatronumCast, 0.2) &&
        play(actions, SpellCue::ObliviateCast, 0.2) && play(actions, SpellCue::AccioCast, 0.2) &&
        play(actions, SpellCue::ImperioCast, 0.2),
        "a backward seek restores both new voices in their original audible interval");
    track.Clear();
    Check(track.Advance(20.3, true).empty(), "round clear removes all twelve voice histories");

    track.Record(50, SpellCue::ObliviateCast, 2, 601, 602);
    Check(play(track.Advance(50, true), SpellCue::ObliviateCast, 0),
        "Obliviate has its own replay cue at cast acceptance");
    Check(track.Advance(50.5, true).empty() && track.Advance(51, true).empty(),
        "the half-second Obliviate gameplay charge does not restart or truncate a longer voice");
    track.ResetPlayback();
    Check(play(track.Advance(51.25, true), SpellCue::ObliviateCast, 1.25),
        "replay can join Obliviate's remaining natural clip after its charge has finished");
    track.Stop(51.3, SpellCue::ObliviateCast, 601, 999);
    Check(track.Advance(51.3, true).empty(), "another target cannot stop Obliviate's recorded voice");
    track.Stop(51.4, SpellCue::ObliviateCast, 601, 602);
    Check(stop(track.Advance(51.4, true), SpellCue::ObliviateCast),
        "exact actor cancellation stops only the owned Obliviate cue");
    track.Clear();

    track.Record(100, SpellCue::PatronumCast, 3.0, 501, 0);
    Check(play(track.Advance(100, true), SpellCue::PatronumCast, 0),
        "Patronum's three-second voice starts at the charge event");
    Check(track.Advance(100.4, true).empty() && track.Advance(100.8, true).empty(),
        "the Patronum wind-up voice does not restart during normal replay progression");
    track.ResetPlayback();
    Check(play(track.Advance(102.999, true), SpellCue::PatronumCast, 2.999),
        "a replay can join the final millisecond of Patronum's cast voice");
    Check(stop(track.Advance(103.0, true), SpellCue::PatronumCast) &&
        track.Advance(103.1, true).empty(),
        "the three-second clip ends exactly when the Patronum launch becomes eligible");
    track.ResetPlayback();
    Check(play(track.Advance(101.5, true), SpellCue::PatronumCast, 1.5),
        "seeking back from flight to charge restores the Patronum voice at its original offset");
    track.Stop(101.6, SpellCue::PatronumCast, 999, 0);
    Check(track.Advance(101.6, true).empty(), "an unrelated caster cannot truncate Patronum's charge voice");
    track.Stop(101.7, SpellCue::PatronumCast, 501, 0);
    Check(stop(track.Advance(101.7, true), SpellCue::PatronumCast),
        "canceling the original caster's Patronum charge stops its replay voice");
}

void TestReplayHistory() {
    using History = wizard::ReplayHistory<int>;
    History history;
    Check(history.Size() == 0 && !history.At(0), "empty replay history has no frame");
    for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        Check(!history.Record(invalid, 1, true) && history.Size() == 0,
            "nonfinite capture timestamps cannot enter replay history");
        Check(!history.At(invalid), "nonfinite replay seek has no frame");
    }
    Check(history.Record(10, 1), "first replay capture is accepted");
    Check(!history.Record(10, 2) && history.At(10)->value == 1,
        "ordinary duplicate capture preserves the accepted frame");
    Check(history.Record(10, 3, true) && history.Size() == 1 && history.At(10)->value == 3,
        "forced stationary-clock capture replaces rather than duplicates");
    Check(!history.Record(10.01, 4) && history.Size() == 1,
        "render callbacks faster than thirty hertz are downsampled");
    Check(history.Record(10.01, 5, true) && history.Size() == 1 && !history.At(10),
        "forced event updates the current interval without exposing its future value earlier");
    Check(history.At(10.01) && history.At(10.01)->value == 5,
        "forced event is visible at its exact replay timestamp");
    Check(history.Record(10.05, 6), "next sample interval accepts a live capture");
    Check(history.At(10.04) && history.At(10.04)->value == 5,
        "between-frame seeks use the previous sample and never a future sample");
    Check(history.At(10.05) && history.At(10.05)->value == 6,
        "exact replay seek selects the sample at that time");
    Check(!history.Record(10.03, 7, true) && history.Size() == 2 && history.At(10.05)->value == 6,
        "small backwards clock jitter cannot reorder or replace captured frames");
    Check(!history.At(10.009) && !history.At(9), "seeking before capture cannot borrow a future frame");
    Check(history.Record(11, 8), "capture resumes after a live callback gap");
    Check(!history.At(10.5), "replay cannot bridge a missing interval with stale spell effects");
    Check(history.At(10.15) && history.At(10.15)->value == 6,
        "a previous sample remains available for at most the gap allowance");
    Check(!history.At(10.151) && !history.At(11.101),
        "stale seeks between or after samples return no effect frame");
    const auto recordedSize = history.Size();
    Check(history.At(10.05) && history.At(11) && history.At(10.01) && history.Size() == recordedSize,
        "backward and forward replay seeks do not mutate live history");
    Check(history.Record(1, 9) && history.Size() == 1 && history.At(1)->value == 9 && !history.At(11),
        "a substantial backwards live timeline reset starts a fresh recording");
    history.Clear();
    Check(history.Size() == 0 && !history.At(1), "explicit session cleanup removes captured effects");

    // A long high-frequency session must retain only its recent bounded window.
    for (int i = 0; i <= 12000; ++i) {
        history.Record(i / 120.0, i);
        Check(history.Size() <= History::MaxSamples, "high-frequency replay recording obeys its entry cap");
    }
    Check(history.Size() >= 899 && history.Size() <= 902,
        "thirty-hertz capture retains approximately thirty seconds rather than the whole match");
    Check(!history.At(69.99) && history.At(70.04) && history.At(100),
        "age pruning retains the latest thirty seconds and discards earlier match effects");
    const auto latest = history.At(100);
    Check(latest && latest->value == 12000, "latest accepted capture remains queryable after pruning");

    // Constant-time goals or paused updates are bounded even when forced often.
    const auto boundedSize = history.Size();
    for (int i = 0; i < 2000; ++i) history.Record(100, i, true);
    Check(history.Size() == boundedSize && history.At(100)->value == 1999,
        "many same-time forced updates replace one snapshot without growing memory");
}

void TestReplaySmoothing() {
    using namespace wizard;
    using Frame = SpellVisuals::Frame;
    using History = ReplayHistory<Frame>;
    History history;
    Frame a, b;
    a.time = 20; a.ballValid = true; a.ballKey = 100; a.ball = {0, 0, 93};
    SpellVisuals::Entity car;
    car.key = 200; car.id = 1; car.position = {0, 0, 30};
    a.cars.push_back(car);
    a.patronums.push_back({1, {0, 100, 93}, {1, 0, 0}, 19, 21.5, -1, {}});
    a.spell = Spell::BombardaMaxima; a.spellCharging = true; a.chargeRemaining = 1;
    a.walls.push_back({1, {0, 0, 200}, {1, 0, 0}, 350, 19, 22, -1});
    b = a; b.time += 1.0 / 30; b.ball.x = 60; b.cars[0].position.x = 60;
    b.patronums[0].position.x = 40;
    b.spellCharging = false; b.chargeRemaining = 0; b.spell = Spell::None;
    b.walls.push_back({2, {1000, 0, 200}, {1, 0, 0}, 350, 20, 23, -1});
    auto newcomer = car; newcomer.key = 201; newcomer.position.x = 500;
    b.cars.push_back(newcomer);
    b.patronums.push_back({2, {1000, 100, 93}, {1, 0, 0}, 20, 22.5, -1, {}});
    Check(!history.AtSpan(100).before && !history.AtSpan(100).after,
        "an empty replay has no interpolation endpoints");
    history.Record(100, a); history.Record(100 + 1.0 / 30, b);
    Check(!history.AtSpan(99).before && !history.AtSpan(99).after &&
        !history.AtSpan(std::numeric_limits<double>::quiet_NaN()).before,
        "interpolation never borrows a future frame before capture or for invalid time");
    for (int frame = 0; frame < 4; ++frame) {
        const auto span = history.AtSpan(100 + frame / 120.0);
        Check(span.before && span.after && Near(span.blend, frame / 4.0),
            "30Hz replay capture supplies correctly bounded 120Hz interpolation fractions");
        const auto out = SmoothReplayFrame(span.before->value, &span.after->value, span.blend);
        Check(Near(out.time, 20 + frame / 120.0) && Near(out.ball.x, frame * 15) &&
            Near(out.cars[0].position.x, frame * 15) && Near(out.patronums[0].position.x, frame * 10),
            "replay visual clock, ball, car and same-flight ghost advance smoothly each display frame");
        Check(out.spell == Spell::BombardaMaxima && out.spellCharging && out.chargeRemaining == 1 &&
            out.cars.size() == 1 && out.patronums.size() == 1 && out.walls.size() == 1,
            "future spell release and newly spawned cars, ghosts and walls are not visible early");
    }
    const auto exact = history.AtSpan(100 + 1.0 / 30);
    Check(exact.before && exact.before->value.spell == Spell::None && !exact.after && exact.blend == 0,
        "the discrete new state becomes visible exactly at its captured timestamp");
    const auto mid = SmoothReplayFrame(a, &b, .5);
    const auto repeated = SmoothReplayFrame(a, &b, .5);
    Check(mid.cars[0].position.x == repeated.cars[0].position.x && mid.time == repeated.time &&
        history.Size() == 2 && history.At(100)->value.cars[0].position.x == 0,
        "repeated and backward replay seeks are deterministic and do not mutate archived poses");
    history.Record(101, b);
    const auto nearGap = history.AtSpan(100.05);
    Check(nearGap.before && !nearGap.after && !history.AtSpan(100.5).before,
        "the interpolation bracket does not bridge an unrecorded gap or draw stale effects");

    Frame changed = b;
    changed.cars[0].key = 999; changed.cars[0].id = a.cars[0].id;
    auto out = SmoothReplayFrame(a, &changed, .5);
    Check(out.cars[0].position.x == 0,
        "a respawn with the same numeric bot ID cannot inherit an older car's interpolation");
    changed = b; changed.cars[0].id = 99;
    out = SmoothReplayFrame(a, &changed, .5);
    Check(Near(out.cars[0].position.x, 30) && out.cars[0].id == 1,
        "a stable opaque car key survives numeric roster ID changes without changing earlier metadata");
    changed = b; changed.cars.push_back(changed.cars[0]);
    Check(SmoothReplayFrame(a, &changed, .5).cars[0].position.x == 0,
        "ambiguous duplicate car keys do not redirect interpolation");
    changed = b; changed.cars[0].position.x = 5000;
    Check(SmoothReplayFrame(a, &changed, .5).cars[0].position.x == 0,
        "a same-actor teleport is held rather than painting an interpolated trail through the stadium");
    changed = b; changed.ballKey = 101;
    Check(SmoothReplayFrame(a, &changed, .5).ball.x == 0,
        "ball actor replacement cannot borrow the previous ball's motion");
    changed = b; changed.ball.x = std::numeric_limits<float>::infinity();
    Check(SmoothReplayFrame(a, &changed, .5).ball.IsFinite(),
        "invalid recorded ball coordinates do not contaminate valid earlier replay data");
    changed = b; changed.patronums[0].impact = changed.time; changed.patronums[0].ends = changed.time + .45;
    out = SmoothReplayFrame(a, &changed, .5);
    Check(out.patronums[0].position.x == 0 && out.patronums[0].impact == -1,
        "a future Patronum impact cannot leak into the approaching flight phase");
    changed = b; changed.patronums[0].began = 20;
    Check(SmoothReplayFrame(a, &changed, .5).patronums[0].position.x == 0,
        "a new summon from the same caster cannot become the previous flight's interpolation endpoint");

    changed = b;
    changed.cars[0].forward = {-1, 0, 0}; changed.cars[0].right = {0, -1, 0};
    out = SmoothReplayFrame(a, &changed, .5);
    Check(Near(out.cars[0].forward.Length(), 1) && Near(out.cars[0].right.Length(), 1) &&
        Near(out.cars[0].up.Length(), 1) && Near(out.cars[0].forward.Dot(out.cars[0].right), 0) &&
        out.cars[0].forward.Cross(out.cars[0].right).Dot(out.cars[0].up) > .999 &&
        std::abs(out.cars[0].forward.y) > .999,
        "quaternion interpolation remains an orthonormal car rotation through a 180-degree turn");
    const auto yaw = [](SpellVisuals::Entity& entity, double radians) {
        entity.forward = {static_cast<float>(std::cos(radians)), static_cast<float>(std::sin(radians)), 0};
        entity.right = {-static_cast<float>(std::sin(radians)), static_cast<float>(std::cos(radians)), 0};
        entity.up = {0, 0, 1};
    };
    auto rotated = a;
    yaw(rotated.cars[0], 179 * 3.141592653589793 / 180);
    changed = b; yaw(changed.cars[0], -179 * 3.141592653589793 / 180);
    out = SmoothReplayFrame(rotated, &changed, .5);
    Check(out.cars[0].forward.x < -.9999 && std::abs(out.cars[0].forward.y) < .001,
        "orientation crosses the short two-degree route across yaw wrap rather than spinning backwards");
    changed = b; changed.cars[0].forward = {}; changed.cars[0].right = {};
    out = SmoothReplayFrame(a, &changed, .5);
    Check(out.cars[0].forward.x == 1 && out.cars[0].right.y == 1,
        "invalid future axes retain a valid earlier orientation");
    changed = b; changed.patronums[0].forward = {-1, 0, 0};
    out = SmoothReplayFrame(a, &changed, .5);
    Check(Near(out.patronums[0].forward.Length(), 1) && std::abs(out.patronums[0].forward.y) > .999,
        "ghost direction remains normalized through a 180-degree homing turn");

    out = SmoothReplayFrame(a, nullptr, 0, .05);
    Check(Near(out.time, 20.05) && out.ball.x == 0 && out.cars[0].position.x == 0,
        "a short capture tail advances animation time without extrapolating native object motion");
    auto paused = a; paused.replayClockRunning = false;
    Check(SmoothReplayFrame(paused, nullptr, 0, .05).time == 20 &&
        SmoothReplayFrame(a, nullptr, 0, .101).time == 20,
        "paused captures and stale tails cannot advance the replay animation clock");
    changed = b; changed.time = a.time;
    Check(SmoothReplayFrame(a, &changed, .5).ball.x == 0,
        "a stationary source clock does not interpolate native poses during a pause");
    changed = b; changed.time = a.time - 1;
    Check(SmoothReplayFrame(a, &changed, .5).time == a.time &&
        SmoothReplayFrame(a, &b, std::numeric_limits<double>::infinity()).time == a.time,
        "backwards or invalid interpolation clocks preserve the earlier scene");
}

void TestChargedSpells() {
    using namespace wizard;
    Check(AvadaChargeSeconds == 2.0 && LeviosaChargeSeconds == 2.0,
        "Avada and Leviosa retain their two-second charge");
    Check(EatSlugsChargeSeconds == 1.5, "Eat Slugs has a one-and-a-half-second wind-up");
    Check(StupefyChargeSeconds == 0.65 && StupefyChargeSeconds >= 0.5 && StupefyChargeSeconds <= 0.75,
        "Stupefy has the requested short 0.65-second wind-up");
    Check(PetrificusChargeSeconds == 1.5, "Petrificus has the requested one-and-a-half-second wind-up");
    Check(ProtegoChargeSeconds == 1.0, "Protego has the requested one-second wind-up");
    Check(BombardaChargeSeconds == 2.0 && Spell::Depulso == Spell::BombardaMaxima &&
        static_cast<int>(Spell::BombardaMaxima) == 4,
        "Bombarda Maxima keeps spell ID 4 and adds a two-second ball charge");
    Check(PatronumChargeSeconds == 3.0 && SpellChargeSeconds(Spell::ExpectoPatronum) == 3.0,
        "Patronum waits for the full three-second cast before launching its ghost");
    Check(ObliviateChargeSeconds == 0.5 && SpellChargeSeconds(Spell::Obliviate) == 0.5 &&
        Spell::Confundo == Spell::Obliviate && static_cast<int>(Spell::Obliviate) == 13,
        "Obliviate replaces Confundo at ID 13 and has a half-second wind-up");
    Check(AccioChargeSeconds == 0.5 && SpellChargeSeconds(Spell::Accio) == 0.5,
        "Accio charges for half a second before its three-second summon");
    Check(IsPlayerTargetSpell(Spell::Imperio) && IsChargedSpell(Spell::Imperio) &&
        ImperioChargeSeconds == 1.0 && SpellChargeSeconds(Spell::Imperio) == 1.0 &&
        ImperioDurationSeconds == 3.5 && ImperioDurationSeconds >= 3 && ImperioDurationSeconds <= 4,
        "Imperio charges for one second before mirroring for three and a half seconds");
    for (const auto spell : ActiveSpells) {
        const bool charged = spell == Spell::AvadaKedavra || spell == Spell::Leviosa ||
            spell == Spell::EatSlugs || spell == Spell::Stupefy || spell == Spell::PetrificusTotalus ||
            spell == Spell::Protego || spell == Spell::BombardaMaxima || spell == Spell::ExpectoPatronum ||
            spell == Spell::Obliviate || spell == Spell::Accio || spell == Spell::Imperio;
        Check(IsChargedSpell(spell) == charged, "all seven opponent charges and four ball charges are configured");
        const double expected = spell == Spell::EatSlugs ? 1.5 : spell == Spell::Stupefy ? 0.65 :
            spell == Spell::PetrificusTotalus ? 1.5 : spell == Spell::Protego ? 1.0 :
            spell == Spell::ExpectoPatronum ? 3.0 : spell == Spell::Obliviate || spell == Spell::Accio ? 0.5 :
            spell == Spell::Imperio ? 1.0 : charged ? 2.0 : 0.0;
        Check(SpellChargeSeconds(spell) == expected,
            "each charged spell uses its own delay while instant spells have no delay");
        Check(IsValidChargeTarget(spell, -1) == (charged && IsBallTargetSpell(spell)) &&
            IsValidChargeTarget(spell, 0) == (charged && IsPlayerTargetSpell(spell)) &&
            IsValidChargeTarget(spell, MaxPlayerId) == (charged && IsPlayerTargetSpell(spell)),
            "ball charges use the ball sentinel and player charges use concrete roster IDs");
        Check(!IsValidChargeTarget(spell, -2) && !IsValidChargeTarget(spell, MaxPlayerId + 1),
            "no charge accepts an invalid target sentinel or overflowing roster ID");
    }
    for (const int id : {-1, 0, 5, 7, 10, 11, 12, 17, 100}) {
        Check(!IsChargedSpell(static_cast<Spell>(id)) && SpellChargeSeconds(static_cast<Spell>(id)) == 0,
            "empty, instant, removed and invalid spells cannot charge");
    }
    Check(static_cast<int>(Spell::EatSlugs) == 14 && static_cast<int>(Spell::ExpectoPatronum) == 15 &&
        static_cast<int>(Spell::Imperio) == 16 && MaxSpellId == 16,
        "Imperio appends ID 16 without reusing removed IDs or changing other spells");
    Check(EatSlugsDurationSeconds == 3.75 && EatSlugsDurationSeconds >= 3 && EatSlugsDurationSeconds <= 4,
        "Eat Slugs has the requested three-to-four-second obstruction duration");
    Check(SlugFlightSeconds == 0.35 && SlugFlightSeconds > 0 && SlugFlightSeconds < EatSlugsChargeSeconds,
        "slugs have a short separate projectile flight after their charge");
}

void TestAccioGuidance() {
    using namespace wizard;
    Check(AvadaChargeSeconds == 2.0, "Avada has a two-second charge");
    Check(AccioDurationSeconds == 3.0, "Accio releases guidance after at most three active seconds");
    Check(AccioApproachSeconds(0) == 1.2 && AccioApproachSeconds(5500) == 2.8,
        "Accio approach duration scales between 1.2 and 2.8 seconds");
    const Vec3 front = AccioTarget({10, 20, 17}, {2, 0, 1});
    Check(Near(front.x, 270) && Near(front.y, 20) && Near(front.z, 110),
        "Accio aims ahead of the horizontal nose and above the floor");
    Check(AccioTarget({0, 0, 500}, {0, 0, 1}).x == 260 &&
        AccioTarget({0, 0, 500}, {0, 0, 1}).z == 593,
        "vertical nose keeps a finite airborne target clear of the car");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (double invalid : {0.0, -1.0, static_cast<double>(nan), static_cast<double>(infinity)}) {
        Check(!GuideAccio({}, {}, {}, {}, invalid, 2), "Accio rejects invalid frame time");
        Check(!GuideAccio({}, {}, {}, {}, 0.016, invalid), "Accio rejects invalid approach duration");
    }
    Check(!GuideAccio({nan, 0, 0}, {}, {}, {}, 0.016, 2) &&
        !GuideAccio({}, {infinity, 0, 0}, {}, {}, 0.016, 2) &&
        !GuideAccio({}, {}, {nan, 0, 0}, {}, 0.016, 2) &&
        !GuideAccio({}, {}, {}, {infinity, 0, 0}, 0.016, 2),
        "Accio rejects every nonfinite body or target input");
    int timedOut = 0;
    for (const float distance : {100.0f, 1500.0f, 5500.0f}) {
        for (const int rate : {30, 60, 144}) {
            for (int scenario = 0; scenario < 5; ++scenario) {
                const double approach = AccioApproachSeconds(distance);
                const double timeout = AccioDurationSeconds;
                double now = 0, settled = 0;
                Vec3 ball = AccioTarget({0, 0, 17}, {1, 0, 0}) + Vec3{distance, 0, 250};
                Vec3 velocity = scenario == 4 ? Vec3{1000, 400, 300} : Vec3{};
                Vec3 target{}, targetVelocity{};
                bool completed = false;
                while (now < timeout) {
                    const double requestedStep = scenario == 4 && now > 0.4 && now < 0.4 + 1.0 / rate ? 0.25 : 1.0 / rate;
                    const double step = std::min(requestedStep, timeout - now);
                    const float yaw = scenario == 3 ? static_cast<float>(now) * 0.45f : 0.0f;
                    const Vec3 carVelocity = scenario == 1 ? Vec3{-2300, 0, 0} :
                        scenario == 2 ? Vec3{0, 1000, 0} : scenario == 3 ? Vec3{400, 100, 0} : Vec3{};
                    const Vec3 car = Vec3{0, 0, 17} + carVelocity * static_cast<float>(now);
                    target = AccioTarget(car, {std::cos(yaw), std::sin(yaw), 0});
                    targetVelocity = carVelocity + (scenario == 3 ?
                        Vec3{-std::sin(yaw), std::cos(yaw), 0} * (260 * 0.45f) : Vec3{});
                    const auto guided = GuideAccio(ball, velocity, target, targetVelocity, step, approach);
                    Check(guided.has_value(), "Accio simulation produces finite guidance");
                    Check(guided->velocity.Length() <= AccioMaxSpeed + 0.01f,
                        "Accio flight stays within physical speed cap");
                    Check((guided->velocity - velocity).Length() <=
                        AccioMaxAcceleration * static_cast<float>(std::min(step, 0.1)) + 0.01f,
                        "Accio acceleration stays bounded across frame rates and a hitch");
                    settled = guided->arrived ? settled + step : 0;
                    if (settled >= AccioSettleSeconds) { completed = true; break; }
                    velocity = guided->velocity;
                    ball = ball + velocity * static_cast<float>(step);
                    now += step;
                }
                Check(now <= timeout, "Accio guidance never extends beyond its three-second lifetime");
                if (completed) {
                    Check((ball - target).Length() <= 35 && (velocity - targetVelocity).Length() <= 120,
                        "Accio arrival stays controlled relative to static, moving or turning caster");
                } else {
                    ++timedOut;
                    Check(now == timeout && ball.IsFinite() && velocity.IsFinite(),
                        "Accio can hand back a finite in-flight ball at the three-second timeout");
                }
                Check(completed || distance > 1500,
                    "shorter Accio lifetime still settles nearby static, moving and turning targets");
            }
        }
    }
    Check(timedOut > 0, "distant Accio trajectories exercise timeout instead of extending the spell");
}

void TestPatronum() {
    using namespace wizard;
    Check(PatronumSpeed == 1700 && PatronumSpeed > 1000 && PatronumSpeed < 2500 && PatronumTimeoutSeconds == 2.5 &&
        PatronumShotSpeed == 2400 && PatronumShotSpeed > 1500 && PatronumShotSpeed < 4200 && PatronumHitRadius == 85,
        "the larger Patronum has moderate 1700 movement and 2400 shot speed without extending its lifetime");
    Check(static_cast<int>(Spell::PetrificusTotalus) == 9 && Spell::ArrestoMomentum == Spell::PetrificusTotalus,
        "Petrificus preserves the former hold's wire identity");
    Check(std::string(GetSpellInfo(Spell::PetrificusTotalus).identifier) == "petrificus_totalus" &&
        std::string(GetSpellInfo(Spell::PetrificusTotalus).name) == "Petrificus Totalus" &&
        PetrificusDurationSeconds == 2.0,
        "Petrificus has its new public name and retains the two-second body hold");
    Check(IsBallTargetSpell(Spell::ExpectoPatronum) && !IsPlayerTargetSpell(Spell::ExpectoPatronum) &&
        IsChargedSpell(Spell::ExpectoPatronum) && SpellChargeSeconds(Spell::ExpectoPatronum) == 3.0,
        "Patronum charges against the ball rather than acquiring an opposing player");
    Check(std::string(GetSpellInfo(Spell::ExpectoPatronum).identifier) == "expecto_patronum",
        "Patronum exposes its practice identifier");
    const auto spawn = PatronumSpawn({10, 20, 17}, {0, 2, 0});
    Check(spawn && Near(spawn->x, 10) && Near(spawn->y, 160) && Near(spawn->z, 87),
        "mini car spawns ahead of the caster, above the pitch");
    const auto fallback = PatronumSpawn({0, 0, -30}, {});
    Check(fallback && fallback->x == 140 && fallback->z == 60,
        "invalid orientation falls back safely and spawn stays above the floor");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    Check(!PatronumSpawn({nan, 0, 0}, {1, 0, 0}) && !PatronumSpawn({}, {infinity, 0, 0}),
        "nonfinite car transforms cannot create a phantom");
    for (const float goalSide : {-1.0f, 1.0f}) {
        const auto shot = PatronumShotVelocity({500, 0, 93}, {0, goalSide * 5120, 300});
        Check(shot && shot->IsFinite() && std::abs(shot->Length() - PatronumShotSpeed) < 0.01f &&
            shot->y * goalSide > 0 && shot->x < 0 && shot->z > 0,
            "the one-shot velocity points toward the supplied opposing goal on either team");
    }
    const auto capped = PatronumShotVelocity({}, {1, 0, 0}, 100000);
    const auto slower = PatronumShotVelocity({}, {1, 0, 0}, 1000);
    Check(capped && capped->x == PatronumShotSpeed && slower && slower->x == 1000,
        "shot speed is bounded while lower requested speeds remain valid");
    Check(!PatronumShotVelocity({}, {}) && !PatronumShotVelocity({nan, 0, 0}, {1, 0, 0}) &&
        !PatronumShotVelocity({}, {infinity, 0, 0}) && !PatronumShotVelocity({}, {1, 0, 0}, 0) &&
        !PatronumShotVelocity({}, {1, 0, 0}, nan),
        "invalid or coincident goal aiming never writes a bad ball velocity");
    for (const double bad : {0.0, -0.1, 2.500001, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()})
        Check(!AdvancePatronum({}, {1000, 0, 93}, {1000, 0, 93}, bad), "flight rejects invalid or expired frame intervals");
    for (const float bad : {0.0f, 9.0f, 1001.0f, nan, infinity})
        Check(!AdvancePatronum({}, {1000, 0, 93}, {1000, 0, 93}, 0.016, bad), "flight validates the native ball radius");
    Check(!AdvancePatronum({nan, 0, 0}, {}, {}, 0.016) &&
        !AdvancePatronum({}, {infinity, 0, 0}, {}, 0.016) &&
        !AdvancePatronum({}, {}, {nan, 0, 0}, 0.016), "every nonfinite ghost/ball position is rejected");

    for (const double dt : {1.0 / 30, 1.0 / 60, 1.0 / 120}) {
        for (const float distance : {100.0f, 1000.0f, 2500.0f, 4200.0f, 4420.0f, 4430.0f, 5500.0f, 10000.0f}) {
            const Vec3 ball{distance, 0, 93};
            Vec3 ghost{0, 0, 93};
            bool hit = false;
            double time = 0;
            int strikes = 0;
            while (time < PatronumTimeoutSeconds && !hit) {
                const double stepSeconds = std::min(dt, PatronumTimeoutSeconds - time);
                const auto step = AdvancePatronum(ghost, ball, ball, stepSeconds);
                Check(step && step->position.IsFinite() && step->forward.IsFinite(), "stationary-ball flight remains finite at each tested frame rate");
                if (!step) break;
                Check((step->position - ghost).Length() <= PatronumSpeed * stepSeconds + 0.01,
                    "homing advance cannot exceed its per-frame speed bound");
                ghost = step->position; time += stepSeconds; hit = step->hit;
                if (hit) ++strikes;
            }
            const double travelTime = std::max(0.0, static_cast<double>(distance - (93 + PatronumHitRadius))) / PatronumSpeed;
            if (travelTime <= PatronumTimeoutSeconds) {
                Check(hit && strikes == 1 && time <= travelTime + dt + 0.002,
                    "a reachable stationary ball is struck once within its slower travel bound");
                if (distance == 2500) Check(time >= 1.365 && time <= 1.366 + dt,
                    "a 2500-unit stationary target takes about 1.37 seconds to reach at the middle tuning");
            } else Check(!hit && strikes == 0,
                "farther stationary balls can outlast the unchanged 2.5-second ghost lifetime");
        }
    }
    for (const Vec3 velocity : {Vec3{6000, 0, 0}, Vec3{0, 5000, 0}, Vec3{-6000, 0, 0}}) {
        Vec3 ghost{0, 0, 93}, ball{10000, 0, 93};
        const double dt = 1.0 / 60;
        bool hit = false;
        for (double time = 0; time < PatronumTimeoutSeconds && !hit; time += dt) {
            const Vec3 nextBall = ball + velocity * static_cast<float>(dt);
            const auto step = AdvancePatronum(ghost, ball, nextBall, dt);
            Check(step && step->position.IsFinite(), "receding and transverse fast-ball interception stays finite");
            if (!step) break;
            hit = step->hit; ghost = step->position; ball = nextBall;
        }
        Check(hit == (velocity.x < 0),
            "the slower ghost can catch an approaching ball while faster escaping balls can outrun it");
    }
    const auto crossing = AdvancePatronum({}, {-1000, 0, 0}, {1000, 0, 0}, 0.1);
    Check(crossing && crossing->hit, "relative swept collision catches a ball crossing between native frames");
    const auto hitch = AdvancePatronum({}, {10000, 0, 0}, {16000, 0, 0}, 1.0);
    Check(hitch && hitch->position.IsFinite() && (hitch->hit || hitch->position.Length() <= PatronumSpeed + 0.1f),
        "a long native hitch is simulated with bounded substeps and travel");
    const auto boundary = AdvancePatronum({}, {178, 0, 0}, {178, 0, 0}, 0.000001);
    const auto outside = AdvancePatronum({}, {178.1f, 0, 0}, {178.1f, 0, 0}, 0.000001);
    Check(boundary && boundary->hit && outside && !outside->hit,
        "ball plus ghost contact radius includes the edge without expanding it");
}

void TestSuspension() {
    using namespace wizard;
    const Vec3 ground{10, 20, 0};
    Check(LeviosaRiseSpeed == 700 && LeviosaRoofDropSpeed == 150 && LeviosaSafetySeconds == 8,
        "roof lift has a bounded speed and emergency lifetime instead of a three-second plateau");
    Check(Near(SuspensionPosition(ground, LeviosaRiseSpeed, 0).z, 0), "lift starts at current position");
    Check(Near(SuspensionPosition(ground, LeviosaRiseSpeed, 0.5).z, 350), "free-space lift is linear from its current start");
    Check(Near(SuspensionPosition(ground, LeviosaRiseSpeed, 1).z, 700), "lift continues after its old ramp time");
    Check(Near(SuspensionPosition(ground, LeviosaRiseSpeed, 3).z, 2100), "lift has no fixed 650-unit plateau");
    Check(Near(SuspensionPosition({10, 20, 1950}, 0, 1).z, 1950), "zero-rise hold never lowers a high car");
    Check(Near(SuspensionPosition(ground, -650, 1).z, 0), "negative rise preserves position");
    Check(Near(SuspensionPosition({10, 20, 1700}, 650, 1).z, 2350), "free-space diagnostics do not invent a roof at 1800");
    Check(Near(SuspensionPosition({10, 20, 1950}, 650, 1).z, 2600), "high cars continue rising until actual collision");
    Check(Near(SuspensionPosition(ground, 650, -1).z, 0), "negative elapsed time preserves start height");
    const auto midway = SuspensionPosition(ground, 650, 0.325);
    Check(Near(midway.x, ground.x) && Near(midway.y, ground.y), "lift preserves horizontal position");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    Check(Near(SuspensionPosition(ground, 650, nan).z, 0), "nonfinite elapsed time preserves position");
    Check(Near(SuspensionPosition(ground, 650, infinity).z, 0), "infinite elapsed time preserves position");
    Check(Near(SuspensionPosition(ground, static_cast<float>(nan), 1).z, 0), "nonfinite rise preserves position");
    Check(Near(SuspensionPosition(ground, static_cast<float>(infinity), 1).z, 0), "infinite rise preserves position");
    const Vec3 invalid{static_cast<float>(nan), 20, 50};
    const auto unchanged = SuspensionPosition(invalid, 650, 1);
    Check(std::isnan(unchanged.x) && unchanged.y == invalid.y && unchanged.z == invalid.z,
        "invalid start is returned unchanged without computing a new position");
    Check(Near(SuspensionPosition(ground, 700, std::numeric_limits<double>::max()).z, ground.z),
        "overflowing diagnostic trajectories cannot propagate invalid positions");

    const Vec3 car{0, 0, 1980}, roof{0, 0, 2040};
    Check(IsLeviosaRoofContact(car, true, roof, {0, 0, -1}), "actual overhead world contact releases at roof");
    Check(IsLeviosaRoofContact(car, true, roof, {0, 0, 1}),
        "contact geometry is robust to undocumented normal orientation");
    Check(IsLeviosaRoofContact({100, 300, 300}, true, {130, 300, 350}, {.8f, 0, -.6f}),
        "nearby curved upper ceiling releases without a hardcoded arena height");
    Check(!IsLeviosaRoofContact({}, true, {80, 0, 10}, {-.8f, 0, .6f}) &&
        !IsLeviosaRoofContact({}, true, {80, 0, 10}, {.8f, 0, -.6f}),
        "upward-facing curved base above the car origin is not a roof under either normal convention");
    Check(IsLeviosaRoofContact({}, true, {30, 0, 50}, {.8f, 0, -.6f}) &&
        IsLeviosaRoofContact({}, true, {30, 0, 50}, {-.8f, 0, .6f}),
        "upper curved roof qualifies with either reported normal convention");
    Check(IsLeviosaRoofContact({0, 0, 2700}, true, {0, 0, 2760}, {0, 0, -1}),
        "custom high roofs are supported by contact geometry");
    Check(!IsLeviosaRoofContact(car, false, roof, {0, 0, -1}), "no world contact cannot release lift");
    Check(!IsLeviosaRoofContact(car, true, {0, 0, 1940}, {0, 0, 1}), "floor contact is not a roof");
    Check(!IsLeviosaRoofContact(car, true, {60, 0, 2000}, {-1, 0, 0}), "vertical side wall is not a roof");
    Check(!IsLeviosaRoofContact(car, true, {60, 0, 2000}, {1, 0, .1f}), "nearly vertical walls do not trigger early release");
    Check(!IsLeviosaRoofContact(car, true, {0, 0, 2180}, {0, 0, -1}), "distant stale ceiling contact is rejected");
    Check(!IsLeviosaRoofContact(car, true, roof, {}), "zero contact normal is rejected");
    Check(!IsLeviosaRoofContact(car, true, {static_cast<float>(nan), 0, 2040}, {0, 0, -1}),
        "nonfinite contact position is rejected");
    Check(!IsLeviosaRoofContact(car, true, roof, {0, static_cast<float>(nan), -1}),
        "nonfinite contact normal is rejected");
    for (const float reach : {0.0f, -1.0f, 2001.0f, static_cast<float>(nan), static_cast<float>(infinity)})
        Check(!IsLeviosaRoofContact(car, true, roof, {0, 0, -1}, reach), "invalid body reach is rejected");
}

void TestTargeting() {
    using namespace wizard;
    const Vec3 origin{0, 0, 0}, forward{2, 0, 0};
    std::vector<TargetCandidate> targets{
        {2, {2000, 0, 0}, true}, {3, {100, 20, 0}, true},
        {4, {50, 0, 0}, false}, {5, {-50, 0, 0}, true}, {6, {DefaultSpellRange + 100, 0, 0}, true}
    };
    Check(SelectTarget(origin, forward, targets) == 2, "alignment beats proximity; ineligible, behind, and distant targets ignored");
    targets.push_back({8, {1000, 0, 0}, true});
    Check(SelectTarget(origin, forward, targets) == 8, "equal alignment prefers nearest");
    targets.push_back({7, {1000, 0, 0}, true});
    Check(SelectTarget(origin, forward, targets) == 7, "equal distance and alignment uses stable player ID");
    std::reverse(targets.begin(), targets.end());
    Check(SelectTarget(origin, forward, targets) == 7, "target selection independent of array order");
    Check(!SelectTarget(origin, {}, targets), "zero forward vector has no target");
    Check(!SelectTarget(origin, forward, targets, -1), "invalid range rejected");
    Check(!SelectTarget(origin, forward, targets, 4500, 1.1f), "invalid cosine rejected");
    Check(!SelectTarget(origin, forward, {{0, {0, 0, 0}, true}}), "coincident target ignored");
    Check(!SelectTarget(origin, forward, {{-1, {20, 0, 0}, true}}), "invalid player ID ignored");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Check(!SelectTarget({nan, 0, 0}, forward, targets), "nonfinite origin rejected");
    Check(!SelectTarget(origin, forward, {{1, {nan, 0, 0}, true}}), "nonfinite target ignored");
    Check(SelectTarget(origin, forward, {{1, {DefaultSpellRange, 0, 0}, true}}) == 1, "range edge included");
    Check(!SelectTarget(origin, forward, {{1, {DefaultSpellRange + 1, 0, 0}, true}}), "beyond new default player range excluded");
    Check(SelectTarget(origin, forward, {{1, {5000, 0, 0}, true}}) == 1, "player target beyond old range now included");
    Check(!SelectTarget(origin, forward, {{1, {100, 100, 0}, true}}), "outside cone ignored");
    const Vec3 normalized = Vec3{3, 4, 0}.Normalized();
    Check(Near(normalized.x, 0.6) && Near(normalized.y, 0.8), "vectors normalize");
    Check(Vec3{nan, 0, 0}.Normalized().LengthSquared() == 0, "invalid vector normalization is safe");
}

void TestSpellTargetsAndRange() {
    using namespace wizard;
    const std::set<Spell> playerSpells{Spell::AvadaKedavra, Spell::Leviosa, Spell::Stupefy,
        Spell::PetrificusTotalus, Spell::Obliviate, Spell::EatSlugs, Spell::Imperio};
    const std::set<Spell> ballSpells{Spell::Accio, Spell::BombardaMaxima, Spell::Protego, Spell::ExpectoPatronum};
    for (const auto spell : ActiveSpells) {
        Check(IsPlayerTargetSpell(spell) == (playerSpells.count(spell) != 0), "player-target spell classification");
        Check(IsBallTargetSpell(spell) == (ballSpells.count(spell) != 0), "ball-target spell classification");
        Check(IsPlayerTargetSpell(spell) != IsBallTargetSpell(spell), "every active spell targets exactly one kind");
    }
    for (const int id : {-1, 0, 5, 7, 10, 11, 12, 100}) {
        Check(!IsPlayerTargetSpell(static_cast<Spell>(id)) && !IsBallTargetSpell(static_cast<Spell>(id)),
            "charging, removed and invalid spell IDs cannot acquire targets");
    }
    const Vec3 origin{10, -20, 30};
    Check(InSpellRange(origin, origin), "coincident ball remains in range");
    Check(InSpellRange(origin, origin + Vec3{3300, 4400, 0}), "default range includes three-dimensional distance edge");
    Check(!InSpellRange(origin, origin + Vec3{3300, 4401, 0}), "beyond default range rejected");
    Check(InSpellRange(origin, origin + Vec3{5000, 0, 0}), "ball target beyond old range now included");
    Check(InSpellRange(origin, origin + Vec3{0, 0, 500}, 500), "custom range edge included vertically");
    Check(!InSpellRange(origin, origin + Vec3{0, 0, 500.01f}, 500), "just beyond custom range rejected");
    const float custom = 2345.6787f;
    Check(InSpellRange({}, {custom, 0, 0}, custom), "fractional host range includes exact boundary");
    Check(!InSpellRange({}, {std::nextafter(custom, std::numeric_limits<float>::infinity()), 0, 0}, custom),
        "next representable position past fractional host range rejected");
    Check(!SelectTarget({}, {1, 0, 0}, {{1, {custom, 0, 0}, true}}, 500), "player targeting uses custom shorter range");
    Check(SelectTarget({}, {1, 0, 0}, {{1, {custom, 0, 0}, true}}, custom) == 1,
        "player and ball targeting agree at custom range boundary");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const float range : {0.0f, -1.0f, nan, inf})
        Check(!InSpellRange(origin, origin, range), "zero, negative and nonfinite ranges rejected");
    Check(!InSpellRange({nan, 0, 0}, origin), "nonfinite range origin rejected");
    Check(!InSpellRange(origin, {inf, 0, 0}), "nonfinite target position rejected");
    Check(!InSpellRange({std::numeric_limits<float>::max(), 0, 0},
        {-std::numeric_limits<float>::max(), 0, 0}), "overflowing target displacement rejected");
}

void TestObliviateControls() {
    using namespace wizard;
    struct Input {
        float Throttle, Steer;
        float Pitch = 0.2f, Yaw = -0.3f, Roll = 0.4f;
        float DodgeForward = -0.5f, DodgeStrafe = 0.6f;
        bool Jump = true, ActivateBoost = true, HoldingBoost = false, Handbrake = true;
    };
    for (const float throttle : {-1.0f, -0.25f, 0.0f, 0.375f, 1.0f}) {
        for (const float steer : {-1.0f, -0.6f, 0.0f, 0.125f, 1.0f}) {
            Input input{throttle, steer};
            InvertDirectionalControls(input);
            Check(input.Throttle == -throttle && input.Steer == -steer,
                "Obliviate reverses keyboard directions and fractional controller axes");
            Check(input.Pitch == -0.2f && input.Yaw == 0.3f && input.Roll == -0.4f &&
                input.DodgeForward == 0.5f && input.DodgeStrafe == -0.6f,
                "Obliviate reverses aerial and dodge directions with driving input");
            Check(input.Jump && input.ActivateBoost && !input.HoldingBoost && input.Handbrake,
                "Obliviate preserves jump, boost and handbrake buttons");
        }
    }
    Input excessive{2.0f, -4.0f};
    InvertDirectionalControls(excessive);
    Check(excessive.Throttle == -1 && excessive.Steer == 1, "inverted driving axes remain normalized");
    Input invalid{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
    InvertDirectionalControls(invalid);
    Check(invalid.Throttle == 0 && invalid.Steer == 0, "invalid driving axes cannot propagate through inversion");
    Check(ObliviateDurationSeconds == 5 && ConfundoDurationSeconds == ObliviateDurationSeconds,
        "Obliviate retains a five-second control scramble and the legacy constant alias");
}

void TestImperioControls() {
    using namespace wizard;
    struct Input {
        float Throttle = 0, Steer = 0, Pitch = 0, Yaw = 0, Roll = 0, DodgeForward = 0, DodgeStrafe = 0;
        unsigned Handbrake = 0, Jump = 0, ActivateBoost = 0, HoldingBoost = 0, Jumped = 0;
    };
    for (unsigned buttons = 0; buttons < 32; ++buttons) {
        Input source{1, -.75f, .25f, -.5f, 1, -.2f, .3f,
            buttons & 1, buttons & 2, buttons & 4, buttons & 8, buttons & 16};
        const auto out = MirrorControls(source);
        Check(out.Throttle == 1 && out.Steer == -.75f && out.Pitch == .25f &&
            out.Yaw == -.5f && out.Roll == 1 && out.DodgeForward == -.2f && out.DodgeStrafe == .3f,
            "Imperio preserves normalized driving, aerial and dodge commands");
        Check(out.Handbrake == ((buttons & 1) ? 1u : 0u) && out.Jump == ((buttons & 2) ? 1u : 0u) &&
            out.ActivateBoost == ((buttons & 4) ? 1u : 0u) && out.HoldingBoost == ((buttons & 8) ? 1u : 0u) &&
            out.Jumped == ((buttons & 16) ? 1u : 0u),
            "Imperio mirrors all five native input flags as normalized bits");
        Check(source.Steer == -.75f && source.ActivateBoost == (buttons & 4),
            "mirroring does not change the caster's source controls");
        const auto repeat = MirrorControls(out);
        Check(repeat.Throttle == out.Throttle && repeat.Steer == out.Steer && repeat.Yaw == out.Yaw &&
            repeat.Jump == out.Jump && repeat.HoldingBoost == out.HoldingBoost,
            "copying controls on prediction and authority is idempotent rather than double inversion");
    }
    Input extreme{2, -3, 4, -5, 6, -7, 8, 3, 4, 5, 6, 7};
    const auto normalized = MirrorControls(extreme);
    Check(normalized.Throttle == 1 && normalized.Steer == -1 && normalized.Pitch == 1 &&
        normalized.Yaw == -1 && normalized.Roll == 1 && normalized.DodgeForward == -1 && normalized.DodgeStrafe == 1,
        "Imperio clamps excessive values on every axis");
    const float nan = std::numeric_limits<float>::quiet_NaN(), infinity = std::numeric_limits<float>::infinity();
    Input invalid{nan, infinity, -infinity, nan, infinity, -infinity, nan};
    const auto neutral = MirrorControls(invalid);
    Check(neutral.Throttle == 0 && neutral.Steer == 0 && neutral.Pitch == 0 && neutral.Yaw == 0 &&
        neutral.Roll == 0 && neutral.DodgeForward == 0 && neutral.DodgeStrafe == 0,
        "invalid caster axes become neutral rather than poisoning victim controls");
    const auto idle = MirrorControls(Input{});
    Check(idle.Throttle == 0 && idle.Steer == 0 && !idle.Handbrake && !idle.Jump && !idle.ActivateBoost &&
        !idle.HoldingBoost && !idle.Jumped, "idle caster input supplies no stale victim buttons");
}

void TestActiveClock() {
    using namespace wizard;
    Check(AdvanceActiveClock(0, 3, true, true) == 3, "three-second input stall still expires effect deadline");
    Check(AdvanceActiveClock(10, 4.5, true, true) == 14.5, "frame hitch advances full elapsed interval without a short cap");
    double repeated = 0;
    for (int i = 0; i < 60; ++i) repeated = AdvanceActiveClock(repeated, 0.05, true, true);
    Check(Near(repeated, 3), "repeated fifty-millisecond frames advance the full three-second duration");
    Check(AdvanceActiveClock(10, 30, false, false) == 10, "paused time does not advance effects");
    Check(AdvanceActiveClock(10, 30, false, true) == 10, "resume transition does not consume paused interval");
    Check(AdvanceActiveClock(10, 30, true, false) == 10, "pause transition does not advance effects");
    Check(AdvanceActiveClock(10, -1, true, true) == 10, "negative elapsed time cannot rewind effects");
    Check(AdvanceActiveClock(10, std::numeric_limits<double>::quiet_NaN(), true, true) == 10,
        "nonfinite elapsed time cannot poison effect clock");
    Check(AdvanceActiveClock(10, std::numeric_limits<double>::infinity(), true, true) == 10,
        "infinite elapsed time cannot poison effect clock");
    Check(AdvanceActiveClock(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), true, true) ==
        std::numeric_limits<double>::max(), "overflowing clock advance is rejected");
}

void TestCooldowns() {
    using namespace wizard;
    SpellDeck deck(1234);
    deck.EnsurePlayer(5, 100);
    Check(deck.ReadyFor(5) == Spell::None && Near(deck.Remaining(5, 100), 10), "joining begins full cooldown");
    deck.EnsurePlayer(5, 105);
    Check(Near(deck.Remaining(5, 105), 5), "repeated ensure does not restart cooldown");
    deck.Update(109.99);
    Check(deck.ReadyFor(5) == Spell::None, "no grant before deadline");
    Check(!deck.Consume(5, 109.99), "unready cast rejected");
    deck.Update(110);
    const Spell held = deck.ReadyFor(5);
    Check(held != Spell::None && IsValidSpell(held), "grant at deadline");
    Check(deck.Remaining(5, 110) == 0, "held spell has no cooldown");
    deck.Update(1000);
    Check(deck.ReadyFor(5) == held, "held spell persists until successful consumption");
    Check(deck.Consume(5, 1000), "successful cast consumed");
    Check(!deck.Consume(5, 1000) && Near(deck.Remaining(5, 1000), 10), "duplicate cast cannot spend twice");
    Check(!deck.Consume(999, 1000), "unknown player cannot cast");
    deck.Update(1010);
    Check(deck.ReadyFor(5) != Spell::None, "next spell granted after cast cooldown");
    Check(deck.Disarm(5, 1010) && deck.ReadyFor(5) == Spell::None && Near(deck.Remaining(5, 1010), 10),
        "disarming held spell starts new cooldown");
    Check(!deck.Disarm(5, 1011) && Near(deck.Remaining(5, 1011), 9),
        "disarming unready player does not restart cooldown");
    Check(!deck.Disarm(999, 1011), "unknown player cannot be disarmed");
    deck.EnsurePlayer(8, 1010);
    deck.RemoveMissing({8});
    Check(deck.Size() == 1 && deck.ReadyFor(5) == Spell::None, "departed player state removed");
    deck.Clear();
    Check(deck.Size() == 0, "new match clears held spells");
    deck.SetCooldown(-1);
    Check(deck.Cooldown() == 1, "cooldown minimum clamped");
    deck.SetCooldown(1000);
    Check(deck.Cooldown() == 60, "cooldown maximum clamped");
    deck.SetCooldown(std::numeric_limits<double>::quiet_NaN());
    Check(deck.Cooldown() == 10, "nonfinite cooldown gets default");
    deck.EnsurePlayer(-1, 0);
    deck.EnsurePlayer(1, std::numeric_limits<double>::infinity());
    Check(deck.Size() == 0, "invalid player or clock cannot create state");
    deck.EnsurePlayer(1, 0);
    deck.Update(std::numeric_limits<double>::infinity());
    Check(deck.ReadyFor(1) == Spell::None, "nonfinite clock cannot deal");
    deck.Update(10);
    Check(!deck.Consume(1, std::numeric_limits<double>::infinity()), "nonfinite clock cannot consume");

    SpellDeck first(42, 1), second(42, 1);
    first.EnsurePlayer(2, 0); first.EnsurePlayer(1, 0);
    second.EnsurePlayer(1, 0); second.EnsurePlayer(2, 0);
    std::set<Spell> seen;
    for (int time = 1; time <= 200; ++time) {
        first.Update(time); second.Update(time);
        Check(first.ReadyFor(1) == second.ReadyFor(1) && first.ReadyFor(2) == second.ReadyFor(2),
            "seeded dealing deterministic regardless of insertion order");
        seen.insert(first.ReadyFor(1));
        const auto dealt = first.ReadyFor(1);
        Check(dealt != static_cast<Spell>(5) && dealt != static_cast<Spell>(7) &&
            dealt != static_cast<Spell>(10) && dealt != static_cast<Spell>(11) && dealt != static_cast<Spell>(12),
            "removed spells never dealt");
        first.Consume(1, time); first.Consume(2, time);
        second.Consume(1, time); second.Consume(2, time);
    }
    Check(seen.size() == SpellCount && SpellCount == 11 && seen.count(Spell::Obliviate) &&
        seen.count(Spell::EatSlugs) && seen.count(Spell::ExpectoPatronum) && seen.count(Spell::Imperio),
        "all eleven spells including Obliviate and Imperio are reachable");
    for (const auto spell : ActiveSpells) {
        const auto& info = GetSpellInfo(spell);
        Check(*info.name != '\0' && *info.description != '\0' && *info.identifier != '\0', "all metadata entries populated");
    }
    Check(std::string(GetSpellInfo(Spell::EatSlugs).identifier) == "eat_slugs" &&
        std::string(GetSpellInfo(Spell::EatSlugs).name) == "Eat Slugs",
        "Eat Slugs has its own practice identifier and readable name");
    for (const int id : {5, 7, 10, 11, 12}) {
        Check(!IsValidSpell(static_cast<Spell>(id)), "removed spell IDs remain reserved and invalid");
        Check(std::string(GetSpellInfo(static_cast<Spell>(id)).identifier) == "none",
            "removed spell IDs do not expose usable metadata");
    }
    Check(IsValidSpell(Spell::None) && !IsCastableSpell(Spell::None), "charging state is valid but not castable");
    Check(BombardaMaximaSpeed == 12000 && BombardaRicochetDurationSeconds == 5.0 &&
        DepulsoSpeed == BombardaMaximaSpeed &&
        std::string(GetSpellInfo(Spell::BombardaMaxima).identifier) == "bombarda_maxima" &&
        std::string(GetSpellInfo(Spell::BombardaMaxima).name) == "Bombarda Maxima" &&
        std::string(GetSpellInfo(Spell::Depulso).name) == "Bombarda Maxima",
        "Bombarda Maxima now has a five-second 12000-unit ricochet while preserving its ID and legacy alias");
    Check(std::string(GetSpellInfo(Spell::Obliviate).identifier) == "obliviate" &&
        std::string(GetSpellInfo(Spell::Confundo).name) == "Obliviate" &&
        std::string(GetSpellInfo(Spell::Imperio).identifier) == "imperio",
        "the new spells expose canonical names while old Confundo source references resolve to Obliviate");
    Check(deck.Grant(5, Spell::Protego, 1500) && deck.ReadyFor(5) == Spell::Protego && deck.Remaining(5, 1500) == 0,
        "practice grant immediately holds selected spell");
    Check(!deck.Grant(5, Spell::None, 1500), "practice grant rejects charging state");
    for (const int id : {5, 7, 10, 11, 12})
        Check(!deck.Grant(5, static_cast<Spell>(id), 1500), "practice grant rejects every removed spell ID");
    Check(!deck.Grant(-1, Spell::Accio, 1500) && !deck.Grant(MaxPlayerId + 1, Spell::Accio, 1500) &&
        !deck.Grant(0, Spell::Accio, std::numeric_limits<double>::quiet_NaN()), "practice grant rejects invalid player and clock");
    SpellDeck changing(10, 10);
    changing.EnsurePlayer(0, 0);
    changing.SetCooldown(1);
    Check(Near(changing.CooldownFor(0), 10) && Near(changing.Remaining(0, 2), 8),
        "changing settings preserves existing cooldown's duration and deadline");
    changing.Update(10); changing.Consume(0, 10);
    Check(Near(changing.CooldownFor(0), 1) && Near(changing.Remaining(0, 10), 1),
        "new cooldown records new per-player duration for radial HUD");
}

void TestBarrier() {
    using namespace wizard;
    const auto planned = PlanBarrier({0, 0, 93}, {1000, 0, 0}, {0, 1, 0});
    Check(planned && IsValidBarrier(*planned) && Near(planned->center.x, 450), "barrier forms 450 units along ball travel");
    Check(Near(planned->radius, 350) && Near(planned->center.z - planned->radius, 20),
        "circular barrier has a 350-unit radius and its bottom remains above field");
    Check(Near(planned->normal.x, 1) && Near(planned->normal.y, 0), "ball travel takes priority over caster direction");
    const auto stationary = PlanBarrier({0, 0, 100}, {}, {0, 5, 0});
    Check(stationary && Near(stationary->normal.y, 1), "stationary ball uses caster forward");
    const auto noDirection = PlanBarrier({}, {}, {});
    Check(noDirection && Near(noDirection->normal.x, 1), "zero directions use stable fallback");
    const auto vertical = PlanBarrier({0, 0, 1000}, {0, 0, 1000}, {1, 0, 0});
    Check(vertical && IsValidBarrier(*vertical) && Near(vertical->center.z, 1450), "vertical ball travel has stable perpendicular wall basis");
    const auto steep = PlanBarrier({0, 0, 1000}, {5, -2, 1000}, {1, 0, 0});
    Check(steep && IsValidBarrier(*steep), "nearly vertical basis remains orthonormal");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Check(!PlanBarrier({nan, 0, 0}, {}, {}) && !PlanBarrier({}, {nan, 0, 0}, {}) &&
        !PlanBarrier({}, {}, {}, 450, 0), "invalid barrier planning rejected");
    Barrier wall;
    const auto hit = SweepBarrier({-100, 0, 0}, {100, 0, 0}, {1000, 50, 20}, wall, 10);
    Check(hit && Near(hit->fraction, 0.45) && Near(hit->position.x, -12), "fast sphere sweeps front face and is corrected outside");
    Check(hit && Near(hit->velocity.x, -950) && Near(hit->velocity.y, 50) && Near(hit->velocity.z, 20),
        "bounce reflects normal velocity while preserving tangential motion");
    const auto back = SweepBarrier({100, 0, 0}, {-100, 0, 0}, {-1000, 0, 0}, wall, 10);
    Check(back && Near(back->position.x, 12) && Near(back->velocity.x, 950), "barrier also blocks return-side impacts");
    Check(!SweepBarrier({-12, 0, 0}, {-50, 0, 0}, {-950, 0, 0}, wall, 10), "separating corrected ball does not bounce repeatedly");
    Check(!SweepBarrier({-100, 500, 0}, {100, 500, 0}, {1000, 0, 0}, wall, 10), "disk does not block beyond its horizontal radius");
    Check(!SweepBarrier({-100, 0, 500}, {100, 0, 500}, {1000, 0, 0}, wall, 10), "disk does not block beyond its vertical radius");
    const auto edge = SweepBarrier({-100, 355, 0}, {100, 355, 0}, {1000, 0, 0}, wall, 10);
    Check(edge && Near(edge->contact.y, 350) && edge->normal.y > 0 && edge->velocity.y > 0,
        "sphere radius catches the circular rim with a glancing contact normal");
    Check(edge && Near(edge->fraction, (100 - std::sqrt(75.0)) / 200) && Near(edge->normal.y, 0.5),
        "rim time and normal agree with the analytic constant-radial-distance crossing");
    const float diagonal = std::sqrt(0.5f);
    const auto rim = SweepBarrier({-100, 356 * diagonal, 356 * diagonal}, {100, 356 * diagonal, 356 * diagonal},
        {1000, 0, 0}, wall, 10);
    Check(rim && std::abs(std::hypot(rim->contact.y, rim->contact.z) - 350) < 0.001 &&
        rim->normal.y > 0 && rim->normal.z > 0 && rim->velocity.y > 0 && rim->velocity.z > 0,
        "diagonal rim contact lies on the circle and reflects out in both radial axes");
    const auto fastRim = SweepBarrier({-50000, 355, 0}, {50000, 355, 0}, {6000, 0, 0}, wall, 10);
    Check(fastRim && Near(fastRim->normal.y, 0.5), "long fast sweep retains precision for the narrow rounded rim");
    Check(!SweepBarrier({-100, 345, 345}, {100, 345, 345}, {1000, 0, 0}, wall, 10),
        "empty square corners outside the circle are passable");
    Check(!SweepBarrier({-100, 356, 256}, {100, 356, 256}, {1000, 0, 0}, wall, 10),
        "the old rectangular corner has no invisible disk collision");
    Check(!SweepBarrier({-100, 360, 0}, {100, 360, 0}, {1000, 0, 0}, wall, 10), "exact tangent does not spuriously reverse velocity");
    Check(SweepBarrier({-50000, 359.99f, 0}, {50000, 359.99f, 0}, {6000, 0, 0}, wall, 10).has_value(),
        "a shallow rim intersection is not missed by a long fast sweep");
    Check(!SweepBarrier({-50000, 360.01f, 0}, {50000, 360.01f, 0}, {6000, 0, 0}, wall, 10),
        "a near-tangent rim miss remains passable");
    const auto lateral = SweepBarrier({5, 380, 0}, {5, 330, 0}, {0, -1000, 0}, wall, 10);
    Check(lateral && Near(lateral->contact.y, 350) && lateral->normal.x > 0 && lateral->normal.y > 0 &&
        lateral->velocity.x > 0 && lateral->velocity.y > 0,
        "sphere can strike the rounded rim while moving parallel to the disk plane");
    const auto endpoint = SweepBarrier({-100, 0, 0}, {-10, 0, 0}, {1000, 0, 0}, wall, 10);
    Check(endpoint && Near(endpoint->fraction, 1), "an entering face contact exactly at the segment endpoint is included");
    Check(!SweepBarrier({-5, 0, 0}, {-5, 0, 0}, {}, wall, 10), "stationary overlapping ball is not repeatedly reflected");
    const auto overlap = SweepBarrier({-5, 0, 0}, {0, 0, 0}, {1000, 0, 0}, wall, 10);
    Check(overlap && Near(overlap->fraction, 0) && Near(overlap->position.x, -12), "approaching overlapping sphere is depenetrated safely");
    Check(!SweepBarrier({-5, 0, 0}, {-15, 0, 0}, {-1000, 0, 0}, wall, 10), "overlapping sphere moving away is untouched");
    Check(!SweepBarrier({-100, 0, 0}, {100, 0, 0}, {1000, 0, 0}, wall, 0), "zero sphere radius rejected");
    Check(!SweepBarrier({nan, 0, 0}, {}, {}, wall) && !SweepBarrier({}, {}, {nan, 0, 0}, wall), "nonfinite sweep data rejected");
    Check(Near(ClosestBarrierPoint(wall, {50, 500, 0}).y, 350), "nearest point clamps radially to the circular rim");
    Check(Near(ClosestBarrierPoint(wall, {50, 200, 100}).y, 200) &&
        Near(ClosestBarrierPoint(wall, {50, 200, 100}).z, 100), "nearest interior point stays inside the disk face");
    const Barrier rotated{{20, -30, 400}, {0.6f, 0.8f, 0}, {-0.8f, 0.6f, 0}, {0, 0, 1}, 350};
    const auto rotatedRim = SweepBarrier(rotated.center - rotated.normal * 100 + rotated.up * 355,
        rotated.center + rotated.normal * 100 + rotated.up * 355, rotated.normal * 1000, rotated, 10);
    Check(rotatedRim && (rotatedRim->contact - (rotated.center + rotated.up * 350)).Length() < 0.001f &&
        rotatedRim->normal.z > 0 && rotatedRim->velocity.z > 0,
        "translated and rotated disks use the same circular rim collision");
    // Constant-radial-distance sweeps have an independent closed-form answer:
    // face contact at |x|=sphereRadius, or a circular rim cross-section.
    // Exercise both sides, multiple ball sizes and azimuths at high speed.
    for (const float sphereRadius : {1.0f, 10.0f, 93.0f}) {
        for (const float radialFactor : {-1.0f, 0.0f, 0.5f, 0.999f, 1.01f}) {
            for (int angle = 0; angle < 8; ++angle) {
                const float theta = angle * 0.7853981633974483f;
                const float radial = wall.radius + radialFactor * sphereRadius;
                const float y = std::cos(theta) * radial, z = std::sin(theta) * radial;
                const double outside = std::max(0.0, std::hypot(static_cast<double>(y), static_cast<double>(z)) - wall.radius);
                const bool expectedHit = outside < sphereRadius;
                for (const float side : {-1.0f, 1.0f}) {
                    const Vec3 incoming{-side * 6000, 0, 0};
                    const auto analytic = SweepBarrier({side * 50000, y, z}, {-side * 50000, y, z}, incoming, wall, sphereRadius);
                    Check(static_cast<bool>(analytic) == expectedHit,
                        "disk sweep matches analytic hit/miss across ball sizes, rim depths, azimuths and directions");
                    if (!analytic) continue;
                    const double normalDistance = std::sqrt(static_cast<double>(sphereRadius) * sphereRadius - outside * outside);
                    const double expectedFraction = (50000 - normalDistance) / 100000;
                    Check(std::abs(analytic->fraction - expectedFraction) < 1e-7 &&
                        std::abs(analytic->normal.x - side * normalDistance / sphereRadius) < 0.001,
                        "fast disk contact time and normal match the analytic circular cross-section");
                    Check(std::abs((analytic->position - ClosestBarrierPoint(wall, analytic->position)).Length() -
                        (sphereRadius + 2)) < 0.005f &&
                        std::abs(analytic->velocity.Dot(analytic->normal) + 0.95f * incoming.Dot(analytic->normal)) < 0.01f,
                        "disk correction separates the sphere and reflection applies restitution along its contact normal");
                }
            }
        }
    }
    Barrier invalidRadius = wall; invalidRadius.radius = 0;
    Check(!SweepBarrier({-100, 0, 0}, {100, 0, 0}, {1000, 0, 0}, invalidRadius), "nonpositive disk radius rejected");
    wall.normal = {};
    Check(!SweepBarrier({-100, 0, 0}, {100, 0, 0}, {1000, 0, 0}, wall), "invalid wall basis rejected");
}

void TestProtocol() {
    using namespace wizard;
    Check(ParseMessage("W2 C")->kind == MessageKind::Cast, "cast parsed without identity");
    Check(ParseMessage("W2 H")->kind == MessageKind::Hello, "hello parsed");
    std::vector<Spell> states{Spell::None};
    states.insert(states.end(), ActiveSpells.begin(), ActiveSpells.end());
    for (const auto spell : states) {
        for (const float range : {MinSpellRange, std::nextafter(MinSpellRange, MaxSpellRange),
            2345.6787f, DefaultSpellRange, std::nextafter(MaxSpellRange, MinSpellRange), MaxSpellRange}) {
            Message message;
            message.kind = MessageKind::State;
            message.playerId = 65535;
            message.spell = spell;
            message.remainingDeciseconds = spell == Spell::None ? 600 : 0;
            message.cooldownDeciseconds = 600;
            message.enabled = true;
            message.spellRange = range;
            const std::string wire = SerializeMessage(message);
            const auto parsed = ParseMessage(wire);
            Check(wire.size() <= MaxMessageLength && parsed && parsed->playerId == message.playerId &&
                parsed->spell == message.spell && parsed->remainingDeciseconds == message.remainingDeciseconds &&
                parsed->cooldownDeciseconds == 600 && parsed->enabled && parsed->spellRange == range,
                "state preserves every spell and exact bounded float range");
        }
    }
    const auto legacyState = ParseMessage("W2 S 4 1 0 100 1");
    Check(legacyState && legacyState->spellRange == LegacySpellRange && legacyState->spellRange == 4500 &&
        legacyState->spell == Spell::Accio, "legacy seven-field state preserves its original range");
    const auto newDefaultState = ParseMessage("W2 S 4 1 0 100 1 5500");
    Check(newDefaultState && newDefaultState->spellRange == DefaultSpellRange,
        "new host state synchronizes the increased default range");
    const auto scientificRange = ParseMessage("W2 S 4 1 0 100 1 5e2");
    Check(scientificRange && scientificRange->spellRange == MinSpellRange, "valid scientific range parsed exactly");
    for (int code = 0; code <= static_cast<int>(Feedback::Charging); ++code) {
        Message message;
        message.kind = MessageKind::Feedback;
        message.playerId = 0;
        message.feedback = static_cast<Feedback>(code);
        const auto parsed = ParseMessage(SerializeMessage(message));
        Check(parsed && parsed->kind == MessageKind::Feedback && parsed->playerId == 0 &&
            parsed->feedback == message.feedback, "feedback round trip");
    }
    for (const auto spell : ActiveSpells) {
        for (const int target : {-1, 0, MaxPlayerId}) {
            Message message;
            message.kind = MessageKind::Effect;
            message.spell = spell;
            message.casterId = MaxPlayerId;
            message.targetId = target;
            message.recipientId = target;
            message.source = {-99999, 99999, -99999};
            message.destination = {99999, -99999, 99999};
            message.normal = {0.6f, 0, -0.8f};
            message.durationMillis = MaxEffectDurationMillis;
            const auto wire = SerializeMessage(message);
            const auto parsed = ParseMessage(wire);
            Check(parsed && parsed->kind == MessageKind::Effect && parsed->spell == message.spell &&
                parsed->casterId == message.casterId && parsed->targetId == target && parsed->recipientId == target &&
                parsed->source.x == -99999 && parsed->destination.y == -99999 && Near(parsed->normal.z, -0.8) &&
                parsed->durationMillis == MaxEffectDurationMillis && wire.size() <= MaxMessageLength,
                "quantized scene event round trip within transport byte limit");
            message.kind = MessageKind::EffectSnapshot;
            const auto snapshot = ParseMessage(SerializeMessage(message));
            Check(snapshot && snapshot->kind == MessageKind::EffectSnapshot && snapshot->recipientId == target,
                "directed persistent snapshot round trip");
            message.kind = MessageKind::Cancel;
            const auto cancel = ParseMessage(SerializeMessage(message));
            Check(cancel && cancel->kind == MessageKind::Cancel && cancel->spell == spell &&
                cancel->casterId == MaxPlayerId && cancel->targetId == target && cancel->recipientId == target,
                "directed cancellation identifies exact caster and target effect");
        }
    }
    for (const int recipient : {-1, 0, MaxPlayerId}) {
        Message clear; clear.kind = MessageKind::Clear; clear.recipientId = recipient;
        const auto parsed = ParseMessage(SerializeMessage(clear));
        Check(parsed && parsed->kind == MessageKind::Clear && parsed->recipientId == recipient, "directed scene clear round trip");
    }
    for (const auto kind : {MessageKind::Charge, MessageKind::ChargeSnapshot}) {
        for (const auto spell : {Spell::AvadaKedavra, Spell::Leviosa, Spell::EatSlugs, Spell::Stupefy, Spell::PetrificusTotalus, Spell::Protego, Spell::BombardaMaxima, Spell::ExpectoPatronum, Spell::Obliviate}) {
            const int maximumMillis = static_cast<int>(SpellChargeSeconds(spell) * 1000);
            for (const int duration : {1, maximumMillis / 2, maximumMillis - 1, maximumMillis}) {
                for (const int recipient : {-1, 0, MaxPlayerId}) {
                    Message charge;
                    charge.kind = kind; charge.spell = spell;
                    charge.casterId = MaxPlayerId; charge.targetId = IsBallTargetSpell(spell) ? -1 : 0; charge.recipientId = recipient;
                    charge.source = {-99999, 99999, -99999}; charge.destination = {99999, -99999, 99999};
                    charge.normal = {0, 1, 0}; charge.durationMillis = duration;
                    const std::string wire = SerializeMessage(charge);
                    const auto parsed = ParseMessage(wire);
                    Check(parsed && parsed->kind == kind && parsed->spell == spell &&
                        parsed->casterId == MaxPlayerId && parsed->targetId == charge.targetId && parsed->recipientId == recipient &&
                        parsed->source.x == -99999 && parsed->destination.y == -99999 &&
                        parsed->durationMillis == duration && wire.size() <= MaxMessageLength,
                        "each charged spell and remaining snapshot round trips within its limit and byte budget");
                }
            }
            Message invalidCharge; invalidCharge.kind = kind; invalidCharge.spell = spell;
            invalidCharge.casterId = 0; invalidCharge.targetId = IsBallTargetSpell(spell) ? -1 : 1;
            for (const int invalidDuration : {-1, 0, maximumMillis + 1, 10000}) {
                invalidCharge.durationMillis = invalidDuration;
                Check(SerializeMessage(invalidCharge).empty(), "charge cannot serialize an out-of-bounds countdown");
                const std::string prefix = kind == MessageKind::Charge ? "W2 G " : "W2 J ";
                Check(!ParseMessage(prefix + std::to_string(static_cast<int>(spell)) +
                    " 0 " + std::to_string(invalidCharge.targetId) + " -1 0 0 0 0 0 0 1000 0 0 " + std::to_string(invalidDuration)),
                    "charge parser enforces that spell's countdown limit");
            }
            invalidCharge.durationMillis = maximumMillis;
            invalidCharge.targetId = IsBallTargetSpell(spell) ? 0 : -1;
            Check(SerializeMessage(invalidCharge).empty(), "charge rejects the other actor type's target representation");
        }
        Message invalidCharge; invalidCharge.kind = kind;
        invalidCharge.casterId = 0; invalidCharge.targetId = 1;
        for (const auto spell : ActiveSpells) {
            invalidCharge.spell = spell;
            invalidCharge.targetId = IsBallTargetSpell(spell) ? -1 : 1;
            invalidCharge.durationMillis = 100;
            Check(SerializeMessage(invalidCharge).empty() == !IsChargedSpell(spell),
                "only the eight charged spells can enter the charge protocol with their valid target type");
        }
        invalidCharge.spell = Spell::EatSlugs;
        invalidCharge.targetId = 1;
        invalidCharge.durationMillis = 2000;
        Check(SerializeMessage(invalidCharge).empty(), "Eat Slugs cannot inherit a two-second charge");
    }
    for (const char kind : {'G', 'J'}) {
        const std::string prefix = std::string("W2 ") + kind + " ";
        const auto wardBoundary = ParseMessage(prefix + "6 0 -1 -1 0 0 0 0 0 0 1000 0 0 1000");
        Check(wardBoundary && wardBoundary->spell == Spell::Protego && wardBoundary->targetId == -1 &&
            wardBoundary->durationMillis == 1000, "Protego accepts its one-second ball charge boundary");
        Check(!ParseMessage(prefix + "6 0 -1 -1 0 0 0 0 0 0 1000 0 0 1001") &&
            !ParseMessage(prefix + "6 0 -1 -1 0 0 0 0 0 0 1000 0 0 2000"),
            "Protego cannot exceed its one-second charge limit");
        Check(!ParseMessage(prefix + "6 0 0 -1 0 0 0 0 0 0 1000 0 0 1000") &&
            !ParseMessage(prefix + "6 0 65535 -1 0 0 0 0 0 0 1000 0 0 1000") &&
            !ParseMessage(prefix + "6 0 -2 -1 0 0 0 0 0 0 1000 0 0 1000"),
            "Protego charge cannot masquerade as a player target or unknown sentinel");
        const auto bodyBoundary = ParseMessage(prefix + "9 0 1 -1 0 0 0 0 0 0 1000 0 0 1500");
        Check(bodyBoundary && bodyBoundary->durationMillis == 1500,
            "Petrificus accepts its exact one-and-a-half-second boundary");
        Check(!ParseMessage(prefix + "9 0 1 -1 0 0 0 0 0 0 1000 0 0 1501") &&
            !ParseMessage(prefix + "9 0 1 -1 0 0 0 0 0 0 1000 0 0 2000"),
            "Petrificus rejects charge times beyond its own deadline");
        const auto stunBoundary = ParseMessage(prefix + "8 0 1 -1 0 0 0 0 0 0 1000 0 0 650");
        Check(stunBoundary && stunBoundary->durationMillis == 650, "Stupefy accepts its 650-millisecond boundary");
        Check(!ParseMessage(prefix + "8 0 1 -1 0 0 0 0 0 0 1000 0 0 651") &&
            !ParseMessage(prefix + "8 0 1 -1 0 0 0 0 0 0 1000 0 0 750"),
            "Stupefy rejects durations above its chosen 650-millisecond limit");
        const auto slugBoundary = ParseMessage(prefix + "14 0 1 -1 0 0 0 0 0 0 1000 0 0 1500");
        Check(slugBoundary && slugBoundary->durationMillis == 1500,
            "Eat Slugs accepts its exact one-and-a-half-second boundary");
        Check(!ParseMessage(prefix + "14 0 1 -1 0 0 0 0 0 0 1000 0 0 1501"),
            "Eat Slugs rejects one millisecond beyond its boundary");
        Check(!ParseMessage(prefix + "14 0 1 -1 0 0 0 0 0 0 1000 0 0 2000"),
            "Eat Slugs parser rejects the other charged spells' two-second duration");
        const auto bombardaBoundary = ParseMessage(prefix + "4 0 -1 -1 0 0 0 0 0 0 1000 0 0 2000");
        Check(bombardaBoundary && bombardaBoundary->spell == Spell::BombardaMaxima &&
            bombardaBoundary->targetId == -1 && bombardaBoundary->durationMillis == 2000,
            "Bombarda accepts its exact two-second ball-target charge boundary");
        Check(!ParseMessage(prefix + "4 0 -1 -1 0 0 0 0 0 0 1000 0 0 2001") &&
            !ParseMessage(prefix + "4 0 0 -1 0 0 0 0 0 0 1000 0 0 2000") &&
            !ParseMessage(prefix + "4 0 -2 -1 0 0 0 0 0 0 1000 0 0 2000"),
            "Bombarda rejects excess charge duration and any player or unknown target sentinel");
        const auto patronumBoundary = ParseMessage(prefix + "15 0 -1 -1 0 0 0 0 0 0 1000 0 0 3000");
        Check(patronumBoundary && patronumBoundary->spell == Spell::ExpectoPatronum &&
            patronumBoundary->targetId == -1 && patronumBoundary->durationMillis == 3000,
            "Patronum accepts its exact three-second ball-target charge boundary");
        Check(!ParseMessage(prefix + "15 0 -1 -1 0 0 0 0 0 0 1000 0 0 3001") &&
            !ParseMessage(prefix + "15 0 0 -1 0 0 0 0 0 0 1000 0 0 3000") &&
            !ParseMessage(prefix + "15 0 -2 -1 0 0 0 0 0 0 1000 0 0 3000"),
            "Patronum rejects a fourth-second charge or an opposing-player target representation");
        for (const std::string& suffix : {
            "2 0 1 -1 0 0 0 0 0 0 1000 0 0 0", "2 0 1 -1 0 0 0 0 0 0 1000 0 0 2001",
            "2 0 -1 -1 0 0 0 0 0 0 1000 0 0 2000", "2 -1 1 -1 0 0 0 0 0 0 1000 0 0 2000",
            "2 0 1 -2 0 0 0 0 0 0 1000 0 0 2000", "2 0 1 -1 0 0 0 0 0 0 1000 0 0 2000 extra",
            "2 0 1 -1 100000 0 0 0 0 0 1000 0 0 2000", "2 0 1 -1 0 0 0 0 0 0 1001 0 0 2000"
        }) Check(!ParseMessage(prefix + suffix), "malformed Avada charge or snapshot is rejected");
        for (int id = 0; id <= MaxSpellId; ++id) {
            if (IsChargedSpell(static_cast<Spell>(id))) continue;
            Check(!ParseMessage(prefix + std::to_string(id) + " 0 1 -1 0 0 0 0 0 0 1000 0 0 2000"),
                "every noncharged, removed or empty spell is rejected by charge parser");
        }
    }
    Message impact; impact.kind = MessageKind::Impact; impact.casterId = MaxPlayerId; impact.source = {-123.4f, 99.6f, 88};
    const auto parsedImpact = ParseMessage(SerializeMessage(impact));
    Check(parsedImpact && parsedImpact->kind == MessageKind::Impact && parsedImpact->casterId == MaxPlayerId &&
        parsedImpact->source.x == -123 && parsedImpact->source.y == 100, "barrier impact quantizes world contact location");
    for (const int caster : {0, MaxPlayerId}) {
        for (const Vec3 direction : {Vec3{1, 0, 0}, Vec3{0, -1, 0}, Vec3{0.6f, 0.8f, 0}}) {
            Message strike; strike.kind = MessageKind::PatronumImpact; strike.casterId = caster;
            strike.source = {-99999, 99.6f, 99999}; strike.normal = direction;
            const auto wire = SerializeMessage(strike);
            const auto decoded = ParseMessage(wire);
            Check(decoded && decoded->kind == MessageKind::PatronumImpact && decoded->spell == Spell::ExpectoPatronum &&
                decoded->casterId == caster && decoded->source.x == -99999 && decoded->source.y == 100 &&
                decoded->source.z == 99999 && std::abs(decoded->normal.x - direction.x) < 0.001f &&
                std::abs(decoded->normal.y - direction.y) < 0.001f && wire.size() <= MaxMessageLength,
                "Patronum strike position and direction round trip in a compact V event");
        }
    }
    for (const std::string& wire : {
        "W2 V", "W2 V 0 0 0 0 1000 0", "W2 V 0 0 0 0 1000 0 0 extra",
        "W2 V -1 0 0 0 1000 0 0", "W2 V 65536 0 0 0 1000 0 0",
        "W2 V 0 100000 0 0 1000 0 0", "W2 V 0 0 0 0 1001 0 0",
        "W2 V 0 0 0 0 1000 1000 0", "W2 V 0 0 0 0 0 0 0", "W2 V 0 0 0 0 nan 0 0"
    }) Check(!ParseMessage(wire), "malformed Patronum strike events are rejected");
    Message badStrike; badStrike.kind = MessageKind::PatronumImpact; badStrike.casterId = 0;
    for (const Vec3 direction : {Vec3{}, Vec3{0.1f, 0, 0}, Vec3{1, 1, 0},
        Vec3{std::numeric_limits<float>::quiet_NaN(), 0, 0}}) {
        badStrike.normal = direction;
        Check(SerializeMessage(badStrike).empty(), "invalid outbound Patronum directions never reach the transport");
    }
    for (const int caster : {0, 123, MaxPlayerId}) {
        Message cancelCharge; cancelCharge.kind = MessageKind::ProtegoChargeCancel; cancelCharge.casterId = caster;
        const auto wire = SerializeMessage(cancelCharge);
        const auto decoded = ParseMessage(wire);
        Check(decoded && decoded->kind == MessageKind::ProtegoChargeCancel && decoded->spell == Spell::Protego &&
            decoded->casterId == caster && decoded->recipientId == -1 && wire == "W2 U " + std::to_string(caster),
            "Protego wind-up cancellation has its own compact broadcast message");
    }
    for (const int caster : {-1, MaxPlayerId + 1}) {
        Message cancelCharge; cancelCharge.kind = MessageKind::ProtegoChargeCancel; cancelCharge.casterId = caster;
        Check(SerializeMessage(cancelCharge).empty(), "Protego charge cancellation rejects an invalid outgoing caster");
    }
    for (const std::string& wire : {"W2 U", "W2 U 0 extra", "W2 U -1", "W2 U 65536", "W2 U +1",
        "W2 U 1.0", "W2 U none", "W2 U 99999999999999999999999"})
        Check(!ParseMessage(wire), "malformed Protego charge cancellations are rejected");
    const std::vector<std::string> malformed{
        "", "W2", "W1 C", "W2 C 7", "W2 H extra", " W2 C", "W2 C ", "W2  C", "W2\tC", "W2 C\n",
        "W2 S", "W2 S 0 0 100 100", "W2 S 0 0 100 100 1 extra", "W2 S -1 0 100 100 1", "W2 S +1 0 100 100 1",
        "W2 S 65536 0 100 100 1", "W2 S 99999999999999999999999 0 100 100 1", "W2 S 1 -1 0 100 1",
        "W2 S 1 7 0 100 1", "W2 S 1 12 0 100 1", "W2 S 1 0 -1 100 1", "W2 S 1 0 601 600 1",
        "W2 S 1 0 1.5 100 1", "W2 S 1 0 100 100 2", "W2 S 1 0 100 100 true", "W2 S 1 1 100 100 1",
        "W2 S 1 0 100 0 1", "W2 S 1 0 100 99 1", "W2 S 1 0 100 601 1",
        "W2 S 1 1 0 100 1 -500", "W2 S 1 1 0 100 1 +500", "W2 S 1 1 0 100 1 nan",
        "W2 S 1 1 0 100 1 inf", "W2 S 1 1 0 100 1 0", "W2 S 1 1 0 100 1 499.99997",
        "W2 S 1 1 0 100 1 10000.001", "W2 S 1 1 0 100 1 1e999", "W2 S 1 1 0 100 1 1e-999",
        "W2 S 1 1 0 100 1 4500garbage", "W2 S 1 1 0 100 1 0x1194", "W2 S 1 1 0 100 1 4500 500",
        "W2 S 1 1 0 100 1 5e", "W2 S 1 1 0 100 1 5,000", "W2 S 1 1 0 100 1 4.5.0",
        "W2 F 0 -1", "W2 F 0 7", "W2 F 0 0 extra", "W2 F -1 0", "W2 F 1x 0",
        "W2 E", "W2 E 1 0 -1", "W2 E 0 0 -1 -1 0 0 0 0 0 0 1000 0 0 600",
        "W2 E 7 0 -1 -1 0 0 0 0 0 0 1000 0 0 600", "W2 E 12 0 -1 -1 0 0 0 0 0 0 1000 0 0 600",
        "W2 E 1 -1 -1 -1 0 0 0 0 0 0 1000 0 0 600", "W2 E 1 0 -2 -1 0 0 0 0 0 0 1000 0 0 600",
        "W2 E 1 0 -01 -1 0 0 0 0 0 0 1000 0 0 600", "W2 E 1 0 -1 -2 0 0 0 0 0 0 1000 0 0 600",
        "W2 E 1 0 -1 -1 100000 0 0 0 0 0 1000 0 0 600", "W2 E 1 0 -1 -1 0 0 0 0 0 0 1001 0 0 600",
        "W2 E 1 0 -1 -1 0 0 0 0 0 0 1000 1000 1000 600", "W2 E 1 0 -1 -1 0 0 0 0 0 0 1000 0 0 0",
        "W2 E 1 0 -1 -1 0 0 0 0 0 0 1000 0 0 10001", "W2 E 1 0 -1 -1 0 0 0 0 0 0 1000 0 0 600 extra",
        "W2 X", "W2 X 7 0 -1 -1", "W2 X 1 -1 -1 -1", "W2 X 1 0 -2 -1", "W2 X 1 0 -1 -2",
        "W2 R", "W2 R -2", "W2 R -01", "W2 R 65536", "W2 R -1 extra",
        "W2 B", "W2 B -1 0 0 0", "W2 B 0 +1 0 0", "W2 B 0 0 100000 0", "W2 B 0 0 0 0 extra",
        std::string("W2 C\0", 5), std::string(MaxMessageLength + 1, 'x')
    };
    for (const auto& wire : malformed) Check(!ParseMessage(wire), "malformed or contradictory wire rejected");
    for (const int id : {5, 7, 10, 11, 12}) {
        const std::string spell = std::to_string(id);
        Check(!ParseMessage("W2 S 1 " + spell + " 0 100 1 4500"),
            "removed spell IDs cannot enter held state through the wire");
        for (const char kind : {'E', 'P'}) {
            Check(!ParseMessage(std::string("W2 ") + kind + " " + spell + " 0 1 -1 0 0 0 0 0 0 1000 0 0 5000"),
                "removed spells cannot enter live effects or join snapshots");
        }
        Check(!ParseMessage("W2 X " + spell + " 0 1 -1"), "removed spell cancellation packets are rejected");
        for (const auto kind : {MessageKind::State, MessageKind::Effect, MessageKind::EffectSnapshot, MessageKind::Cancel}) {
            Message removed;
            removed.kind = kind; removed.playerId = 1; removed.casterId = 0; removed.targetId = 1;
            removed.spell = static_cast<Spell>(id);
            Check(SerializeMessage(removed).empty(), "removed spell IDs cannot be serialized by any spell-bearing message");
        }
    }
    Message invalidRange; invalidRange.kind = MessageKind::State; invalidRange.playerId = 1;
    for (const float range : {0.0f, -500.0f, std::nextafter(MinSpellRange, 0.0f),
        std::nextafter(MaxSpellRange, std::numeric_limits<float>::infinity()),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        invalidRange.spellRange = range;
        Check(SerializeMessage(invalidRange).empty(), "outgoing state rejects invalid host range");
    }
    Message invalid;
    invalid.kind = MessageKind::State;
    Check(SerializeMessage(invalid).empty(), "invalid outgoing player rejected");
    invalid.playerId = 2;
    invalid.spell = static_cast<Spell>(100);
    Check(SerializeMessage(invalid).empty(), "invalid outgoing spell rejected");
    invalid.spell = Spell::Accio;
    invalid.remainingDeciseconds = 1;
    Check(SerializeMessage(invalid).empty(), "contradictory outgoing ready state rejected");
    invalid.kind = MessageKind::Effect;
    invalid.casterId = 0;
    invalid.targetId = -1;
    Check(!SerializeMessage(invalid).empty(), "ball and self effects serialize without playerId");
    invalid.targetId = -2;
    Check(SerializeMessage(invalid).empty(), "invalid outgoing effect target rejected");
    invalid.targetId = -1;
    invalid.spell = Spell::None;
    Check(SerializeMessage(invalid).empty(), "effect must name a concrete spell");
    invalid.spell = Spell::Protego; invalid.source = {100000, 0, 0};
    Check(SerializeMessage(invalid).empty(), "out of range outgoing effect coordinate rejected");
    invalid.source = {}; invalid.normal = {1, 1, 1};
    Check(SerializeMessage(invalid).empty(), "invalid outgoing direction rejected");
    invalid.normal = {1, 0, 0}; invalid.recipientId = -2;
    Check(SerializeMessage(invalid).empty(), "invalid outgoing recipient rejected");
}
} // namespace

int main() {
    TestReplayAudio();
    TestReplayHistory();
    TestReplaySmoothing();
    TestChargedSpells();
    TestPatronum();
    TestAccioGuidance();
    TestSuspension();
    TestTargeting();
    TestSpellTargetsAndRange();
    TestObliviateControls();
    TestImperioControls();
    TestActiveClock();
    TestBarrier();
    TestCooldowns();
    TestProtocol();
    std::cout << "PASS: " << checks << " gameplay and protocol checks\n";
}
