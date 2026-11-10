#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace wizard {

enum class SpellCue { Avada, LeviosaCast, LeviosaFloat, EatSlugs, Stupefy, PetrificusCast, ProtegoCast, BombardaCast, PatronumCast, ObliviateCast, AccioCast, ImperioCast, Count };

// Audio events have their own timeline: a sound must not restart every time a
// cosmetic snapshot is drawn. Each cue models one independent output channel.
class ReplayAudioTrack final {
public:
    struct Event {
        std::uint64_t id;
        SpellCue cue;
        double timeline;
        double duration;
        double end;
        std::uint64_t casterCar;
        std::uint64_t targetCar;
    };
    struct Action {
        SpellCue cue;
        bool play;
        double offsetSeconds;
    };

    static constexpr std::size_t MaxEvents = 256;
    static constexpr double MaxAge = 30.0;
    static constexpr double MaxJump = 0.5;

    // The owner stops actual output before resetting playback or clearing a
    // session. These operations discard bookkeeping, not native audio handles.
    void ResetPlayback() {
        lastPlayback_.reset();
        playing_.fill(0);
    }
    void Clear() {
        events_.clear(); lastRecord_.reset(); nextId_ = 1;
        ResetPlayback();
    }
    std::size_t EventCount() const { return events_.size(); }

    bool Record(double timeline, SpellCue cue, double duration,
        std::uint64_t casterCar = 0, std::uint64_t targetCar = 0) {
        if (!ValidCue(cue) || !std::isfinite(duration) || duration <= 0 || duration > MaxAge ||
            !PrepareRecord(timeline)) return false;
        // One live buffer per cue: a newer voice replaces that channel's old
        // voice, while distinct voices/floating effects can still overlap.
        for (auto& event : events_)
            if (event.cue == cue && event.end > timeline) event.end = timeline;
        events_.push_back({nextId_++, cue, timeline, duration, timeline + duration, casterCar, targetCar});
        Trim(timeline);
        return true;
    }

    // Count means all cue channels. Nonzero keys restrict the cancellation to
    // the exact original actor; old same-ID bot effects cannot cut a new voice.
    void Stop(double timeline, SpellCue cue = SpellCue::Count,
        std::uint64_t casterCar = 0, std::uint64_t targetCar = 0) {
        if ((cue != SpellCue::Count && !ValidCue(cue)) || !PrepareRecord(timeline)) return;
        for (auto& event : events_) {
            if ((cue == SpellCue::Count || event.cue == cue) &&
                (!casterCar || event.casterCar == casterCar) && (!targetCar || event.targetCar == targetCar) &&
                event.timeline <= timeline && event.end > timeline) event.end = timeline;
        }
        Trim(timeline);
    }

    // Normal progression starts a cue once. First reads, backward seeks and
    // large forward skips join only the clip audible at the destination, using
    // its source offset, rather than bursting all skipped casts at once.
    std::vector<Action> Advance(double timeline, bool enabled, bool paused = false) {
        std::vector<Action> actions;
        if (!std::isfinite(timeline) || timeline < 0) {
            StopPlaying(actions); lastPlayback_.reset(); return actions;
        }
        const bool seek = lastPlayback_ &&
            (timeline < *lastPlayback_ - Epsilon || timeline - *lastPlayback_ > MaxJump + Epsilon);
        if (lastPlayback_ && timeline < *lastPlayback_ && !seek) timeline = *lastPlayback_;
        if (seek || !enabled || paused) StopPlaying(actions);
        lastPlayback_ = timeline;
        if (!enabled || paused) return actions;

        std::array<const Event*, CueCount> wanted{};
        for (const auto& event : events_) {
            if (event.timeline <= timeline && timeline < event.end)
                wanted[Index(event.cue)] = &event;
        }
        // Stops precede starts globally, which also makes seek/mute transitions
        // deterministic when several independent channels overlap.
        for (std::size_t i = 0; i < CueCount; ++i) {
            const auto id = wanted[i] ? wanted[i]->id : 0;
            if (playing_[i] && playing_[i] != id) {
                actions.push_back({static_cast<SpellCue>(i), false, 0}); playing_[i] = 0;
            }
        }
        for (std::size_t i = 0; i < CueCount; ++i) {
            const auto* event = wanted[i];
            if (event && playing_[i] != event->id) {
                actions.push_back({event->cue, true, std::clamp(timeline - event->timeline, 0.0, event->duration)});
                playing_[i] = event->id;
            }
        }
        return actions;
    }

private:
    static constexpr std::size_t CueCount = static_cast<std::size_t>(SpellCue::Count);
    static constexpr double Epsilon = 1e-9;
    static std::size_t Index(SpellCue cue) { return static_cast<std::size_t>(cue); }
    static bool ValidCue(SpellCue cue) { return Index(cue) < CueCount; }
    bool PrepareRecord(double timeline) {
        if (!std::isfinite(timeline) || timeline < 0) return false;
        if (lastRecord_ && timeline < *lastRecord_ - 0.1) Clear();
        else if (lastRecord_ && timeline < *lastRecord_) return false;
        lastRecord_ = timeline;
        return true;
    }
    void Trim(double timeline) {
        while (!events_.empty() && (events_.size() > MaxEvents || events_.front().end < timeline - MaxAge))
            events_.pop_front();
    }
    void StopPlaying(std::vector<Action>& actions) {
        for (std::size_t i = 0; i < CueCount; ++i) {
            if (!playing_[i]) continue;
            actions.push_back({static_cast<SpellCue>(i), false, 0}); playing_[i] = 0;
        }
    }

    std::deque<Event> events_;
    std::array<std::uint64_t, CueCount> playing_{};
    std::optional<double> lastRecord_;
    std::optional<double> lastPlayback_;
    std::uint64_t nextId_ = 1;
};

} // namespace wizard
