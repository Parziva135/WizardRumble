#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>

class CanvasWrapper;
class ImageWrapper;

// One plugin-lifetime texture library. Keep it outside copied replay scenes.
// Load/Shutdown run on the plugin's game-thread lifecycle; Draw only uses the
// Canvas supplied by the current drawable callback and never loads a texture.
class SpellGlow final {
public:
    enum class Sprite { Soft, Mist };
    struct Tint { float r, g, b, a; };

    SpellGlow() = default;
    ~SpellGlow();
    SpellGlow(const SpellGlow&) = delete;
    SpellGlow& operator=(const SpellGlow&) = delete;

    bool Load(const std::filesystem::path& folder);
    bool Ready() const;
    const std::string& Error() const;
    void Shutdown();

    // Screen-space radii in pixels. The caller owns projection and budgets.
    // False means unavailable, invisible or unsafe; no lazy loading occurs.
    bool Draw(CanvasWrapper& canvas, Sprite sprite, float centerX, float centerY,
        float radiusX, float radiusY, Tint tint);

private:
    struct Size { float x = 0, y = 0; };
    std::array<std::shared_ptr<ImageWrapper>, 2> images_;
    std::array<Size, 2> sizes_{};
    bool ready_ = false;
    std::string error_;
};
