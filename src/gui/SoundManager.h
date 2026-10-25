// SoundManager.h - sound effects generated in code (no audio files).
//
// Each effect is a short array of 16-bit samples computed from sine waves,
// noise and decay envelopes, loaded into an sf::SoundBuffer at start-up.
#pragma once

#include <SFML/Audio.hpp>

#include <array>

namespace gui {

enum class SoundEffect { Move, Capture, Castle, Check, GameEnd, Notify, Count };

class SoundManager {
public:
    SoundManager();
    void play(SoundEffect effect);
    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }

private:
    static constexpr int COUNT = static_cast<int>(SoundEffect::Count);
    std::array<sf::SoundBuffer, COUNT> buffers_;
    std::array<sf::Sound, COUNT> sounds_;
    bool enabled_ = true;
};

}  // namespace gui
