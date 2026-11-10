#include "SpellAudio.h"

#include <Windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

#pragma comment(lib, "winmm.lib")

namespace {
std::uint16_t U16(const unsigned char* data) {
    return static_cast<std::uint16_t>(data[0] | (static_cast<unsigned>(data[1]) << 8));
}
std::uint32_t U32(const unsigned char* data) {
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
}
std::string WaveError(const char* operation, MMRESULT result) {
    char message[MAXERRORLENGTH]{};
    waveOutGetErrorTextA(result, message, MAXERRORLENGTH);
    return std::string(operation) + ": " + (message[0] ? message : "Windows audio error") +
        " (" + std::to_string(result) + ")";
}
}

struct SpellAudio::Impl {
    std::vector<std::int16_t> samples;
    std::vector<std::int16_t> playback;
    WAVEFORMATEX format{};
    HWAVEOUT output = nullptr;
    WAVEHDR header{};
    bool prepared = false;
    std::string error;
};

SpellAudio::SpellAudio() : impl_(std::make_unique<Impl>()) {}
SpellAudio::~SpellAudio() { Stop(); }

bool SpellAudio::Load(const std::filesystem::path& path) {
    Stop();
    impl_->samples.clear();
    impl_->playback.clear();
    impl_->format = {};
    impl_->error.clear();
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto fail = [&](const char* reason) {
        impl_->error = reason;
        return false;
    };
    if (!input) return fail("Spell audio file is missing or unreadable");
    const auto bytes = input.tellg();
    if (bytes < 12 || bytes > 16 * 1024 * 1024) return fail("Spell WAV has an invalid size");
    std::vector<unsigned char> file(static_cast<std::size_t>(bytes));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(file.data()), static_cast<std::streamsize>(file.size())))
        return fail("Spell WAV could not be read completely");
    if (std::memcmp(file.data(), "RIFF", 4) || std::memcmp(file.data() + 8, "WAVE", 4))
        return fail("Spell audio must be a PCM WAV file");
    const auto end = static_cast<std::uint64_t>(U32(file.data() + 4)) + 8;
    if (end < 12 || end > file.size()) return fail("Spell WAV RIFF size is truncated");
    bool haveFormat = false;
    const unsigned char* pcm = nullptr;
    std::uint32_t pcmBytes = 0;
    WAVEFORMATEX format{};
    for (std::uint64_t position = 12; position + 8 <= end;) {
        const auto* chunk = file.data() + static_cast<std::size_t>(position);
        const auto length = U32(chunk + 4);
        const auto next = position + 8 + static_cast<std::uint64_t>(length);
        if (next > end) return fail("Spell WAV contains a truncated chunk");
        if (!std::memcmp(chunk, "fmt ", 4)) {
            if (haveFormat || length < 16) return fail("Spell WAV has an invalid format chunk");
            format.wFormatTag = U16(chunk + 8);
            format.nChannels = U16(chunk + 10);
            format.nSamplesPerSec = U32(chunk + 12);
            format.nAvgBytesPerSec = U32(chunk + 16);
            format.nBlockAlign = U16(chunk + 20);
            format.wBitsPerSample = U16(chunk + 22);
            haveFormat = true;
        } else if (!std::memcmp(chunk, "data", 4)) {
            if (pcm) return fail("Spell WAV contains multiple data chunks");
            pcm = chunk + 8;
            pcmBytes = length;
        }
        position = next + (length & 1u);
    }
    if (!haveFormat || !pcm || !pcmBytes || format.wFormatTag != WAVE_FORMAT_PCM ||
        format.wBitsPerSample != 16 || format.nChannels < 1 || format.nChannels > 2 ||
        format.nSamplesPerSec < 8000 || format.nSamplesPerSec > 96000 ||
        format.nBlockAlign != format.nChannels * 2 || pcmBytes % format.nBlockAlign != 0 ||
        format.nAvgBytesPerSec != format.nSamplesPerSec * format.nBlockAlign)
        return fail("Spell WAV must contain 16-bit mono or stereo PCM");
    impl_->samples.resize(pcmBytes / sizeof(std::int16_t));
    std::memcpy(impl_->samples.data(), pcm, pcmBytes);
    impl_->format = format;
    impl_->playback.resize(impl_->samples.size());
    return true;
}

bool SpellAudio::Play(float volume, bool loop, double offsetSeconds) {
    if (!Ready()) {
        if (impl_->error.empty()) impl_->error = "Spell audio is not loaded";
        return false;
    }
    Stop();
    impl_->error.clear();
    if (!std::isfinite(offsetSeconds) || offsetSeconds < 0) {
        impl_->error = "Spell audio offset must be finite and nonnegative";
        return false;
    }
    if (offsetSeconds >= Duration()) return true;
    const auto offsetFrames = static_cast<std::size_t>(offsetSeconds * impl_->format.nSamplesPerSec);
    const auto offsetSamples = offsetFrames * impl_->format.nChannels;
    const float gain = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    if (gain <= 0) return true;
    if (gain == 1) impl_->playback = impl_->samples;
    else {
        for (std::size_t i = 0; i < impl_->samples.size(); ++i)
            impl_->playback[i] = static_cast<std::int16_t>(std::lround(impl_->samples[i] * gain));
    }
    const auto opened = waveOutOpen(&impl_->output, WAVE_MAPPER, &impl_->format, 0, 0, CALLBACK_NULL);
    if (opened != MMSYSERR_NOERROR) {
        impl_->output = nullptr;
        impl_->error = WaveError("Opening spell audio", opened);
        return false;
    }
    impl_->header = {};
    impl_->header.lpData = reinterpret_cast<LPSTR>(impl_->playback.data() + offsetSamples);
    impl_->header.dwBufferLength = static_cast<DWORD>((impl_->playback.size() - offsetSamples) * sizeof(std::int16_t));
    if (loop) {
        impl_->header.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
        impl_->header.dwLoops = std::numeric_limits<DWORD>::max();
    }
    const auto prepared = waveOutPrepareHeader(impl_->output, &impl_->header, sizeof(WAVEHDR));
    if (prepared != MMSYSERR_NOERROR) {
        impl_->error = WaveError("Preparing spell audio", prepared);
        Stop();
        return false;
    }
    impl_->prepared = true;
    const auto written = waveOutWrite(impl_->output, &impl_->header, sizeof(WAVEHDR));
    if (written != MMSYSERR_NOERROR) {
        impl_->error = WaveError("Playing spell audio", written);
        Stop();
        return false;
    }
    // Windows owns playback asynchronously; this class retains the samples and
    // prepared header until Stop/reset returns the buffer. No callback captures.
    return true;
}

void SpellAudio::Stop() {
    if (!impl_->output) return;
    waveOutReset(impl_->output);
    if (impl_->prepared) waveOutUnprepareHeader(impl_->output, &impl_->header, sizeof(WAVEHDR));
    waveOutClose(impl_->output);
    impl_->output = nullptr;
    impl_->prepared = false;
    impl_->header = {};
}

bool SpellAudio::Ready() const { return !impl_->samples.empty(); }
double SpellAudio::Duration() const {
    return Ready() && impl_->format.nChannels && impl_->format.nSamplesPerSec ?
        static_cast<double>(impl_->samples.size() / impl_->format.nChannels) / impl_->format.nSamplesPerSec : 0;
}
const std::string& SpellAudio::Error() const { return impl_->error; }
