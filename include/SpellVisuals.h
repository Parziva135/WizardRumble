#pragma once

#include "SpellCore.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CanvasWrapper;
class GameWrapper;
class SpellGlow;

// Cosmetic renderer only: all physics, spell ownership and collision remain
// with the host adapter. Every time value uses Frame::time's clock domain.
class SpellVisuals final {
public:
    struct Entity {
        int id = -1;
        wizard::Vec3 position;
        wizard::Vec3 forward{1, 0, 0};
        wizard::Vec3 right{0, 1, 0};
        wizard::Vec3 up{0, 0, 1};
        wizard::Vec3 halfExtent{60, 42, 22};
        wizard::Vec3 localOffset{0, 0, 18};
        // Opaque local car identity. Never dereferenced or sent over the wire.
        std::uint64_t key = 0;
    };
    struct Wall {
        std::uint32_t id = 0;
        wizard::Vec3 center;
        wizard::Vec3 normal{1, 0, 0};
        float radius = 350;
        double began = 0;
        double ends = 0;
        double impact = -1;
    };
    struct Effect {
        wizard::Spell spell = wizard::Spell::None;
        int casterId = -1;
        int targetId = -1;
        wizard::Vec3 start;
        wizard::Vec3 end;
        double began = 0;
        double duration = 0;
        bool snapshot = false;
        // Charged spells use duration as their remaining channel lifetime.
        // A snapshot joins the channel without replaying a killing bolt.
        bool charging = false;
        // Bound once when the scene is received; numeric player IDs may be
        // shared by LAN bots and must not redirect an in-flight spell.
        std::uint64_t casterKey = 0;
        std::uint64_t targetKey = 0;
    };
    struct Patronum {
        std::uint32_t id = 0;
        wizard::Vec3 position;
        wizard::Vec3 forward{1, 0, 0};
        double began = 0;
        double ends = 0;
        double impact = -1;
        wizard::Vec3 contact;
    };
    struct Frame {
        wizard::Spell spell = wizard::Spell::None;
        double remaining = 0;
        double cooldown = 10;
        double time = 0;
        float hudScale = 1;
        bool enabled = true;
        bool synchronized = true;
        bool transportReady = true;
        bool isHost = true;
        bool showHud = true;
        bool showEffects = true;
        std::string feedback;
        std::optional<wizard::Vec3> target;
        bool ballValid = false;
        wizard::Vec3 ball;
        // Opaque recorded ball identity prevents blending across ball resets.
        std::uint64_t ballKey = 0;
        float ballRadius = 93;
        std::vector<Entity> cars;
        std::vector<Wall> walls;
        bool avadaCharging = false;
        double chargeRemaining = 0;
        bool spellCharging = false;
        int localPlayerId = -1;
        // Set only for the current local PRI/car, independently of HUD/VFX toggles.
        bool localSlugged = false;
        std::uint64_t localPlayerKey = 0;
        std::vector<Patronum> patronums;
        // Only used for bounded animation-clock advance after the last sample.
        bool replayClockRunning = true;
    };

    void Reset();
    void ClearEffects(bool preserveSlugs = false);
    void StartEffect(const Effect& effect);
    void StopEffect(wizard::Spell spell, int casterId, int targetId, double now, bool shatter = true,
        std::uint64_t casterKey = 0, std::uint64_t targetKey = 0);
    void Draw(CanvasWrapper canvas, GameWrapper& game, const Frame& frame, SpellGlow* glow = nullptr);

private:
    std::vector<Effect> effects_;
    std::vector<Effect> slugOverlays_;
    wizard::Spell lastSpell_ = wizard::Spell::None;
    double revealAt_ = -100;
};
