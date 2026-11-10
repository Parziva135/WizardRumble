#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iterator>
#include <utility>

namespace wizard {

// Cosmetic snapshots in the game's replay timeline. The adapter records only
// live play; seeking a replay uses At() and never mutates this history.
template<class T>
class ReplayHistory final {
public:
    struct Sample {
        double timeline;
        T value;
    };
    struct PlaybackSpan {
        const Sample* before = nullptr;
        const Sample* after = nullptr;
        double blend = 0;
    };

    static constexpr double MaxAge = 30.0;
    static constexpr std::size_t MaxSamples = 960;
    static constexpr double SampleInterval = 1.0 / 30.0;
    static constexpr double MaxGap = 0.1;

    void Clear() { samples_.clear(); }
    std::size_t Size() const { return samples_.size(); }

    // A forced event within the current sample interval replaces that sample.
    // It can update a stationary game clock without duplicating timestamps or
    // allowing frequent render/event callbacks to grow an unbounded history.
    bool Record(double timeline, T snapshot, bool force = false) {
        if (!std::isfinite(timeline)) return false;
        if (!samples_.empty()) {
            const double delta = timeline - samples_.back().timeline;
            if (delta < -MaxGap) Clear();
            else if (delta < 0) return false;
            else if (delta + Epsilon < SampleInterval) {
                if (!force) return false;
                samples_.back() = Sample{timeline, std::move(snapshot)};
                Trim(timeline);
                return true;
            }
        }
        samples_.push_back(Sample{timeline, std::move(snapshot)});
        Trim(timeline);
        return true;
    }

    // Never borrow a future snapshot or bridge an unrecorded interval. In
    // particular, a replay seek outside captured live play draws no effects.
    const Sample* At(double timeline) const {
        if (!std::isfinite(timeline) || samples_.empty() || timeline < samples_.front().timeline)
            return nullptr;
        const auto after = std::upper_bound(samples_.begin(), samples_.end(), timeline,
            [](double time, const Sample& sample) { return time < sample.timeline; });
        if (after == samples_.begin()) return nullptr;
        const auto found = std::prev(after);
        if (timeline - found->timeline > MaxGap + Epsilon) return nullptr;
        return &*found;
    }

    // Future samples supply continuous poses only; the adapter must keep the
    // earlier sample's spell/event state. Never interpolate through a capture
    // hole, or borrow the first future event when no earlier sample exists.
    PlaybackSpan AtSpan(double timeline) const {
        const auto* before = At(timeline);
        if (!before) return {};
        const auto next = std::upper_bound(samples_.begin(), samples_.end(), timeline,
            [](double time, const Sample& sample) { return time < sample.timeline; });
        if (next == samples_.end()) return {before, nullptr, 0};
        const double interval = next->timeline - before->timeline;
        if (!std::isfinite(interval) || interval <= Epsilon || interval > MaxGap + Epsilon)
            return {before, nullptr, 0};
        return {before, &*next, std::clamp((timeline - before->timeline) / interval, 0.0, 1.0)};
    }

private:
    static constexpr double Epsilon = 1e-9;
    void Trim(double timeline) {
        while (!samples_.empty() &&
            (samples_.size() > MaxSamples || timeline - samples_.front().timeline > MaxAge + Epsilon))
            samples_.pop_front();
    }
    std::deque<Sample> samples_;
};

} // namespace wizard
