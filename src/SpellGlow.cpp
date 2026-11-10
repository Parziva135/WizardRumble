#include "SpellGlow.h"

#include <functional> // ImageWrapper.h uses std::function without including it.
#include "bakkesmod/wrappers/ImageWrapper.h"
#include "bakkesmod/wrappers/canvaswrapper.h"

#include <algorithm>
#include <cmath>
#include <exception>

namespace {
// UE3 Canvas DrawTile takes EBlendMode, not ESimpleElementBlendMode.
// EBlendMode lists Opaque=0, Masked=1, Translucent=2, Additive=3.
// https://docs.unrealengine.com/udk/Three/CanvasTechnicalGuide.html
// Native appearance/blending still requires a Rocket League playtest.
constexpr unsigned char AdditiveBlend = 3;
constexpr float MinimumRadius = .5f;
constexpr double MaximumVisibleAreaFraction = .28;

bool Finite(float value) { return std::isfinite(value); }
float Unit(float value) { return std::clamp(value, 0.f, 1.f); }

class RestorePosition final {
public:
    explicit RestorePosition(CanvasWrapper& canvas)
        : canvas_(canvas), position_(canvas.GetPositionFloat()) {}
    ~RestorePosition() { canvas_.SetPosition(position_); }
private:
    CanvasWrapper& canvas_;
    Vector2F position_;
};
}

SpellGlow::~SpellGlow() { Shutdown(); }

bool SpellGlow::Load(const std::filesystem::path& folder) {
    Shutdown();
    const std::array<const char*, 2> names{{"glow_soft.png", "glow_mist.png"}};
    for (std::size_t i = 0; i < names.size(); ++i) {
        try {
            const auto path = folder / names[i];
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error) || error) {
                if (!error_.empty()) error_ += "; ";
                error_ += std::string(names[i]) + ": file missing";
                continue;
            }
            auto image = std::make_shared<ImageWrapper>(path, false, false);
            if (!image->LoadForCanvas() || !image->IsLoadedForCanvas()) {
                if (!error_.empty()) error_ += "; ";
                error_ += std::string(names[i]) + ": Canvas load failed";
                continue;
            }
            const auto size = image->GetSizeF();
            if (!Finite(size.X) || !Finite(size.Y) || size.X < 1 || size.Y < 1 ||
                size.X > 4096 || size.Y > 4096) {
                if (!error_.empty()) error_ += "; ";
                error_ += std::string(names[i]) + ": invalid texture dimensions";
                continue;
            }
            sizes_[i] = {size.X, size.Y};
            images_[i] = std::move(image);
        } catch (const std::exception&) {
            if (!error_.empty()) error_ += "; ";
            error_ += std::string(names[i]) + ": texture load error";
        }
    }
    ready_ = images_[0] && images_[1];
    return ready_;
}

bool SpellGlow::Ready() const { return ready_; }
const std::string& SpellGlow::Error() const { return error_; }

void SpellGlow::Shutdown() {
    ready_ = false;
    for (auto& image : images_) image.reset();
    sizes_ = {};
    error_.clear();
}

bool SpellGlow::Draw(CanvasWrapper& canvas, Sprite sprite, float centerX, float centerY,
    float radiusX, float radiusY, Tint tint) {
    const auto index = static_cast<std::size_t>(sprite);
    if (index >= images_.size() || !images_[index] || !images_[index]->IsLoadedForCanvas() ||
        !Finite(centerX) || !Finite(centerY) || !Finite(radiusX) || !Finite(radiusY) ||
        radiusX < MinimumRadius || radiusY < MinimumRadius ||
        !Finite(tint.r) || !Finite(tint.g) || !Finite(tint.b) || !Finite(tint.a)) return false;
    tint = {Unit(tint.r), Unit(tint.g), Unit(tint.b), Unit(tint.a)};
    if (tint.a <= 0 || (tint.r <= 0 && tint.g <= 0 && tint.b <= 0)) return false;
    const auto viewport = canvas.GetSize();
    const float width = static_cast<float>(viewport.X), height = static_cast<float>(viewport.Y);
    if (!Finite(width) || !Finite(height) || width < 1 || height < 1 || width > 32768 || height > 32768)
        return false;
    const float maximumRadius = std::max(width, height) * 4;
    if (radiusX > maximumRadius || radiusY > maximumRadius) return false;
    // Work in double until bounds are clipped, so extreme but finite projected
    // coordinates cannot overflow into a giant or nonfinite Canvas rectangle.
    const double left = static_cast<double>(centerX) - radiusX;
    const double top = static_cast<double>(centerY) - radiusY;
    const double fullWidth = static_cast<double>(radiusX) * 2;
    const double fullHeight = static_cast<double>(radiusY) * 2;
    const double x = std::max(0., left), y = std::max(0., top);
    const double right = std::min(static_cast<double>(width), left + fullWidth);
    const double bottom = std::min(static_cast<double>(height), top + fullHeight);
    const double croppedWidth = right - x, croppedHeight = bottom - y;
    if (croppedWidth < .5 || croppedHeight < .5 ||
        croppedWidth * croppedHeight > width * static_cast<double>(height) * MaximumVisibleAreaFraction)
        return false;
    const auto size = sizes_[index];
    const float u = static_cast<float>((x - left) / fullWidth * size.x);
    const float v = static_cast<float>((y - top) / fullHeight * size.y);
    const float ul = static_cast<float>(croppedWidth / fullWidth * size.x);
    const float vl = static_cast<float>(croppedHeight / fullHeight * size.y);
    // RGB carries intensity even on additive paths that ignore alpha. Asset
    // RGB also falls to black at its boundary, preventing rectangular edges.
    const LinearColor color{tint.r * tint.a, tint.g * tint.a, tint.b * tint.a, 1.f};
    RestorePosition restore(canvas);
    canvas.SetPosition(Vector2F{static_cast<float>(x), static_cast<float>(y)});
    canvas.DrawTile(images_[index].get(), static_cast<float>(croppedWidth), static_cast<float>(croppedHeight),
        u, v, ul, vl, color, 1, AdditiveBlend);
    return true;
}
