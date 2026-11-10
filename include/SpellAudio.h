#pragma once

#include <filesystem>
#include <memory>
#include <string>

// Local PCM playback only. The host adapter decides when an accepted cast plays.
// The volume applies to this buffer, never to Windows or the game's mixer.
class SpellAudio final {
public:
    SpellAudio();
    ~SpellAudio();
    SpellAudio(const SpellAudio&) = delete;
    SpellAudio& operator=(const SpellAudio&) = delete;

    bool Load(const std::filesystem::path& path);
    // Looping playback is explicitly stopped when its gameplay effect ends.
    bool Play(float volume = 1.0f, bool loop = false, double offsetSeconds = 0);
    void Stop();
    bool Ready() const;
    double Duration() const;
    const std::string& Error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
