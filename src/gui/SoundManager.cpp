// SoundManager.cpp
#include "SoundManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gui {

namespace {

constexpr unsigned SAMPLE_RATE = 44100;
constexpr float PI = 3.14159265f;

// Tiny deterministic noise generator (so every run sounds the same).
struct Noise {
    std::uint32_t state = 12345;
    float next() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>((state >> 9) & 0x7FFF) / 16383.5f - 1.f;  // -1..1
    }
};

// A wooden "tock": a decaying low tone plus a short burst of filtered noise.
void addTock(std::vector<float>& buf, float startSec, float pitch, float loudness) {
    Noise noise;
    float lowPass = 0.f;
    const int start = static_cast<int>(startSec * SAMPLE_RATE);
    const int length = static_cast<int>(0.09f * SAMPLE_RATE);
    for (int i = 0; i < length && start + i < static_cast<int>(buf.size()); ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float body = std::sin(2 * PI * pitch * t) * std::exp(-t * 55.f);
        lowPass += 0.35f * (noise.next() - lowPass);  // simple low-pass filter
        float click = lowPass * std::exp(-t * 180.f);
        buf[start + i] += loudness * (0.75f * body + 0.9f * click);
    }
}

// A soft sine tone with attack and release (for check / game end chimes).
void addTone(std::vector<float>& buf, float startSec, float freq, float durationSec, float loudness) {
    const int start = static_cast<int>(startSec * SAMPLE_RATE);
    const int length = static_cast<int>(durationSec * SAMPLE_RATE);
    for (int i = 0; i < length && start + i < static_cast<int>(buf.size()); ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float attack = std::min(1.f, t / 0.01f);
        float release = std::exp(-t * 6.f);
        float v = std::sin(2 * PI * freq * t) + 0.3f * std::sin(4 * PI * freq * t);  // add an overtone
        buf[start + i] += loudness * attack * release * v;
    }
}

void toBuffer(const std::vector<float>& samples, sf::SoundBuffer& buffer) {
    std::vector<sf::Int16> pcm(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        float v = std::max(-1.f, std::min(1.f, samples[i]));
        pcm[i] = static_cast<sf::Int16>(v * 30000.f);
    }
    buffer.loadFromSamples(pcm.data(), pcm.size(), 1, SAMPLE_RATE);
}

}  // namespace

SoundManager::SoundManager() {
    auto make = [](float seconds) { return std::vector<float>(static_cast<std::size_t>(seconds * SAMPLE_RATE), 0.f); };

    std::vector<float> move = make(0.12f);
    addTock(move, 0.f, 190.f, 0.8f);
    toBuffer(move, buffers_[static_cast<int>(SoundEffect::Move)]);

    std::vector<float> capture = make(0.16f);
    addTock(capture, 0.f, 150.f, 0.9f);
    addTock(capture, 0.025f, 240.f, 0.7f);
    toBuffer(capture, buffers_[static_cast<int>(SoundEffect::Capture)]);

    std::vector<float> castle = make(0.25f);
    addTock(castle, 0.f, 190.f, 0.75f);
    addTock(castle, 0.11f, 170.f, 0.75f);
    toBuffer(castle, buffers_[static_cast<int>(SoundEffect::Castle)]);

    std::vector<float> check = make(0.45f);
    addTock(check, 0.f, 190.f, 0.6f);
    addTone(check, 0.02f, 880.f, 0.18f, 0.25f);
    addTone(check, 0.14f, 1175.f, 0.3f, 0.22f);
    toBuffer(check, buffers_[static_cast<int>(SoundEffect::Check)]);

    std::vector<float> end = make(1.2f);
    addTone(end, 0.00f, 523.25f, 0.9f, 0.22f);  // C
    addTone(end, 0.12f, 659.25f, 0.9f, 0.20f);  // E
    addTone(end, 0.24f, 783.99f, 0.9f, 0.20f);  // G
    addTone(end, 0.36f, 1046.5f, 0.8f, 0.16f);  // C
    toBuffer(end, buffers_[static_cast<int>(SoundEffect::GameEnd)]);

    std::vector<float> notify = make(0.3f);
    addTone(notify, 0.f, 660.f, 0.25f, 0.25f);
    toBuffer(notify, buffers_[static_cast<int>(SoundEffect::Notify)]);

    for (int i = 0; i < COUNT; ++i) sounds_[i].setBuffer(buffers_[i]);
}

void SoundManager::play(SoundEffect effect) {
    if (!enabled_ || effect == SoundEffect::Count) return;
    sf::Sound& s = sounds_[static_cast<int>(effect)];
    s.stop();
    s.play();
}

}  // namespace gui
