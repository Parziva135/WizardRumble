#include "SpellVisuals.h"
#include "SpellGlow.h"
#include "PatronumMesh.h"
#include "PatronumVolume.h"
#include "PatronumShell.h"

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/canvaswrapper.h"
#include "bakkesmod/wrappers/GameObject/CameraWrapper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {
using wizard::Spell;
using wizard::Vec3;
constexpr float Pi = 3.14159265358979323846f;
constexpr float Tau = Pi * 2;

struct Point {
    float x = 0;
    float y = 0;
    Point operator+(Point p) const { return {x + p.x, y + p.y}; }
    Point operator-(Point p) const { return {x - p.x, y - p.y}; }
    Point operator*(float scale) const { return {x * scale, y * scale}; }
};
struct Ink { int r, g, b, a = 255; };

float Unit(double value) { return static_cast<float>(std::clamp(value, 0.0, 1.0)); }
float Ease(double value) { const float t = Unit(value); return t * t * (3 - 2 * t); }
float Out(double value) { const float t = 1 - Unit(value); return 1 - t * t * t; }
Vec3 Lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vector Engine(Vec3 p) { return {p.x, p.y, p.z}; }
Vec3 Core(Vector p) { return {p.X, p.Y, p.Z}; }
Vector2F Pixel(Point p) { return {p.x, p.y}; }
Point Polar(Point center, float radius, float angle) {
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}
Ink Alpha(Ink ink, float opacity) { ink.a = static_cast<int>(std::clamp(ink.a * opacity, 0.0f, 255.0f)); return ink; }
Ink SpellInk(Spell spell, int alpha = 255) {
    const auto& color = wizard::GetSpellInfo(spell).color;
    return {color[0], color[1], color[2], alpha};
}
void Color(CanvasWrapper& canvas, Ink color) {
    canvas.SetColor(static_cast<char>(std::clamp(color.r, 0, 255)),
        static_cast<char>(std::clamp(color.g, 0, 255)),
        static_cast<char>(std::clamp(color.b, 0, 255)),
        static_cast<char>(std::clamp(color.a, 0, 255)));
}
void Line(CanvasWrapper& canvas, Point a, Point b, Ink color, float width = 1) {
    if (color.a <= 0) return;
    Color(canvas, color);
    canvas.DrawLine(Pixel(a), Pixel(b), std::max(0.5f, width));
}
void Triangle(CanvasWrapper& canvas, Point a, Point b, Point c, Ink color) {
    if (color.a <= 0) return;
    Color(canvas, color);
    canvas.FillTriangle(Pixel(a), Pixel(b), Pixel(c));
}
void Polygon(CanvasWrapper& canvas, const std::vector<Point>& points, Ink color) {
    if (points.size() < 3 || color.a <= 0) return;
    for (std::size_t i = 1; i + 1 < points.size(); ++i) Triangle(canvas, points[0], points[i], points[i + 1], color);
}
void Disc(CanvasWrapper& canvas, Point center, float radius, Ink color, int segments = 40) {
    for (int i = 0; i < segments; ++i)
        Triangle(canvas, center, Polar(center, radius, Tau * i / segments), Polar(center, radius, Tau * (i + 1) / segments), color);
}
void Arc(CanvasWrapper& canvas, Point center, float radius, float from, float sweep, Ink color, float width = 1, int segments = 60) {
    if (std::abs(sweep) < 0.0001f) return;
    const int count = std::max(2, static_cast<int>(std::ceil(segments * std::abs(sweep) / Tau)));
    for (int i = 0; i < count; ++i)
        Line(canvas, Polar(center, radius, from + sweep * i / count),
            Polar(center, radius, from + sweep * (i + 1) / count), color, width);
}
void Polyline(CanvasWrapper& canvas, const std::vector<Point>& points, Ink color, float width = 1, bool closed = false) {
    for (std::size_t i = 1; i < points.size(); ++i) Line(canvas, points[i - 1], points[i], color, width);
    if (closed && points.size() > 2) Line(canvas, points.back(), points.front(), color, width);
}
void Label(CanvasWrapper& canvas, Point at, const std::string& text, float scale, Ink color, bool centered = false) {
    Color(canvas, color);
    if (centered) { const auto size = canvas.GetStringSize(text, scale, scale); at.x -= size.X * 0.5f; }
    canvas.SetPosition(Pixel(at));
    canvas.DrawString(text, scale, scale, true);
}
void Spark(CanvasWrapper& canvas, Point center, float radius, Ink color) {
    Polygon(canvas, {{center.x, center.y - radius}, {center.x + radius * 0.3f, center.y},
        {center.x, center.y + radius}, {center.x - radius * 0.3f, center.y}}, color);
    Line(canvas, {center.x - radius * 0.6f, center.y}, {center.x + radius * 0.6f, center.y}, color, 1);
}

void Glyph(CanvasWrapper& canvas, Spell spell, Point center, float size, Ink ink, float time) {
    const float u = size / 30;
    const float width = std::max(1.4f, 2.5f * u);
    const float fine = std::max(0.8f, 1.15f * u);
    const Ink detail = Alpha(SpellInk(spell, ink.a), 0.85f);
    auto P = [center, u](float x, float y) { return Point{center.x + x * u, center.y + y * u}; };
    auto L = [&](float x1, float y1, float x2, float y2) { Line(canvas, P(x1, y1), P(x2, y2), ink, width); };
    auto F = [&](float x1, float y1, float x2, float y2) { Line(canvas, P(x1, y1), P(x2, y2), detail, fine); };
    auto Gem = [&](float x, float y, float radius) {
        Polyline(canvas, {P(x, y - radius), P(x + radius * 0.65f, y),
            P(x, y + radius), P(x - radius * 0.65f, y)}, detail, fine, true);
    };
    auto Path = [&](std::initializer_list<Point> points, bool filled = false) {
        std::vector<Point> result;
        for (auto p : points) result.push_back(P(p.x, p.y));
        if (filled) Polygon(canvas, result, ink); else Polyline(canvas, result, ink, width);
    };
    switch (spell) {
    case Spell::Accio:
        Arc(canvas, P(-6, 0), 24 * u, Pi * 0.47f, Pi * 1.02f, detail, fine, 28);
        Arc(canvas, P(-6, 0), 16 * u, Pi * 0.45f, Pi * 1.1f, ink, width, 28);
        Disc(canvas, P(20, -5), 5 * u, ink, 18);
        Arc(canvas, P(20, -5), 8 * u, -Pi * 0.6f, Pi * 1.25f, detail, fine, 20);
        L(11, 4, -7, 4); Path({{-1, -2}, {-8, 4}, {-1, 10}});
        F(3, -18, 8, -22); F(21, 10, 26, 15);
        Spark(canvas, P(-22, -22), 4 * u, detail);
        Gem(8, 22, 3);
        break;
    case Spell::AvadaKedavra:
        Arc(canvas, center, 28 * u, Pi * 0.73f, Pi * 0.48f, detail, fine, 24);
        Arc(canvas, center, 28 * u, -Pi * 0.27f, Pi * 0.48f, detail, fine, 24);
        Path({{6, -27}, {-16, 2}, {-3, 2}, {-10, 27}, {17, -7}, {3, -7}, {6, -27}}, true);
        Spark(canvas, P(19, -21), 5 * u, Alpha(ink, 0.8f));
        F(-28, -14, -24, -14); F(-26, -17, -26, -11);
        F(23, 16, 29, 16); F(26, 12, 26, 20);
        Gem(-20, 23, 3);
        break;
    case Spell::Leviosa:
        Path({{-19, 12}, {-14, -6}, {0, -20}, {19, -23}, {17, -5}, {5, 11}, {-19, 12}});
        L(-18, 19, 13, -15); L(-8, 9, -9, -2); L(1, 0, 13, -1);
        F(-3, 4, -3, -8); F(4, -4, 15, -7); F(6, -8, 6, -16);
        Arc(canvas, P(0, 20), 23 * u, Pi * 0.04f, Pi * 0.91f, detail, fine, 24);
        Spark(canvas, P(-23, -17), 4 * u, detail);
        Spark(canvas, P(25, 6), 3 * u, detail);
        break;
    case Spell::BombardaMaxima:
        Disc(canvas, center, 5 * u, ink, 18);
        for (int i = 0; i < 3; ++i) {
            const float a = -Pi * 0.5f + Tau * i / 3;
            Point a1 = Polar(center, 12 * u, a), a2 = Polar(center, 27 * u, a);
            Line(canvas, a1, a2, ink, width);
            Line(canvas, a2, a2 + Point{std::cos(a + 2.5f), std::sin(a + 2.5f)} * 9 * u, ink, width);
            Line(canvas, a2, a2 + Point{std::cos(a - 2.5f), std::sin(a - 2.5f)} * 9 * u, ink, width);
            Arc(canvas, center, 17 * u, a + 0.45f, 1.18f, detail, fine, 24);
            const Point flare = Polar(center, 29 * u, a + Pi / 3);
            Spark(canvas, flare, 2.8f * u, detail);
        }
        Arc(canvas, center, 9 * u, 0, Tau, detail, fine, 24);
        break;
    case Spell::Protego:
        Arc(canvas, center, 25 * u, 0, Tau, ink, width, 44);
        for (int sector = 0; sector < 4; ++sector) {
            const float angle = Pi * .25f + sector * Pi * .5f;
            Arc(canvas, center, 19 * u, angle - .38f, .76f, detail, fine, 9);
            const Point a = Polar(center, 25 * u, angle), b = Polar(center, 30 * u, angle);
            const Point tangent{-std::sin(angle) * 3 * u, std::cos(angle) * 3 * u};
            Line(canvas, a, b, ink, fine);
            Line(canvas, b - tangent, b + tangent, detail, fine);
        }
        Path({{0, -12}, {9, 0}, {0, 12}, {-9, 0}, {0, -12}});
        F(-4, -1, 4, -1); F(0, -6, 0, 5);
        Spark(canvas, P(18, -25), 3.5f * u, detail);
        break;
    case Spell::Stupefy:
        Arc(canvas, P(9, 0), 21 * u, -Pi * 0.42f, Pi * 0.82f, detail, fine, 24);
        Path({{-24, 8}, {4, 8}, {-2, 18}, {24, -3}, {-4, -3}, {3, -15}, {-24, 8}}, true);
        Spark(canvas, P(19, -20), 5 * u, ink);
        F(-28, -4, -14, -4); F(-24, -13, -13, -13);
        F(-27, 17, -15, 17);
        Spark(canvas, P(24, 20), 3 * u, detail);
        break;
    case Spell::PetrificusTotalus:
        // A bound car and rune chains replace the former time/hourglass glyph.
        Path({{-20, 13}, {-20, -6}, {-12, -16}, {12, -16}, {20, -6}, {20, 13}, {-20, 13}});
        L(-11, -13, -15, -4); L(-15, -4, 15, -4); L(15, -4, 11, -13);
        L(-21, 6, 21, 6); L(-16, 16, -9, 16); L(9, 16, 16, 16);
        F(-28, -1, 28, -1); F(-28, 10, 28, 10);
        Gem(-27, -1, 4); Gem(27, 10, 4); Gem(-7, -1, 3); Gem(8, 10, 3);
        F(-5, -25, 0, -20); F(0, -20, 5, -25); F(0, -20, 0, -30);
        Spark(canvas, P(-25, 24), 3 * u, detail);
        break;
    case Spell::ExpectoPatronum:
        Path({{-21, 13}, {-21, 0}, {-12, -11}, {7, -11}, {20, 0}, {24, 13}, {-21, 13}});
        L(-11, -9, -15, -1); L(-15, -1, 16, -1); L(16, -1, 6, -9);
        Arc(canvas, P(-13, 14), 6 * u, 0, Tau, ink, width, 16);
        Arc(canvas, P(15, 14), 6 * u, 0, Tau, ink, width, 16);
        Arc(canvas, center, 30 * u, Pi * .95f, Pi * .8f, detail, fine, 24);
        Arc(canvas, center, 27 * u, -Pi * .1f, Pi * .65f, detail, fine, 24);
        Spark(canvas, P(3, -25), 7 * u, ink); Gem(-27, -12, 3); Gem(26, -18, 3);
        break;
    case Spell::Obliviate: {
        // A curling mind spiral and opposing arrows communicate reversed controls.
        Point previous = P(0, 0);
        for (int i = 1; i <= 38; ++i) {
            const float t = i / 38.0f, angle = t * Tau * 1.45f;
            const Point point = Polar(center, 18 * u * t, angle);
            Line(canvas, previous, point, ink, width);
            previous = point;
        }
        F(-24, -23, 14, -23); F(24, 23, -14, 23);
        Path({{9, -29}, {16, -23}, {9, -17}});
        Path({{-9, 17}, {-16, 23}, {-9, 29}});
        Gem(-25, 5, 3); Gem(25, -5, 3);
        break;
    }
    case Spell::Imperio:
        // A marionette crossbar and linked keys suggest mirrored controls.
        Gem(0, -25, 4); L(0, -20, 0, -10); L(-23, -10, 23, -10);
        L(-19, -10, -13, 12); L(19, -10, 13, 12); L(0, -10, 0, 8);
        Disc(canvas, P(0, 12), 4 * u, ink, 12);
        L(0, 16, 0, 26); L(-13, 12, 13, 12); L(-13, 12, -18, 21); L(13, 12, 18, 21);
        F(-26, -22, -19, -22); F(19, -22, 26, -22);
        Gem(-22, 27, 3); Gem(22, 27, 3);
        break;
    case Spell::EatSlugs: {
        // A curled slug silhouette, antennae and a little slime trail.
        Path({{-25, 15}, {-20, 0}, {-7, -12}, {13, -10}, {23, 0}, {20, 12}, {8, 17}, {-25, 15}});
        Arc(canvas, P(-3, 4), 10 * u, -Pi * 0.2f, Pi * 1.65f, detail, fine, 24);
        L(14, -8, 14, -23); L(20, -4, 28, -18);
        Disc(canvas, P(14, -24), 3 * u, ink, 10);
        Disc(canvas, P(29, -19), 3 * u, ink, 10);
        F(-24, 23, -7, 23); F(-3, 23, 6, 23); Gem(-20, -14, 3);
        break;
    }
    default:
        Arc(canvas, center, size * 0.5f, -Pi * 0.5f + time, Pi * 1.5f, ink, width, 30);
        break;
    }
}

int Region(Point p, float width, float height) {
    return (p.x < -2 ? 1 : 0) | (p.x > width + 2 ? 2 : 0) |
        (p.y < -2 ? 4 : 0) | (p.y > height + 2 ? 8 : 0);
}
bool ClipLine(Point& a, Point& b, float width, float height) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        const int ca = Region(a, width, height), cb = Region(b, width, height);
        if (!(ca | cb)) return true;
        if (ca & cb) return false;
        const int code = ca ? ca : cb;
        Point p;
        if (code & 8) { if (std::abs(b.y - a.y) < 0.001f) return false; p.y = height + 2; p.x = a.x + (b.x - a.x) * (p.y - a.y) / (b.y - a.y); }
        else if (code & 4) { if (std::abs(b.y - a.y) < 0.001f) return false; p.y = -2; p.x = a.x + (b.x - a.x) * (p.y - a.y) / (b.y - a.y); }
        else if (code & 2) { if (std::abs(b.x - a.x) < 0.001f) return false; p.x = width + 2; p.y = a.y + (b.y - a.y) * (p.x - a.x) / (b.x - a.x); }
        else { if (std::abs(b.x - a.x) < 0.001f) return false; p.x = -2; p.y = a.y + (b.y - a.y) * (p.x - a.x) / (b.x - a.x); }
        if (ca) a = p; else b = p;
    }
    return false;
}

class WorldCanvas {
public:
    WorldCanvas(CanvasWrapper& c, Vec3 position, Vec3 forward, Vec3 right, Vec3 up, SpellGlow* glow = nullptr)
        : canvas(c), camera(position), direction(forward), cameraRight(right), cameraUp(up), glow_(glow) {
        const auto size = c.GetSize(); width = static_cast<float>(size.X); height = static_cast<float>(size.Y);
    }
    bool HasGlow() const { return glow_ && glow_->Ready(); }
    bool Glow(Vec3 center, float radius, Ink ink, float intensity = 1, bool mist = false, float aspect = 1) {
        if (budget <= 0 || !HasGlow() || !center.IsFinite() || !std::isfinite(radius) ||
            !std::isfinite(intensity) || !std::isfinite(aspect) || radius <= 0 || intensity <= 0 ||
            aspect < .15f || aspect > 6 || ink.a <= 0) return false;
        const float depth = (center - camera).Dot(direction);
        if (depth <= std::max(Near + 1, radius * .6f)) return false;
        Point point, edge;
        if (!Project(center, point) || !Project(center + cameraRight * radius, edge)) return false;
        const float projected = std::hypot(edge.x - point.x, edge.y - point.y);
        const float limit = std::clamp(height * .19f, 80.f, 410.f);
        if (!std::isfinite(projected) || projected < .5f) return false;
        const float rx = std::min(projected, limit), ry = std::min(projected * aspect, limit);
        const float area = std::max(0.f, std::min(width, point.x + rx) - std::max(0.f, point.x - rx)) *
            std::max(0.f, std::min(height, point.y + ry) - std::max(0.f, point.y - ry));
        if (glowPixels_ + area > width * height * 2.5f) return false;
        const float strength = std::clamp(intensity, 0.f, 1.f);
        const SpellGlow::Tint tint{ink.r / 255.f * strength, ink.g / 255.f * strength,
            ink.b / 255.f * strength, std::clamp(ink.a / 255.f, 0.f, 1.f)};
        if (!glow_->Draw(canvas, mist ? SpellGlow::Sprite::Mist : SpellGlow::Sprite::Soft,
                point.x, point.y, rx, ry, tint)) return false;
        --budget;
        glowPixels_ += area;
        return true;
    }
    bool Project(Vec3 p, Point& result) {
        if (!p.IsFinite() || (p - camera).Dot(direction) < Near) return false;
        const auto projected = canvas.ProjectF(Engine(p));
        if (!std::isfinite(projected.X) || !std::isfinite(projected.Y)) return false;
        result = {projected.X, projected.Y};
        return true;
    }
    void Segment(Vec3 a, Vec3 b, Ink ink, float thickness = 1) {
        if (budget <= 0 || !a.IsFinite() || !b.IsFinite() || ink.a <= 0) return;
        float da = (a - camera).Dot(direction), db = (b - camera).Dot(direction);
        if (da < Near && db < Near) return;
        if (da < Near) a = Lerp(a, b, (Near + 0.1f - da) / (db - da));
        if (db < Near) b = Lerp(b, a, (Near + 0.1f - db) / ((a - camera).Dot(direction) - db));
        Point pa, pb;
        if (!Project(a, pa) || !Project(b, pb) || !ClipLine(pa, pb, width, height)) return;
        --budget; Line(canvas, pa, pb, ink, thickness);
    }
    void Facet(Vec3 a, Vec3 b, Vec3 c, Ink ink) {
        if (budget <= 0 || !a.IsFinite() || !b.IsFinite() || !c.IsFinite() || ink.a <= 0) return;
        std::vector<Vec3> source{a, b, c}, clipped;
        for (std::size_t i = 0; i < source.size(); ++i) {
            const Vec3 p = source[i], q = source[(i + 1) % source.size()];
            const float dp = (p - camera).Dot(direction), dq = (q - camera).Dot(direction);
            if (dp >= Near + 0.1f) clipped.push_back(p);
            if ((dp >= Near + 0.1f) != (dq >= Near + 0.1f))
                clipped.push_back(Lerp(p, q, (Near + 0.1f - dp) / (dq - dp)));
        }
        std::vector<Point> polygon;
        for (auto p : clipped) { Point screen; if (!Project(p, screen)) return; polygon.push_back(screen); }
        // Clip projected geometry to the viewport, so a near-plane crossing
        // can never produce a giant full-screen ice or shield triangle.
        for (int edge = 0; edge < 4 && !polygon.empty(); ++edge) {
            std::vector<Point> next;
            auto distance = [=](Point p) { return edge == 0 ? p.x : edge == 1 ? width - p.x : edge == 2 ? p.y : height - p.y; };
            for (std::size_t i = 0; i < polygon.size(); ++i) {
                const Point p = polygon[i], q = polygon[(i + 1) % polygon.size()];
                const float dp = distance(p), dq = distance(q);
                if (dp >= 0) next.push_back(p);
                if ((dp >= 0) != (dq >= 0)) next.push_back(p + (q - p) * (dp / (dp - dq)));
            }
            polygon = std::move(next);
        }
        if (polygon.size() < 3) return;
        const int triangles = static_cast<int>(polygon.size()) - 2;
        if (triangles > budget) return;
        budget -= triangles; Polygon(canvas, polygon, ink);
    }
    void Circle(Vec3 center, Vec3 a, Vec3 b, float radius, Ink ink, float thickness = 1, float phase = 0, int segments = 30) {
        for (int i = 0; i < segments; ++i) {
            const float t1 = phase + Tau * i / segments, t2 = phase + Tau * (i + 1) / segments;
            Segment(center + (a * std::cos(t1) + b * std::sin(t1)) * radius,
                center + (a * std::cos(t2) + b * std::sin(t2)) * radius, ink, thickness);
        }
    }
    void Glint(Vec3 center, float radius, Ink ink) {
        const auto a = cameraRight * radius, b = cameraUp * radius;
        Facet(center - a * 0.32f, center + b, center + a * 0.32f, ink);
        Facet(center - a * 0.32f, center - b, center + a * 0.32f, ink);
        Segment(center - a * 0.6f, center + a * 0.6f, ink, 1);
    }
    CanvasWrapper& canvas;
    Vec3 camera, direction, cameraRight, cameraUp;
    float width = 1920, height = 1080;
    int budget = 2200;
private:
    SpellGlow* glow_ = nullptr;
    float glowPixels_ = 0;
    static constexpr float Near = 12;
};

void TargetReticle(WorldCanvas& world, const SpellVisuals::Frame& frame) {
    const bool connected = frame.isHost || (frame.synchronized && frame.transportReady);
    if (!frame.showHud || !frame.enabled || !connected || !wizard::IsCastableSpell(frame.spell) ||
        !frame.target || !frame.target->IsFinite()) return;
    Point center;
    if (!world.Project(*frame.target, center)) return;
    const float scale = std::clamp(world.height / 1080.0f, 0.65f, 2.0f);
    const bool killingCurse = frame.spell == Spell::AvadaKedavra;
    const float radius = (killingCurse ? 29.0f : 25.0f) * scale;
    const float margin = radius + (killingCurse ? 15.0f : 5.0f) * scale;
    if (center.x < margin || center.x > world.width - margin ||
        center.y < margin || center.y > world.height - margin) return;
    const Ink accent = SpellInk(frame.spell, 245), shadow{2, 9, 15, 215};
    auto stroke = [&](Point a, Point b, Ink ink, float width = 1.6f) {
        Line(world.canvas, a, b, shadow, (width + 2.5f) * scale);
        Line(world.canvas, a, b, ink, width * scale);
    };
    if (killingCurse) {
        const Ink green{54, 255, 91, 255}, core{209, 255, 218, 255};
        struct Mark { Point a, b; Ink ink; float width; };
        std::vector<Mark> marks;
        const auto markLine = [&](Point a, Point b, Ink ink, float width) {
            marks.push_back({a, b, ink, width});
        };
        const bool charging = (frame.spellCharging || frame.avadaCharging) && std::isfinite(frame.chargeRemaining);
        const float fullCharge = static_cast<float>(wizard::SpellChargeSeconds(frame.spell));
        const float remaining = charging ? std::clamp(static_cast<float>(frame.chargeRemaining), 0.0f, fullCharge) : fullCharge;
        const float progress = 1 - remaining / fullCharge;
        const float pulse = charging ? 1 + 0.06f * std::sin(static_cast<float>(frame.time) * 14) : 1;
        // Four angular lightning brackets and small ward marks distinguish
        // this lock from the other spell reticles. The target stays clear.
        for (int corner = 0; corner < 4; ++corner) {
            const float angle = Pi * 0.25f + corner * Pi * 0.5f;
            const Point radial{std::cos(angle), std::sin(angle)};
            const Point tangent{-radial.y, radial.x};
            const Point p = center + radial * (radius * pulse);
            markLine(p - tangent * (10 * scale), p + radial * (4 * scale), green, 2);
            markLine(p + radial * (4 * scale), p + tangent * (10 * scale), green, 2);
            const Point mark = center + radial * (42 * scale);
            const std::array<Point, 4> rune{{mark - tangent * (4 * scale) - radial * (5 * scale),
                mark + tangent * (2 * scale), mark - tangent * (2 * scale),
                mark + tangent * (4 * scale) + radial * (5 * scale)}};
            for (std::size_t i = 1; i < rune.size(); ++i) markLine(rune[i - 1], rune[i], green, 1.1f);
            const float from = corner * Pi * 0.5f + 0.15f;
            Arc(world.canvas, center, 23 * scale, from, 0.72f, shadow, 3.7f * scale, 56);
            Arc(world.canvas, center, 23 * scale, from, 0.72f, Alpha(green, 0.8f), 1.2f * scale, 56);
            markLine(Polar(center, 6 * scale, corner * Pi * 0.5f),
                Polar(center, 11 * scale, corner * Pi * 0.5f), core, 1.1f);
        }
        // Draw shadows together so the next rune stroke never erases a
        // previous green stroke at its small angular join.
        for (const auto& mark : marks) Line(world.canvas, mark.a, mark.b, shadow, (mark.width + 2.5f) * scale);
        for (const auto& mark : marks) Line(world.canvas, mark.a, mark.b, mark.ink, mark.width * scale);
        if (charging) {
            Arc(world.canvas, center, 35 * scale, -Pi * 0.5f, Tau, {13, 72, 36, 230}, 2.8f * scale, 64);
            Arc(world.canvas, center, 35 * scale, -Pi * 0.5f, Tau * progress, green, 2.8f * scale, 64);
            const Point head = Polar(center, 35 * scale, -Pi * 0.5f + Tau * progress);
            Line(world.canvas, head - Point{2 * scale, 0}, head + Point{2 * scale, 0}, core, 1.2f * scale);
            Line(world.canvas, head - Point{0, 2 * scale}, head + Point{0, 2 * scale}, core, 1.2f * scale);
        }
        return;
    }
    // A stable center marks the actual selected target. The broken arc and
    // small diamond echo the spell slot without concealing the ball or car.
    for (int corner = 0; corner < 4; ++corner) {
        const float x = corner & 1 ? 1.0f : -1.0f, y = corner & 2 ? 1.0f : -1.0f;
        const Point outer = center + Point{x * radius, y * radius};
        stroke(outer, outer - Point{x * 9 * scale, 0}, accent);
        stroke(outer, outer - Point{0, y * 9 * scale}, accent);
        const float angle = Tau * corner / 4 + Pi * 0.15f;
        Arc(world.canvas, center, 23 * scale, angle, Pi * 0.2f, shadow, 3.6f * scale, 48);
        Arc(world.canvas, center, 23 * scale, angle, Pi * 0.2f, Alpha(accent, 0.55f), scale, 48);
    }
    const Ink focus{235, 255, 249, 245};
    for (int axis = 0; axis < 4; ++axis) {
        const float angle = Tau * axis / 4;
        stroke(Polar(center, 8 * scale, angle), Polar(center, 14 * scale, angle), focus, 1.2f);
    }
    const std::vector<Point> diamond{center + Point{0, -2.4f * scale}, center + Point{2.4f * scale, 0},
        center + Point{0, 2.4f * scale}, center + Point{-2.4f * scale, 0}};
    Polyline(world.canvas, diamond, shadow, 3.5f * scale, true);
    Polyline(world.canvas, diamond, accent, 1.2f * scale, true);
    if (wizard::IsChargedSpell(frame.spell) && frame.spellCharging && std::isfinite(frame.chargeRemaining)) {
        const float progress = 1 - Unit(frame.chargeRemaining / wizard::SpellChargeSeconds(frame.spell));
        const Ink charge = SpellInk(frame.spell);
        Arc(world.canvas, center, 34 * scale, -Pi * 0.5f, Tau, {charge.r/4,charge.g/4,charge.b/4,240}, 3 * scale, 64);
        Arc(world.canvas, center, 34 * scale, -Pi * 0.5f, Tau * progress, charge, 3 * scale, 64);
        for (int mark = 0; mark < 3; ++mark) {
            const float angle = Tau * mark / 3 - static_cast<float>(frame.time) * 0.7f;
            stroke(Polar(center, 40 * scale, angle), Polar(center, 46 * scale, angle), {243, 216, 255, 255}, 1.3f);
        }
    }
}

const SpellVisuals::Entity* Entity(const SpellVisuals::Frame& frame, int id, std::uint64_t key) {
    const SpellVisuals::Entity* result = nullptr;
    for (const auto& car : frame.cars) {
        if ((key ? car.key == key : car.id == id) && car.position.IsFinite()) {
            // Ambiguous legacy IDs are safer at their captured endpoint than
            // attached to whichever bot happens to be first in the roster.
            if (result) return nullptr;
            result = &car;
        }
    }
    return result;
}
Vec3 Position(const SpellVisuals::Frame& frame, int id, Vec3 fallback, std::uint64_t key = 0) {
    const auto* car = Entity(frame, id, key);
    return car ? car->position : fallback;
}
bool SameActor(int firstId, std::uint64_t firstKey, int secondId, std::uint64_t secondKey) {
    return firstKey || secondKey ? firstKey != 0 && firstKey == secondKey : firstId == secondId;
}
void Plane(Vec3 normal, Vec3& right, Vec3& up) {
    normal = normal.Normalized();
    if (normal.LengthSquared() < 0.5f) normal = {1, 0, 0};
    const Vec3 reference = std::abs(normal.z) > 0.95f ? Vec3{0, 1, 0} : Vec3{0, 0, 1};
    right = Cross(reference, normal).Normalized();
    if (right.LengthSquared() < 0.5f) right = {1, 0, 0};
    up = Cross(normal, right).Normalized();
}
Vec3 Scatter(int i, float phase = 0) {
    const float z = 1 - 2 * (i + 0.5f) / 18;
    const float radius = std::sqrt(std::max(0.0f, 1 - z * z));
    const float angle = i * 2.3999632f + phase;
    return {std::cos(angle) * radius, std::sin(angle) * radius, z};
}

// A busy scene may contain eight physical wards. Give each remaining visual
// a bounded share without letting a decorative curl consume the next cast.
class VisualBudget {
public:
    VisualBudget(WorldCanvas& canvas, int limit)
        : world(canvas), available(canvas.budget), allowance(std::min(canvas.budget, limit)) {
        world.budget = allowance;
    }
    ~VisualBudget() { world.budget = available - (allowance - world.budget); }
private:
    WorldCanvas& world;
    int available, allowance;
};

void LineSpark(WorldCanvas& world, Vec3 center, float radius, Ink ink, float angle = 0) {
    const Vec3 a = (world.cameraRight * std::cos(angle) + world.cameraUp * std::sin(angle)) * radius;
    const Vec3 b = (world.cameraUp * std::cos(angle) - world.cameraRight * std::sin(angle)) * radius;
    world.Segment(center - a, center + a, ink, 1.5f);
    world.Segment(center - b, center + b, ink, 1.5f);
    world.Segment(center - (a + b) * 0.33f, center + (a + b) * 0.33f, ink, 1);
    world.Segment(center - (a - b) * 0.33f, center + (a - b) * 0.33f, ink, 1);
}

void WorldArc(WorldCanvas& world, Vec3 center, Vec3 right, Vec3 up, float radius,
    float from, float sweep, Ink ink, float thickness = 1.5f, int segments = 12) {
    for (int i = 0; i < segments; ++i) {
        const float a = from + sweep * i / segments, b = from + sweep * (i + 1) / segments;
        world.Segment(center + (right * std::cos(a) + up * std::sin(a)) * radius,
            center + (right * std::cos(b) + up * std::sin(b)) * radius, ink, thickness);
    }
}

void Burst(WorldCanvas& world, Vec3 center, Ink ink, float age, float scale = 1) {
    if (age < 0 || age > 0.85f) return;
    const float expansion = Out(age / .85f);
    const float opacity = (1 - expansion) * (1 - age / .85f);
    const Ink core{(ink.r + 765) / 4, (ink.g + 765) / 4, (ink.b + 765) / 4, 255};
    world.Glow(center, (75 + expansion * 95) * scale, ink, opacity * .25f, true);
    world.Glow(center, (28 + expansion * 50) * scale, core, opacity * .3f);
    // Broken pressure fronts preserve the impact's center instead of placing
    // the same complete target ring on every spell. No filled blast discs.
    for (int lobe = 0; lobe < 3; ++lobe) {
        const float offset = lobe * Tau / 3 + .19f;
        const float radius = (28 + expansion * (150 + 18 * lobe)) * scale;
        WorldArc(world, center, world.cameraRight, world.cameraUp, radius,
            offset, .72f, Alpha(ink, opacity * .68f), 1.5f, 5);
    }
    for (int i = 0; i < 15; ++i) {
        const Vec3 direction = Scatter(i, .27f);
        const float speed = 240 + 25.0f * (i % 5);
        const Vec3 start = center + direction * ((18 + expansion * speed) * scale) +
            Vec3{0, 0, -age * age * 80 * scale};
        const Vec3 tip = start + direction * ((24 + 30 * (1 - expansion)) * scale);
        world.Segment(start, tip, Alpha(ink, opacity), i % 3 == 0 ? 3.2f : 1.6f);
        world.Segment(Lerp(start, tip, .55f), tip, Alpha(core, opacity), .85f);
        if (i % 5 == 0) LineSpark(world, tip, (8 - age * 6) * scale,
            Alpha(core, opacity), .6f + i);
    }
}
void Bolt(WorldCanvas& world, Vec3 start, Vec3 end, Ink ink, float age, float bend = 0) {
    const float progress = Out(age / 0.24f), fade = 1 - Unit((age - 0.34f) / 0.76f);
    if (age < 0 || fade <= 0) return;
    Vec3 normal, up; Plane((end - start).Normalized(), normal, up);
    const float distance = (end - start).Length();
    const float spread = std::clamp(distance * 0.018f, 32.0f, 95.0f);
    const float flow = age * 13;
    const int segments = 24;
    std::array<Vec3, segments + 1> path;
    Vec3 previous = start;
    path[0] = start;
    const Ink core{(ink.r + 765) / 4, (ink.g + 765) / 4, (ink.b + 765) / 4, 255};
    for (int i = 1; i <= segments; ++i) {
        const float t = progress * i / segments;
        const float envelope = std::sin(t * Pi);
        const float jag = std::sin(i * 2.31f + flow) * .7f +
            std::sin(i * 5.17f - flow * 1.4f) * .3f;
        const float second = std::sin(i * 3.79f - flow * .8f);
        const Vec3 point = Lerp(start, end, t) + up * ((bend + second * spread * 0.6f) * envelope) +
            normal * (jag * envelope * spread);
        // The complete main filament is drawn first, before optional forks.
        const float taper = .65f + .35f * std::sin(Pi * t);
        world.Glow(point, 22 + envelope * 9, ink, fade * .13f);
        world.Segment(previous, point, Alpha(ink, fade * 0.22f), 7.5f * taper);
        world.Segment(previous, point, Alpha(ink, fade), 3.3f * taper);
        world.Segment(previous, point, Alpha(core, fade), 1.1f * taper);
        path[i] = point;
        previous = point;
    }
    LineSpark(world, previous, 21 + 3 * std::sin(age * 14), Alpha(core, fade), age * .8f);
    for (int fork = 0; fork < 5; ++fork) {
        const int index = 4 + fork * 4;
        const float sign = fork % 2 ? -1.0f : 1.0f;
        Vec3 a = path[index];
        for (int step = 1; step <= 3; ++step) {
            const Vec3 b = a + normal * (sign * (18 + step * 10) * (0.8f + progress)) +
                up * (std::sin(fork * 3.1f + step * 2.3f + flow * .4f) * 24) +
                (end - start).Normalized() * (15 + spread * 0.2f);
            world.Segment(a, b, Alpha(ink, fade * (1 - step * 0.15f)), 2.3f);
            world.Segment(a, b, Alpha(core, fade * 0.7f), 0.8f);
            a = b;
        }
    }
    if (age < 0.3f) {
        LineSpark(world, start, 32 * (1 - age / 0.3f), Alpha(core, fade), -age * 3);
    }
    if (progress >= 0.98f) Burst(world, end, ink, age - 0.18f, 1.1f);
}

void AvadaLuminous(WorldCanvas& world, Vec3 start, Vec3 end, float age) {
    if (age < 0 || age >= 1.2f) return;
    const float fade = 1 - Ease((age - .32f) / .88f);
    const Vec3 direction = (end - start).Normalized();
    Vec3 across, up; Plane(direction, across, up);
    const Ink emerald{18, 255, 54, 255}, pale{169, 255, 190, 255};
    // A hot impact and broad feathered light are the first protected draws.
    // Every puff is the actual additive texture, not a post-blur of lines.
    const float hit = 1 - Ease(age / .7f);
    world.Glow(end, 140 + Out(age / .35f) * 100, emerald, hit * .28f, true);
    world.Glow(end, 72 + Out(age / .23f) * 65, emerald, hit * .48f);
    world.Glow(end, 28 + hit * 24, pale, hit * .72f);
    world.Glow(start, 58, emerald, fade * .26f);
    // Overlapping mist and hot-core samples form a continuous luminous shaft.
    // The low-frequency spine bends smoothly; secondary filaments carry the
    // small jagged lightning detail without becoming the whole silhouette.
    const int samples = std::clamp((world.budget - 20) / 4, 10, 64);
    const float spacing = (end - start).Length() / samples;
    std::array<Vec3, 65> spine{};
    for (int i = 0; i <= samples; ++i) {
        const float t = i / static_cast<float>(samples), envelope = std::sin(t * Pi);
        const float flow = age * 7;
        spine[i] = Lerp(start, end, t) + across * ((std::sin(t * 21 + flow) * 15 +
            std::sin(t * 43 - flow * .7f) * 7) * envelope) +
            up * (std::sin(t * 17 - flow * .6f) * envelope * 12);
        const float breath = .8f + .2f * std::sin(t * 18 - age * 9);
        const float radius = std::clamp(spacing * 1.15f, 36.f, 110.f) + envelope * 7;
        world.Glow(spine[i], radius * 1.8f, emerald, fade * breath * .05f, true);
        world.Glow(spine[i], radius, emerald, fade * breath * .15f);
        world.Glow(spine[i], std::clamp(spacing * .72f, 23.f, 70.f), pale, fade * breath * .25f);
        if (i) world.Segment(spine[i - 1], spine[i], Alpha(pale, fade * .8f), 1.25f);
    }
    for (int fork = 0; fork < 8; ++fork) {
        const int index = 2 + fork * (samples - 4) / 7;
        Vec3 previous = spine[index];
        const float side = fork % 2 ? 1.f : -1.f;
        for (int part = 1; part <= 4; ++part) {
            const Vec3 next = previous + across * (side * (23 + part * 12.f)) +
                direction * (14 + part * 5.f) +
                up * (std::sin(fork * 3.7f + part * 5.1f + age * 5) * (24 + part * 8.f));
            const float strength = fade * (1 - part * .17f);
            world.Glow(next, 17.f + part, emerald, strength * .2f);
            world.Segment(previous, next, Alpha({81, 255, 121, 255}, strength), 1.2f);
            previous = next;
        }
    }
    if (age < .65f) for (int mote = 0; mote < 9; ++mote) {
        const Vec3 ray = Scatter(mote, .35f);
        const Vec3 p = end + ray * (35 + Out(age / .65f) * (150 + mote * 8.f));
        const float light = (1 - age / .65f) * .3f;
        world.Glow(p, 24 + mote % 3 * 8.f, emerald, light, mote % 2 == 0);
    }
}

void AvadaBeam(WorldCanvas& world, Vec3 start, Vec3 end, float age) {
    if (world.HasGlow()) { AvadaLuminous(world, start, end, age); return; }
    if (age < 0 || age >= 1.2f) return;
    const float fade = 1 - Ease((age - .28f) / .92f);
    const float scale = std::clamp(world.height / 1080.0f, .65f, 1.8f);
    Vec3 across, up; Plane(end - start, across, up);
    const Vec3 direction = (end - start).Normalized();
    const float spread = std::clamp((end-start).Length() * .031f, 48.0f, 155.0f);
    const float flow = age * 9.0f;
    const Ink green{24,255,64,255}, hot{220,255,197,255};

    // The native hit is immediate after charging. Put the green detonation
    // first, so the killing impact remains visible in a crowded frame.
    const float flash = 1 - Ease(age / .52f);
    if (flash > 0) {
        for (int ray=0; ray<18; ++ray) {
            const Vec3 dir=Scatter((ray*7)%18,ray*.061f);
            const Vec3 a=end+dir*(14+age*210);
            const Vec3 b=end+dir*(110+Out(age/.38f)*(ray%3?300.0f:405.0f));
            world.Segment(a,b,Alpha(green,flash), (ray%3?3.0f:6.0f)*scale);
            world.Segment(a,b,Alpha(hot,flash), (ray%3?.8f:1.55f)*scale);
            if (ray%3==0) {
                const Vec3 tip=b+across*(std::sin(ray*4.1f)*45)+up*(35+ray*3.0f);
                world.Segment(b,tip,Alpha(green,flash*.8f),1.3f*scale);
            }
        }
        for (int shard=0; shard<8; ++shard) {
            const Vec3 dir=Scatter((shard+13)%18,.4f);
            const Vec3 a=end+dir*(55+age*380);
            const Vec3 b=a+dir*(25+flash*35)+up*(age*30);
            world.Segment(a,b,Alpha({84,255,122,255},flash),1.7f*scale);
        }
        LineSpark(world,end,40+flash*65,Alpha(hot,flash),.2f);
    }

    // An ordered bright heart and loose outer strings give the strike a
    // coherent silhouette. Continuous phase avoids replay-visible kink jumps.
    constexpr int steps=24;
    std::array<Vec3,steps+1> spine{};
    for (int strand=0;strand<5;++strand) {
        Vec3 previous=start;
        spine[0]=start;
        for (int i=1;i<=steps;++i) {
            const float t=i/static_cast<float>(steps), envelope=std::sin(t*Pi);
            const float phase=t*Tau*(strand?2.2f:1.1f)+flow+strand*1.83f;
            const float jag=std::sin(i*12.73f+flow*1.8f+strand*4.13f);
            const float kink=std::sin(i*23.31f-flow*1.2f+strand*7.91f);
            const float radius=strand?(.28f+strand*.1f):.12f;
            const Vec3 point=Lerp(start,end,t)+across*((std::cos(phase)*radius+jag*.3f)*spread*envelope)+
                up*((std::sin(phase)*radius+kink*.22f)*spread*envelope);
            const float pulse=.76f+.24f*std::pow(.5f+.5f*std::sin(t*Tau*2-flow*2.0f),3.0f);
            if (strand==0) world.Segment(previous,point,Alpha({7,135,32,180},fade),9.0f*scale);
            world.Segment(previous,point,Alpha(strand%2?Ink{63,255,101,255}:green,fade*pulse),
                (strand?2.4f:4.8f)*scale);
            world.Segment(previous,point,Alpha(hot,fade*pulse), (strand?.7f:1.35f)*scale);
            if (strand==0) spine[i]=point;
            previous=point;
        }
    }
    for (int fork=0;fork<9;++fork) {
        const int anchor=3+fork*(steps-6)/8;
        const float sign=fork%2?-1.0f:1.0f;
        Vec3 a=spine[anchor];
        for (int j=0;j<3;++j) {
            const Vec3 b=a+across*(sign*(33.0f+j*19.0f))+direction*(22.0f+j*14.0f)+
                up*(std::sin(fork*4.13f+j*12.11f+flow*.9f)*(43.0f+j*19.0f));
            world.Segment(a,b,Alpha(green,fade*(1-j*.22f)),(2.2f-j*.4f)*scale);
            world.Segment(a,b,Alpha(hot,fade*(.8f-j*.2f)),.65f*scale);
            if (j==1) {
                const Vec3 branch=b+Scatter((fork+7)%18,flow*.12f)*(45.0f+fork*3.0f);
                world.Segment(b,branch,Alpha({74,255,106,255},fade*.7f),1.0f*scale);
            }
            a=b;
        }
    }
    for(int ember=0;ember<6;++ember) {
        const Vec3 ray=Scatter((ember+9)%18,.25f);
        world.Segment(start+ray*12,start+ray*(32+Out(age/.22f)*40),Alpha(hot,fade),1.25f*scale);
    }
}


// This is a short local atmosphere treatment, not a mutation of the map's sky
// material. Its upper-view boundary follows the projected world horizon.
void AvadaSkyFlash(WorldCanvas& world, float age) {
    if (age < 0 || age >= 0.65f) return;
    const float strength = Ease(age / 0.04f) * (1 - Ease((age - 0.15f) / 0.50f));
    if (strength < 0.025f) return;
    Vec3 horizontal{world.direction.x, world.direction.y, 0};
    horizontal = horizontal.Normalized();
    float boundary = world.direction.z > 0 ? world.height * 0.76f : 0;
    Point horizon;
    if (horizontal.LengthSquared() > 0.5f && world.Project(world.camera + horizontal * 20000, horizon))
        boundary = std::clamp(horizon.y, 0.0f, world.height * 0.76f);
    if (boundary < 4) return;
    const float step = std::max(1.5f, world.height / 540.0f);
    // Fine screen-door shading leaves real scenery exposed even if the
    // Canvas backend ignores alpha. Broad wavy screen-space ribbons have
    // been removed: they read as scanline artifacts rather than world light.
    int rows = 0;
    for (float y = 0; y < boundary && rows < 480; y += step, ++rows) {
        const float seed = std::fmod(rows * 0.6180339f, 1.0f);
        if (seed > strength) continue;
        const float depth = y / std::max(1.0f, boundary);
        const Ink shade{4 + static_cast<int>(depth * 5), 23 + static_cast<int>(depth * 31),
            11 + static_cast<int>(depth * 10), 220};
        const float thickness = std::max(0.5f, step * (0.55f + .17f * (1-depth)) * strength);
        Line(world.canvas, {0,y}, {world.width,y}, shade, thickness);
    }
}

void SpiralCast(WorldCanvas& world, Vec3 start, Vec3 end, Ink first, Ink second,
    float age, float bend, float turns = 2.0f) {
    const float progress = Out(age / 0.28f), fade = 1 - Unit((age - 0.35f) / 0.75f);
    if (age < 0 || fade <= 0) return;
    Vec3 across, up; Plane(end - start, across, up);
    for (int strand = 0; strand < 2; ++strand) {
        Vec3 previous = start;
        const Ink ink = Alpha(strand ? second : first, fade);
        for (int i = 1; i <= 28; ++i) {
            const float t = progress * i / 28, envelope = std::sin(t * Pi);
            const float phase = t * Tau * turns - age * 5 + strand * Pi;
            const Vec3 point = Lerp(start, end, t) + up * (bend * envelope) +
                (across * std::cos(phase) + up * std::sin(phase)) * (envelope * (32 + 16 * strand));
            const float taper = .55f + .45f * std::sin(Pi * t);
            world.Glow(point, 28 + envelope * 14, strand ? second : first, fade * .09f, true);
            world.Segment(previous, point, Alpha(ink, 0.21f), 5.2f * taper);
            world.Segment(previous, point, ink, (strand ? 1.2f : 2.1f) * taper);
            previous = point;
        }
        LineSpark(world, previous, 15, Alpha({244, 249, 255, 255}, fade), age + strand);
    }
    if (progress > 0.98f) {
        for (int mote = 0; mote < 6; ++mote) {
            const Vec3 direction = Scatter(mote, .9f);
            const Vec3 p = end + direction * (25 + Out((age - .16f) / .6f) * 120);
            world.Segment(p, p + direction * 20, Alpha(first, fade), 1.3f);
        }
    }
}

double OutlineTurn(Point a, Point b, Point c) {
    return static_cast<double>(b.x - a.x) * (c.y - a.y) -
        static_cast<double>(b.y - a.y) * (c.x - a.x);
}

std::vector<Point> OutlineHull(std::vector<Point> points) {
    std::sort(points.begin(), points.end(), [](Point a, Point b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    points.erase(std::unique(points.begin(), points.end(), [](Point a, Point b) {
        return std::abs(a.x - b.x) < 0.001f && std::abs(a.y - b.y) < 0.001f;
    }), points.end());
    if (points.size() < 3) return {};
    std::vector<Point> hull(points.size() * 2);
    std::size_t count = 0;
    for (auto point : points) {
        while (count >= 2 && OutlineTurn(hull[count - 2], hull[count - 1], point) <= 0) --count;
        hull[count++] = point;
    }
    const std::size_t lower = count + 1;
    for (std::size_t i = points.size() - 1; i > 0; --i) {
        const auto point = points[i - 1];
        while (count >= lower && OutlineTurn(hull[count - 2], hull[count - 1], point) <= 0) --count;
        hull[count++] = point;
    }
    hull.resize(count - 1);
    return hull;
}

std::vector<Point> CarBodyHull(WorldCanvas& world, const SpellVisuals::Entity& car) {
    const auto safeAxis = [](Vec3 axis, Vec3 fallback) {
        axis = axis.Normalized();
        return axis.LengthSquared() > 0.5f ? axis : fallback;
    };
    const Vec3 forward = safeAxis(car.forward, {1, 0, 0});
    const Vec3 right = safeAxis(car.right, {0, 1, 0});
    const Vec3 up = safeAxis(car.up, {0, 0, 1});
    const auto extent = [](float value, float fallback) {
        return std::isfinite(value) && value >= 5 && value <= 500 ? value : fallback;
    };
    const Vec3 half{extent(car.halfExtent.x, 60), extent(car.halfExtent.y, 42), extent(car.halfExtent.z, 22)};
    const Vec3 offset = car.localOffset.IsFinite() && car.localOffset.LengthSquared() < 250000 ?
        car.localOffset : Vec3{0, 0, 18};
    const Vec3 center = car.position + forward * offset.x + right * offset.y + up * offset.z;

    // Body, cabin and wheel samples form one outer contour. They never draw
    // internal box edges, cross-bracing, filled ice faces or orbiting particles.
    std::vector<Vec3> samples;
    samples.reserve(64);
    const auto sample = [&](float x, float y, float z) {
        samples.push_back(center + forward * (x * half.x) +
            right * (y * half.y) + up * (z * half.z));
    };
    for (const float side : {-1.0f, 1.0f}) {
        sample(1.02f, side * 0.82f, -0.45f);
        sample(-1.00f, side * 0.85f, -0.45f);
        sample(0.98f, side * 0.94f, 0.14f);
        sample(-0.98f, side * 0.96f, 0.14f);
        sample(0.88f, side * 0.80f, 0.42f);
        sample(-0.92f, side * 0.84f, 0.42f);
        sample(0.32f, side * 0.68f, 1.12f);
        sample(-0.55f, side * 0.72f, 1.12f);
        for (const float wheel : {-0.61f, 0.61f}) {
            for (int i = 0; i < 10; ++i) {
                const float phase = Tau * i / 10;
                sample(wheel + std::cos(phase) * 0.20f, side * 1.02f,
                    -0.60f + std::sin(phase) * 0.80f);
            }
        }
    }
    std::vector<Point> projected;
    projected.reserve(samples.size());
    for (const auto point : samples) {
        Point pixel;
        // When the camera intersects the car, skip the contour rather than
        // closing an artificial outline across the near plane.
        if (!world.Project(point, pixel)) return {};
        projected.push_back(pixel);
    }
    return OutlineHull(std::move(projected));
}

void CarFreezeOutline(WorldCanvas& world, const SpellVisuals::Entity& car, Ink edge, float thickness) {
    const auto hull = CarBodyHull(world, car);
    for (std::size_t i = 0; i < hull.size() && world.budget > 0; ++i) {
        Point a = hull[i], b = hull[(i + 1) % hull.size()];
        if (!ClipLine(a, b, world.width, world.height)) continue;
        --world.budget;
        Line(world.canvas, a, b, edge, thickness);
    }
}

void Freeze(WorldCanvas& world, const SpellVisuals::Effect& effect, const SpellVisuals::Frame& frame, float age) {
    const float duration = static_cast<float>(effect.duration > 0 ? effect.duration : wizard::PetrificusDurationSeconds);
    const float opacity = Ease(age / 0.1f) * (1 - Unit((age - duration) / 0.12f));
    if (opacity <= 0) return;
    const float scale = std::clamp(world.height / 1080.0f, 0.8f, 1.3f);
    SpellVisuals::Entity target;
    target.id = effect.targetId; target.position = effect.end;
    if (const auto* entity = Entity(frame, effect.targetId, effect.targetKey)) target = *entity;
    if (!target.position.IsFinite()) return;
    // A thin body contour is the entire sustained bind. White-blue highlights
    // drift over that same silhouette, without filling or boxing in the car.
    CarFreezeOutline(world, target, Alpha({132, 215, 255, 190}, opacity), 1.35f * scale);
    const auto hull = CarBodyHull(world, target);
    for (std::size_t edge = 0; edge < hull.size() && world.budget > 0; ++edge) {
        const float phase = std::fmod(age * .42f + edge * .173f, 1.0f);
        if (phase > .32f) continue;
        const Point a = hull[edge], b = hull[(edge + 1) % hull.size()];
        Point low = a + (b - a) * (phase / .32f * .7f);
        Point high = a + (b - a) * std::min(1.0f, phase / .32f * .7f + .27f);
        if (!ClipLine(low, high, world.width, world.height)) continue;
        --world.budget;
        Line(world.canvas, low, high, Alpha({225, 251, 255, 245}, opacity), .9f * scale);
    }
}

void MemoryDrift(WorldCanvas& world, const SpellVisuals::Effect& effect, const SpellVisuals::Frame& frame, float age) {
    const float duration=static_cast<float>(effect.duration>0?effect.duration:wizard::ObliviateDurationSeconds);
    const float fade=Ease(age/.16f)*(1-Ease((age-duration+.25f)/.25f));
    const auto* target=Entity(frame,effect.targetId,effect.targetKey);
    if(!target||age<0||age>=duration||fade<=0) return;
    const Vec3 center=target->position+target->up*83;
    // Thin memories peel upwards in short curling ribbons; no cage obscures
    // the car. Each ribbon dissolves at its wrap point rather than popping.
    for(int ribbon=0;ribbon<3;++ribbon) {
        const float life=std::fmod(age*.34f+ribbon/3.0f,1.0f);
        const float breath=std::sin(life*Pi)*fade;
        Vec3 previous;
        for(int part=0;part<=9;++part) {
            const float t=part/9.0f,phase=ribbon*2.1f+life*3.0f+t*2.1f;
            const Vec3 p=center+target->up*(25+life*135+t*35)+
                world.cameraRight*(std::sin(phase)*(48+t*22))+
                target->forward*(std::cos(phase)*30);
            if(part) {
                const float taper=breath*(1-t*.68f);
                world.Segment(previous,p,Alpha({255,209,66,205},taper),2.7f);
                world.Segment(previous,p,Alpha({255,252,210,240},taper),.85f);
            }
            previous=p;
        }
    }
    for(int shard=0;shard<5;++shard) {
        const float life=std::fmod(age*.27f+shard/5.0f,1.0f);
        const float phase=shard*2.4f+age*.65f;
        const Vec3 p=center+target->up*(25+life*165)+
            world.cameraRight*(std::cos(phase)*85)+target->forward*(std::sin(phase)*32);
        const Vec3 a=world.cameraRight*(6+life*3),b=world.cameraUp*(10-life*3);
        const Ink ink=Alpha(shard%2?Ink{255,223,102,235}:Ink{255,251,213,240},fade*std::sin(life*Pi));
        world.Segment(p-a,p+b,ink,1.0f); world.Segment(p+b,p+a,ink,1.0f);
    }
}

void ObliviateLaser(WorldCanvas& world, Vec3 start, Vec3 end, float age) {
    constexpr float travel=.18f;
    if(age<0||age>=.68f) return;
    const float progress=Unit(age/travel),fade=1-Ease((age-.18f)/.5f);
    const Vec3 direction=(end-start).Normalized();
    Vec3 across,up; Plane(end-start,across,up);
    const Vec3 head=Lerp(start,end,progress);
    const float scale=std::clamp(world.height/1080.0f,.65f,1.8f);
    const Ink yellow{255,224,35,255},hot{255,255,208,255};
    world.Glow(head, 40, yellow, fade * .36f);
    for (int point = 0; point <= 24; ++point)
        world.Glow(Lerp(start, head, point / 24.f), 20, yellow, fade * .2f);
    // A straight yellow lance travels only after release, with a white core.
    world.Segment(start,head,Alpha({172,118,9,170},fade),9.5f*scale);
    world.Segment(start,head,Alpha(yellow,fade),4.0f*scale);
    world.Segment(start,head,Alpha(hot,fade),1.15f*scale);
    // A pair of fine golden wakes surrounds the straight hot centre without
    // turning the memory spell into another jagged lightning strike.
    for(int strand=0;strand<2;++strand) {
        Vec3 previous=start;
        for(int part=1;part<=13;++part) {
            const float t=progress*part/13.0f,phase=t*Tau*1.4f-age*7+strand*Pi;
            const float radius=std::sin(t*Pi)*14;
            const Vec3 p=Lerp(start,end,t)+(across*std::cos(phase)+up*std::sin(phase))*radius;
            world.Segment(previous,p,Alpha({255,231,111,235},fade*.74f),.9f*scale);
            previous=p;
        }
    }
    LineSpark(world,head,13+fade*15,Alpha(hot,fade),.32f);
    if(progress>=1) {
        const float impact=age-travel;
        const float bloom=1-Ease(impact/.5f);
        for(int wing=0;wing<2;++wing)
            WorldArc(world,end,world.cameraRight,world.cameraUp,24+Out(impact/.4f)*115,
                wing*Pi+.2f,Pi*.65f,Alpha(yellow,bloom),1.5f*scale,9);
        for(int fragment=0;fragment<6;++fragment) {
            const Vec3 ray=Scatter(fragment,.3f);
            const Vec3 p=end+ray*(26+Out(impact/.44f)*135);
            world.Segment(p,p+ray*(12+bloom*20)+direction*8,Alpha(hot,bloom),1.4f*scale);
            world.Segment(p,p+up*10-across*7,Alpha(yellow,bloom),.85f*scale);
        }
        LineSpark(world,end,18+bloom*16,Alpha(hot,bloom),.2f);
    }
}

void ImperioControl(WorldCanvas& world, const SpellVisuals::Effect& effect,
    const SpellVisuals::Frame& frame, float age) {
    const float duration=static_cast<float>(effect.duration>0?effect.duration:wizard::ImperioDurationSeconds);
    const float fade=Ease(age/.12f)*(1-Ease((age-duration+.2f)/.2f));
    const auto* caster=Entity(frame,effect.casterId,effect.casterKey);
    const auto* target=Entity(frame,effect.targetId,effect.targetKey);
    if(!caster||!target||age<0||age>=duration||fade<=0) return;
    const Vec3 start=caster->position+caster->up*65,end=target->position+target->up*75;
    Vec3 across,up; Plane(end-start,across,up);
    const auto path=[&](float t,float strand) {
        const float envelope=std::sin(t*Pi),phase=t*Tau*1.35f-age*2.3f+strand*Pi;
        return Lerp(start,end,t)+up*(envelope*65)+
            across*(std::sin(phase)*15*envelope);
    };
    for(int strand=0;strand<2;++strand) {
        Vec3 previous=start;
        const Ink ink=Alpha(strand?Ink{185,113,255,230}:Ink{255,216,108,250},fade);
        for(int part=1;part<=24;++part) {
            const float t=part/24.0f;
            const Vec3 point=path(t,static_cast<float>(strand));
            if(!strand) world.Segment(previous,point,Alpha({100,55,154,175},fade),4.1f);
            world.Segment(previous,point,ink,strand?1.0f:1.8f); previous=point;
        }
    }
    // Smooth light packets establish caster-to-victim direction without
    // changing the path topology or snapping the whole tether each frame.
    for(int joint=0;joint<3;++joint) {
        const float t=std::fmod(age*.54f+joint/3.0f,1.0f);
        const float pulse=std::sin(t*Pi)*fade;
        const Vec3 p=path(t,0),tail=path(std::max(0.0f,t-.035f),0);
        world.Segment(tail,p,Alpha({255,249,211,255},pulse),3.4f);
        LineSpark(world,p,5.5f,Alpha({255,247,209,245},pulse),.25f);
    }
    const Vec3 bar=end+target->up*53;
    world.Segment(bar-target->right*31,bar+target->right*31,Alpha({255,224,137,245},fade),1.5f);
    world.Segment(bar-target->forward*18,bar+target->forward*18,Alpha({255,244,199,240},fade),1.1f);
    for(float side:{-1.0f,1.0f}) {
        world.Segment(bar+target->right*(side*26),end+target->right*(side*22),Alpha({192,133,255,235},fade),1.05f);
        const Vec3 endPin=end+target->right*(side*22);
        world.Segment(endPin-target->forward*7,endPin+target->forward*7,Alpha({255,231,165,230},fade),1.2f);
    }
}

void BombardaRicochetGlow(WorldCanvas& world, const SpellVisuals::Effect& effect,
    const SpellVisuals::Frame& frame, float age) {
    const float duration = static_cast<float>(effect.duration > 0 ? effect.duration : wizard::BombardaRicochetDurationSeconds);
    if (!frame.ballValid || age < 0 || age >= duration) return;
    const float fade = Ease(age / .08f) * std::min(1.0f, (duration - age) / .25f);
    const float radius = std::clamp(frame.ballRadius, 10.0f, 1000.0f);
    const Ink amber = Alpha({255, 143, 37, 245}, fade), hot = Alpha({255, 244, 191, 255}, fade);
    world.Glow(frame.ball, radius * 1.8f, amber, .13f, true);
    world.Glow(frame.ball, radius * 1.2f, hot, .12f);
    // Open tongues of fire wrap the ball instead of a rigid starburst. Every
    // point is an analytic function of the recorded clock, including on seek.
    for (int band = 0; band < 3; ++band) {
        const float turn = age * (band == 1 ? -.85f : .65f) + band * 2.1f;
        const Vec3 axis = band == 0 ? Vec3{0, 0, 1} :
            band == 1 ? Vec3{.82f, .21f, .53f}.Normalized() : Vec3{-.3f, .86f, .41f}.Normalized();
        Vec3 across, up; Plane(axis, across, up);
        const float orbit = radius + 6 + band * 3.5f;
        WorldArc(world, frame.ball, across, up, orbit, turn, Pi * 1.38f, Alpha(amber, .18f), 7.2f, 14);
        WorldArc(world, frame.ball, across, up, orbit, turn, Pi * 1.38f, Alpha(amber, .85f), 2.8f, 14);
        WorldArc(world, frame.ball, across, up, orbit + 1.5f, turn + Pi * .95f, Pi * .32f,
            Alpha(hot, .95f), 1.7f, 5);
    }
    for (int ember = 0; ember < 10; ++ember) {
        const float phase = age * 1.5f + ember * 2.3999632f;
        const Vec3 ray = Scatter(ember, age * .38f);
        const Vec3 curl = Cross(ray, {0, 0, 1}).Normalized();
        const float lift = 20 + 13 * std::sin(phase);
        Vec3 previous = frame.ball + ray * (radius + 4);
        for (int step = 1; step <= 3; ++step) {
            const float t = step / 3.0f;
            const Vec3 point = frame.ball + ray * (radius + 5 + t * lift) +
                curl * (std::sin(t * Pi) * 10) + Vec3{0, 0, t * t * 22};
            world.Segment(previous, point, Alpha(step == 1 ? hot : amber, 1 - t * .65f),
                step == 1 ? 1.9f : 1.1f);
            previous = point;
        }
    }
}

void Levitate(WorldCanvas& world, const SpellVisuals::Effect& effect, const SpellVisuals::Frame& frame, float age) {
    const float duration = static_cast<float>(effect.duration > 0 ? effect.duration : wizard::LeviosaSafetySeconds);
    const float opacity = Ease(age / .14f) * (1 - Unit((age - duration + .22f) / .35f));
    if (opacity <= 0) return;
    const Vec3 center = Position(frame, effect.targetId, effect.end, effect.targetKey);
    const float onset = 1 - Unit(age / .8f);
    // Open, tapering ribbons read as an updraft rather than a stack of hoops.
    // Their origin is the captured car pose, so roof ascent and replay motion
    // carry the wisps with the body without a second simulated trajectory.
    for (int strand = 0; strand < 3; ++strand) {
        Vec3 previous;
        for (int i = 0; i <= 24; ++i) {
            const float t = i / 24.0f;
            const float angle = strand * Tau / 3 + t * Tau * .82f - age * 1.9f;
            const float radius = 98 + std::sin(t * Pi) * (54 + onset * 30);
            Vec3 point = center + Vec3{std::cos(angle) * radius, std::sin(angle) * radius,
                -125 + t * (315 + onset * 45)};
            point.z = std::max(4.0f, point.z);
            if (i) {
                const float strength = opacity * std::pow(std::max(0.0f, std::sin(t * Pi)), .65f);
                if (i % 3 == 0) world.Glow(point, 34, {179, 132, 255, 255}, strength * .13f, true);
                world.Segment(previous, point, Alpha({159, 105, 239, 180}, strength), 3.0f + onset);
                world.Segment(previous, point, Alpha(strand == 1 ? Ink{242, 222, 255, 245} :
                    Ink{205, 163, 255, 235}, strength), 1.05f + onset * .3f);
            }
            previous = point;
        }
    }
    // Feather-shaped flecks rise continuously and fade at both ends of each
    // cycle. Fixed phase offsets make seeking and different FPS agree.
    for (int feather = 0; feather < 6; ++feather) {
        const float phase = std::fmod(age * .52f + feather / 6.0f, 1.0f);
        const float angle = feather * 2.39996f - age * .5f;
        const Vec3 p = center + Vec3{std::cos(angle) * 148, std::sin(angle) * 148, -95 + phase * 360};
        const float length = 11 + onset * 8;
        const Ink ink = Alpha({239, 221, 255, 240}, opacity * std::sin(phase * Pi));
        world.Segment(p - Vec3{0, 0, length}, p + Vec3{0, 0, length}, ink, 1.3f);
        world.Segment(p - Vec3{0, 0, length * .15f}, p + world.cameraRight * length * .55f + Vec3{0, 0, length * .6f}, ink, 1);
        world.Segment(p + Vec3{0, 0, length * .2f}, p - world.cameraRight * length * .45f + Vec3{0, 0, length * .85f}, ink, 1);
    }
}

void WardRune(WorldCanvas& world, Vec3 center, Vec3 right, Vec3 up, float size, int rune, Ink ink) {
    const auto at = [&](float x, float y) { return center + (right * x + up * y) * size; };
    const auto stroke = [&](float x1, float y1, float x2, float y2) {
        world.Segment(at(x1, y1), at(x2, y2), ink, 1.25f);
    };
    // Varied open calligraphic marks leave clear gaps between strokes, even
    // when a Canvas implementation renders line alpha as fully opaque.
    switch (rune % 6) {
    case 0:
        stroke(0, -1, 0, 1); stroke(-0.55f, 0.45f, 0, 1);
        stroke(0, 1, 0.55f, 0.45f); stroke(0, -0.35f, 0.55f, -0.8f); break;
    case 1:
        stroke(0, 1, 0.6f, 0); stroke(0.6f, 0, 0, -0.7f);
        stroke(0, -0.7f, -0.6f, 0); stroke(-0.6f, 0, 0, 1);
        stroke(0, -0.7f, 0, -1.15f); stroke(-.24f, -.98f, .24f, -.98f); break;
    case 2:
        stroke(-0.55f, 0.8f, 0.45f, 0.8f); stroke(0.45f, 0.8f, -0.4f, -0.65f);
        stroke(-0.4f, -0.65f, 0.55f, -0.65f); stroke(0, 0.1f, 0.55f, 0.1f); break;
    case 3:
        stroke(0, -1, 0, 1); stroke(-0.6f, 0.7f, 0, 0.15f);
        stroke(0, 0.15f, 0.6f, 0.7f); stroke(-0.6f, -0.65f, 0, -0.1f);
        stroke(0, -0.1f, 0.6f, -0.65f); stroke(-.18f, -1.12f, .18f, -1.12f); break;
    case 4:
        stroke(-0.45f, -1, -0.45f, 1); stroke(0.45f, -1, 0.45f, 1);
        stroke(-0.45f, 0, 0.45f, 0); stroke(-0.45f, 0.7f, 0, 1.1f);
        stroke(0, 1.1f, 0.45f, 0.7f); break;
    default:
        stroke(-0.6f, -0.8f, 0, 0.1f); stroke(0, 0.1f, 0.6f, -0.8f);
        stroke(-0.6f, 0.9f, 0, 0.1f); stroke(0, 0.1f, 0.6f, 0.9f);
        stroke(-.24f, 1.12f, .24f, 1.12f); break;
    }
}




void StupefyFlash(WorldCanvas& world, Vec3 start, Vec3 impact, float age) {
    if (age < 0 || age >= 0.85f) return;
    const float fade = 1 - Ease(age / 0.85f);
    const float hot = 1 - Ease(age / 0.18f);
    const float beam = 1 - Ease((age - 0.06f) / 0.29f);
    const float scale = std::clamp(world.height / 1080.0f, 0.65f, 1.8f);
    Vec3 across, up; Plane(impact - start, across, up);
    const Vec3 direction = (impact - start).Normalized();
    const Ink scarlet{255, 22, 47, 255}, white{255, 237, 226, 255};
    world.Glow(impact, 105 + hot * 75, scarlet, hot * .42f, true);
    world.Glow(impact, 45, white, hot * .5f);
    for (int point = 0; point <= 24 && beam > 0; ++point)
        world.Glow(Lerp(start, impact, point / 24.f), 23, scarlet, beam * .18f);

    // The hit is immediate: a white-hot red star and full-length lightning
    // are visible on the very first frame, with no projectile travel delay.
    if (hot > 0) {
        const float radius = 35 + hot * 108;
        for (int ray = 0; ray < 8; ++ray) {
            const float angle = Tau * ray / 8 + 0.15f;
            const Vec3 d = world.cameraRight * std::cos(angle) + world.cameraUp * std::sin(angle);
            world.Segment(impact - d * radius * 0.12f, impact + d * radius,
                Alpha(scarlet, hot), 6.8f * hot * scale + 1.1f);
            world.Segment(impact, impact + d * radius * 0.78f,
                Alpha(white, hot), 2.2f * hot * scale + 0.6f);
        }
        LineSpark(world, start, 34 + hot * 55, Alpha({255, 72, 94, 255}, hot), age * 3);
        LineSpark(world, start, 20 + hot * 24, Alpha(white, hot), age * 3);
    }
    if (beam > 0) {
        constexpr int steps = 20;
        const float spread = std::clamp((impact - start).Length() * 0.010f, 18.0f, 55.0f);
        const float flow = age * 12.0f;
        std::array<Vec3, steps + 1> path;
        path[0] = start;
        for (int i = 1; i <= steps; ++i) {
            const float t = i / static_cast<float>(steps), envelope = std::sin(t * Pi);
            path[i] = Lerp(start, impact, t) + across * (std::sin(i * 17.1f + flow * 1.4f) * spread * envelope) +
                up * (std::sin(i * 31.7f - flow * 1.1f) * spread * envelope * 0.6f);
        }
        // Draw all of the broad red filament before its core and optional forks.
        for (int layer = 0; layer < 3; ++layer)
            for (int i = 1; i <= steps; ++i)
                world.Segment(path[i - 1], path[i], Alpha(layer == 2 ? white :
                    layer == 1 ? scarlet : Ink{139, 9, 30, 165}, beam),
                    (layer == 2 ? 1.35f : layer == 1 ? 4.5f : 10.0f) * scale * beam + 0.5f);
        for (int fork = 0; fork < 4; ++fork) {
            Vec3 a = path[4 + fork * 4];
            const float side = fork % 2 ? 1.0f : -1.0f;
            for (int part = 0; part < 3; ++part) {
                const Vec3 b = a + across * (side * (26 + part * 13)) + direction * 30 +
                    up * (std::sin(flow + fork * 1.7f + part * 7.3f) * 31);
                world.Segment(a, b, Alpha({255, 92, 114, 255}, beam), (2.2f - part * 0.6f) * scale);
                a = b;
            }
        }
    }
    // Two opposed pressure crescents and tapered red ejecta make a sharp,
    // directional stun impact rather than a stack of rotating target circles.
    for (int wing = 0; wing < 2; ++wing) {
        const float wave = age - wing * 0.04f;
        if (wave < 0) continue;
        const float radius = 24 + Out(wave / 0.58f) * (220 + wing * 38.0f);
        WorldArc(world,impact+direction*(wave*110),across,up,radius,
            Pi*wing+.28f,Pi*.72f,
            Alpha(wing?Ink{255,144,154,245}:scarlet,fade),wing?1.4f:2.6f,11);
    }
    for (int spark = 0; spark < 12; ++spark) {
        const Vec3 d = Scatter(spark, 0.35f);
        const Vec3 a = impact + d * (25 + Out(age/.7f)*270)+direction*(age*135);
        const Vec3 b = a + d * (22 + hot * 38);
        world.Segment(a,b,Alpha(scarlet,fade),spark%3?1.8f:3.0f);
        world.Segment(b,b+d*10,Alpha(white,fade),.9f);
    }
}

void Shield(WorldCanvas& world, const SpellVisuals::Wall& wall, double time) {
    if (!wall.center.IsFinite() || !wall.normal.IsFinite() || !std::isfinite(wall.radius) ||
        wall.radius <= 0 || wall.ends <= wall.began) return;
    const float age = static_cast<float>(time - wall.began);
    const float remaining = static_cast<float>(wall.ends - time);
    if (age < 0 || remaining <= 0) return;
    const float appear = Out(age / 0.25f), opacity = appear * std::min(1.0f, remaining / 0.4f);
    if (opacity <= 0) return;
    // A physical ward always keeps its outline and contact response, while
    // eight simultaneous shields cannot spend the whole frame on filigree.
    VisualBudget wardBudget(world, 194);
    Vec3 right, up; Plane(wall.normal, right, up);
    const float radius = wall.radius * appear;
    const auto at = [&](float x, float y) { return wall.center + right * x + up * y; };
    const auto radial = [&](float distance, float angle) {
        return at(distance * std::cos(angle), distance * std::sin(angle));
    };
    const auto arc = [&](float distance, float from, float sweep, Ink ink, float thickness, int steps) {
        for (int i = 0; i < steps; ++i)
            world.Segment(radial(distance, from + sweep * i / steps),
                radial(distance, from + sweep * (i + 1) / steps), ink, thickness);
    };

    // Every element is a stroke; all decorative geometry stays outside the
    // central 65 percent of the disk. No translucent fill is required.
    world.Circle(wall.center, right, up, radius, Alpha({175, 255, 231, 235}, opacity), 2.1f, 0, 48);
    const float impactAge = wall.impact >= wall.began ? static_cast<float>(time - wall.impact) : 10;
    if (impactAge >= 0 && impactAge < 0.7f) {
        const float travel = Out(impactAge / .7f), flash = opacity * (1 - impactAge / .7f);
        for (int sector = 0; sector < 4; ++sector) {
            const float angle = Tau * sector / 4;
            arc(radius * (.18f + travel * .78f), angle, Pi * .36f,
                Alpha({228, 255, 235, 245}, flash), 2.2f, 5);
            const Vec3 head = radial(radius * (.72f + travel * .3f), angle + .25f);
            const Vec3 ray = (head - wall.center).Normalized();
            world.Segment(head - ray * 17, head + ray * (12 + travel * 25),
                Alpha({255, 233, 167, 230}, flash), 1.4f);
        }
    }
    for (int i = 0; i < 8; ++i) {
        const float angle = Tau * i / 8 + age * .045f;
        const Vec3 runeUp = right * std::cos(angle) + up * std::sin(angle);
        const Vec3 runeRight = right * -std::sin(angle) + up * std::cos(angle);
        const float shimmer = .82f + .18f * std::sin(age * 2.1f - i * .67f);
        WardRune(world, radial(radius * .83f, angle), runeRight, runeUp, 18 * appear,
            i, Alpha(i % 3 ? Ink{183, 255, 227, 235} : Ink{255, 232, 162, 235}, opacity * shimmer));
        world.Segment(radial(radius * .956f, angle), radial(radius * 1.018f, angle),
            Alpha({255, 231, 168, 220}, opacity), 1.2f);
    }
    for (int sector = 0; sector < 4; ++sector) {
        const float angle = Tau * sector / 4 - age * .075f;
        arc(radius * .69f, angle, Pi * .31f, Alpha({98, 225, 223, 175}, opacity), 1.0f, 6);
    }
    // Slow travelling highlights make a luminous rim instead of a flat grid.
    for (int light = 0; light < 3; ++light) {
        const float angle = age * .52f + Tau * light / 3;
        arc(radius * 1.012f, angle, Pi * .13f, Alpha({234, 255, 225, 245}, opacity), 2.6f, 5);
        arc(radius * 1.038f, angle - .04f, Pi * .18f, Alpha({94, 228, 213, 110}, opacity), 1.1f, 5);
        const Vec3 spark = radial(radius * 1.012f, angle + Pi * .13f);
        const float size = 4 * appear;
        world.Segment(spark - right * size, spark + right * size, Alpha({255, 248, 199, 240}, opacity), 1);
        world.Segment(spark - up * size, spark + up * size, Alpha({255, 248, 199, 240}, opacity), 1);
    }
}

void AvadaTargetGlow(WorldCanvas& world, const SpellVisuals::Effect& effect,
    const SpellVisuals::Frame& frame, float progress) {
    const auto* target=Entity(frame,effect.targetId,effect.targetKey);
    if (!target) return; // Never move a warning to a respawn or same-ID neighbor.
    const auto hull=CarBodyHull(world,*target);
    if(hull.size()<3) return;
    progress=Unit(progress);
    const float scale=std::clamp(world.height/1080.0f,.7f,1.6f);
    const auto edge=[&](Ink ink,float width) {
        for(std::size_t i=0;i<hull.size()&&world.budget>0;++i) {
            Point a=hull[i],b=hull[(i+1)%hull.size()];
            if(!ClipLine(a,b,world.width,world.height)) continue;
            --world.budget;Line(world.canvas,a,b,ink,width*scale);
        }
    };
    if (world.HasGlow()) {
        const auto axis=[](Vec3 v,Vec3 fallback) { v=v.Normalized();return v.LengthSquared()>.5f?v:fallback; };
        const Vec3 forward=axis(target->forward,{1,0,0}), right=axis(target->right,{0,1,0}), up=axis(target->up,{0,0,1});
        const auto extent=[](float value,float fallback) {return std::isfinite(value)&&value>=5&&value<=500?value:fallback;};
        const Vec3 half{extent(target->halfExtent.x,60),extent(target->halfExtent.y,42),extent(target->halfExtent.z,22)};
        const Vec3 offset=target->localOffset.IsFinite()&&target->localOffset.LengthSquared()<250000?target->localOffset:Vec3{0,0,18};
        const Vec3 center=target->position+forward*offset.x+right*offset.y+up*offset.z;
        const Ink green{27,255,63,255};
        // Stationary, pose-fitted body light only: no orbiting particles or
        // wind-up cloud. The actual painted car remains visible underneath.
        for (float x : {-.65f,0.f,.65f}) for (float y : {-.55f,0.f,.55f}) {
            const Vec3 p=center+forward*(x*half.x)+right*(y*half.y)+up*(half.z*(x==0?.75f:.25f));
            const float radius=std::clamp(half.y*.92f,16.f,75.f);
            world.Glow(p,radius,green,.22f+.13f*progress);
            world.Glow(p,radius*.48f,{130,255,152,255},.1f+.16f*progress);
        }
        for (float x : {-.61f,.61f}) for(float side : {-1.f,1.f})
            world.Glow(center+forward*(x*half.x)+right*(side*half.y)+up*(-half.z*.6f),
                std::clamp(half.z*.85f,12.f,35.f),green,.2f+.1f*progress);
        edge(Alpha({85,255,117,255},.58f+.3f*progress),1.15f);
        return;
    }
    // A tight layered halo and one continuous body tint replace the old
    // particle cloud. This is a pose-fitted Canvas glow, not an engine material
    // override; the installed SDK exposes no LAN/replay emissive setter.
    const float bloom=.88f+.12f*Ease(progress);
    edge(Alpha({5,105,28,65},bloom),10);
    edge(Alpha({20,218,58,132},bloom),5.5f);
    auto body=hull;
    for(int boundary=0;boundary<4&&!body.empty();++boundary) {
        std::vector<Point> clipped;
        const auto distance=[&](Point p) {
            return boundary==0?p.x:boundary==1?world.width-p.x:boundary==2?p.y:world.height-p.y;
        };
        for(std::size_t i=0;i<body.size();++i) {
            const Point a=body[i],b=body[(i+1)%body.size()];
            const float da=distance(a),db=distance(b);
            if(da>=0) clipped.push_back(a);
            if((da>=0)!=(db>=0)) clipped.push_back(a+(b-a)*(da/(da-db)));
        }
        body=std::move(clipped);
    }
    const int triangles=static_cast<int>(body.size())-2;
    if(triangles>0&&triangles<=world.budget) {
        world.budget-=triangles;
        Polygon(world.canvas,body,{31,static_cast<int>(247+8*progress),65,
            static_cast<int>(202+43*progress)});
    }
    // Broad tonal surfaces retain a recognizable hood, cabin and wheels when
    // a Canvas backend treats the body tint as opaque. There are no moving
    // flecks or decorative lines outside the car.
    const auto axis=[](Vec3 value,Vec3 fallback) {
        value=value.Normalized();return value.LengthSquared()>.5f?value:fallback;
    };
    const Vec3 forward=axis(target->forward,{1,0,0}),right=axis(target->right,{0,1,0}),up=axis(target->up,{0,0,1});
    const auto extent=[](float value,float fallback){return std::isfinite(value)&&value>=5&&value<=500?value:fallback;};
    const Vec3 half{extent(target->halfExtent.x,60),extent(target->halfExtent.y,42),extent(target->halfExtent.z,22)};
    const Vec3 offset=target->localOffset.IsFinite()&&target->localOffset.LengthSquared()<250000?
        target->localOffset:Vec3{0,0,18};
    const Vec3 center=target->position+forward*offset.x+right*offset.y+up*offset.z;
    const auto at=[&](float x,float y,float z){return center+forward*(x*half.x)+right*(y*half.y)+up*(z*half.z);};
    const Vec3 view=world.camera-center;
    const auto face=[&](Vec3 a,Vec3 b,Vec3 c,Vec3 d,Vec3 normal,Ink ink) {
        if(view.Dot(normal)<=0) return;
        world.Facet(a,b,c,ink);world.Facet(a,c,d,ink);
    };
    face(at(.88f,-.8f,.42f),at(.88f,.8f,.42f),at(.63f,.75f,.42f),at(.63f,-.75f,.42f),up,{127,255,143,225});
    face(at(.63f,-.75f,.42f),at(.63f,.75f,.42f),at(.32f,.68f,1.12f),at(.32f,-.68f,1.12f),up+forward,{70,234,100,224});
    face(at(.32f,-.68f,1.12f),at(.32f,.68f,1.12f),at(-.55f,.72f,1.12f),at(-.55f,-.72f,1.12f),up,{183,255,180,242});
    face(at(-.55f,-.72f,1.12f),at(-.55f,.72f,1.12f),at(-.92f,.84f,.42f),at(-.92f,-.84f,.42f),up-forward,{61,237,90,225});
    for(const float side:{-1.0f,1.0f}) {
        face(at(.98f,side*.94f,.14f),at(-.98f,side*.96f,.14f),at(-1,side*.85f,-.45f),at(1.02f,side*.82f,-.45f),right*side,{20,213,54,235});
        if(view.Dot(right*side)<=0) continue;
        for(const float wheel:{-.61f,.61f}) {
            const Vec3 hub=at(wheel,side*1.02f,-.6f);
            for(int i=0;i<10;++i) {
                const float a=Tau*i/10,b=Tau*(i+1)/10;
                const auto rim=[&](float phase){return hub+forward*(std::cos(phase)*half.x*.2f)+up*(std::sin(phase)*half.z*.8f);};
                world.Facet(hub,rim(a),rim(b),{47,235,71,235});
            }
        }
    }
    edge({205,255,179,245},1.15f);
}

void ChargeSpell(WorldCanvas& world, const SpellVisuals::Effect& effect,
    const SpellVisuals::Frame& frame, float age) {
    const float full=static_cast<float>(wizard::SpellChargeSeconds(effect.spell));
    const float duration=std::clamp(static_cast<float>(effect.duration),0.0f,full);
    if(age<0 || age>=duration || full<=0) return;
    const float progress=1-(duration-age)/full;
    if(effect.spell==Spell::AvadaKedavra) {
        AvadaTargetGlow(world,effect,frame,progress);
        return; // Avada's wind-up is only the target's clean luminous body glow.
    }
    const bool lift=effect.spell==Spell::Leviosa, stun=effect.spell==Spell::Stupefy;
    const bool bind=effect.spell==Spell::PetrificusTotalus, ward=effect.spell==Spell::Protego;
    const bool blast=effect.spell==Spell::BombardaMaxima, patronum=effect.spell==Spell::ExpectoPatronum;
    const bool memory=effect.spell==Spell::Obliviate, slime=effect.spell==Spell::EatSlugs;
    const bool summon=effect.spell==Spell::Accio, control=effect.spell==Spell::Imperio;
    const bool ballTarget=wizard::IsBallTargetSpell(effect.spell);
    const Ink accent=lift?Ink{194,116,255,255}:stun?Ink{255,47,75,255}:
        bind?Ink{156,181,255,255}:ward?Ink{125,245,218,255}:blast?Ink{255,143,53,255}:
        patronum?Ink{139,235,255,255}:memory?Ink{255,222,48,255}:slime?Ink{91,141,43,255}:
        summon?Ink{85,195,255,255}:control?Ink{188,145,255,255}:Ink{40,255,76,255};
    const Ink pale=lift?Ink{248,223,255,255}:stun?Ink{255,229,223,255}:
        bind?Ink{222,247,255,255}:ward?Ink{238,255,213,255}:blast?Ink{255,246,213,255}:
        patronum?Ink{238,255,255,255}:memory?Ink{255,255,204,255}:slime?Ink{177,196,95,255}:
        summon?Ink{221,248,255,255}:control?Ink{246,224,255,255}:Ink{217,255,195,255};
    const Vec3 caster=Position(frame,effect.casterId,effect.start,effect.casterKey);
    const Vec3 target=ballTarget?(frame.ballValid?frame.ball:effect.end):
        Position(frame,effect.targetId,effect.end,effect.targetKey)+Vec3{0,0,45};
    Vec3 direction=(target-caster).Normalized();
    if(direction.LengthSquared()<.5f) direction={1,0,0};
    const Vec3 source=caster+direction*85+Vec3{0,0,65};
    Vec3 across,up;Plane(direction,across,up);
    const float clock=static_cast<float>(frame.time), intensity=.35f+.65f*Ease(progress);
    world.Glow(source, 38 + progress * 18, accent, intensity * .17f, true);
    world.Glow(source, 12 + progress * 6, pale, intensity * .32f);
    // Each spell gathers at the caster's nose. Nothing bridges to the victim
    // or creates a target circle before the accepted gameplay release.
    const float spread=lift?42.f:bind?26.f:blast?34.f:patronum?47.f:slime?28.f:20.f;
    const float reach=patronum?155.f:lift?130.f:blast?105.f:slime?82.f:95.f;
    // A few tapered curves provide shape instead of a sparkling noisy cloud.
    // Draw the focal energy first so crowded-scene slices retain the cue.
    world.Segment(source-direction*11,source+direction*(12+14*progress),Alpha(accent,intensity*.38f),6.5f);
    world.Segment(source-direction*7,source+direction*(10+14*progress),Alpha(pale,intensity),1.6f);
    for(int strand=0;strand<3;++strand) {
        Vec3 previous;
        for(int step=0;step<=9;++step) {
            const float t=step/9.f, taper=(1-t)*(1-t);
            const float side=std::sin(strand*2.17f+.6f);
            const float wave=std::sin(t*Pi+clock*(lift?1.6f:patronum?1.2f:2.1f)+strand);
            const float rise=lift?(1-t)*-65.f:blast?(1-t)*-20.f:patronum?wave*16.f:0;
            const float crystal=bind?std::abs(std::sin(t*Pi*3+strand))*12.f:0;
            const Vec3 point=source-direction*(reach*(1-t))+across*(side*spread*taper+wave*7*taper)+
                up*((std::cos(strand*2.17f)*spread+crystal)*taper+rise);
            if(step) {
                const float strength=intensity*(.18f+.82f*t);
                world.Segment(previous,point,Alpha(accent,strength*.34f),3.3f-1.4f*t);
                world.Segment(previous,point,Alpha(strand==1?pale:accent,strength),.9f+1.2f*t);
            }
            previous=point;
        }
    }
    for(int mote=0;mote<10;++mote) {
        const float phase=std::fmod(std::max(0.f,clock)*(blast?1.45f:slime?.65f:.8f)+mote*.381966f,1.f);
        const float window=std::sin(Pi*phase);
        const Vec3 drift=across*(std::sin(mote*2.7f)*spread)+up*(std::cos(mote*1.91f)*spread);
        const Vec3 point=source-direction*(reach*(1-phase))+drift*(1-phase);
        const Vec3 stroke=direction*(8+progress*14)+(lift?up*14:Vec3{});
        world.Segment(point,point+stroke,Alpha(accent,intensity*window*window),slime?2.8f:1.6f);
        world.Segment(point+stroke*.55f,point+stroke,Alpha(pale,intensity*window),.8f);
        if(bind&&mote%3==0) world.Segment(point+stroke*.5f-across*4,point+stroke*.5f+across*4,
            Alpha(pale,intensity*window),.8f);
    }
    LineSpark(world,source,6+progress*7,Alpha(pale,intensity),.3f+clock*.35f);
}

bool VisiblePatronum(const SpellVisuals::Patronum& ghost, double time) {
    return ghost.position.IsFinite() && ghost.forward.IsFinite() &&
        std::isfinite(ghost.began) && std::isfinite(ghost.ends) && std::isfinite(ghost.impact) &&
        ghost.ends > ghost.began && time >= ghost.began && time < ghost.ends &&
        (ghost.impact < ghost.began || ghost.contact.IsFinite());
}

template <std::size_t VertexCount, std::size_t FaceCount>
bool PatronumShell(WorldCanvas& world, Vec3 origin, Vec3 forward, Vec3 right, Vec3 up,
    float size, float fade, const std::array<wizard::patronum_shell::Vertex, VertexCount>& vertices,
    const std::array<wizard::patronum_shell::Face, FaceCount>& faces, int reserve) {
    struct VisibleFace { std::size_t index; float depth, light, edge; };
    std::array<Vec3, VertexCount> points{};
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const auto& point = vertices[index];
        points[index] = origin + (forward * point.x + right * point.y + up * point.z) * size;
    }
    std::array<VisibleFace, FaceCount> visible{};
    std::size_t count = 0;
    const Vec3 sun = (Vec3{-.35f, -.45f, 1}).Normalized();
    for (std::size_t index = 0; index < faces.size(); ++index) {
        const auto& face = faces[index];
        const Vec3 center = (points[face.a] + points[face.b] + points[face.c]) / 3;
        const Vec3 normal = forward * face.nx + right * face.ny + up * face.nz;
        const float facing = normal.Dot((world.camera - center).Normalized());
        if (facing <= .005f) continue;
        visible[count++] = {index, (center - world.camera).Dot(world.direction),
            std::clamp(.46f + .54f * std::max(0.f, normal.Dot(sun)), 0.f, 1.f), 1 - facing};
    }
    // Keep the complete chosen shell. Drawing a truncated prefix opens holes
    // through the body and makes a crowded summon look ghostly again.
    if (!count || static_cast<int>(count) + reserve > world.budget) return false;
    std::sort(visible.begin(), visible.begin() + count,
        [](const VisibleFace& a, const VisibleFace& b) { return a.depth > b.depth; });
    for (std::size_t index = 0; index < count; ++index) {
        const auto& visibleFace = visible[index];
        const auto& face = faces[visibleFace.index];
        const bool glass = face.part == wizard::patronum_shell::Part::Glass;
        const bool wheel = face.part == wizard::patronum_shell::Part::Wheel;
        const float light = visibleFace.light, edge = visibleFace.edge;
        const Ink color = glass ? Ink{24, 63, 91, 247} : wheel ?
            Ink{static_cast<int>(22 + light * 22), static_cast<int>(61 + light * 42),
                static_cast<int>(88 + light * 46), 249} :
            Ink{static_cast<int>(72 + light * 99 + edge * 12),
                static_cast<int>(140 + light * 80), static_cast<int>(181 + light * 59), 249};
        world.Facet(points[face.a], points[face.b], points[face.c], Alpha(color, fade));
    }
    return true;
}

void PatronumGhost(WorldCanvas& world, const SpellVisuals::Patronum& ghost, double time) {
    if (!VisiblePatronum(ghost, time)) return;
    const float age = static_cast<float>(time - ghost.began);
    const bool struck = ghost.impact >= ghost.began;
    const float impactAge = struck ? static_cast<float>(time - ghost.impact) : -1;
    const float fade = struck ? 1 - Ease(impactAge / .45f) :
        std::min(1.0f, static_cast<float>((ghost.ends - time) / .15));
    if (fade <= 0) return;
    Vec3 forward = ghost.forward.Normalized();
    if (forward.LengthSquared() < .5f) forward = {1, 0, 0};
    Vec3 right, up; Plane(forward, right, up);
    const float appear = .22f + .78f * Out(age / .12f);
    const float size = 75 * appear * (struck ? 1 + impactAge * .35f : 1);
    const Vec3 origin = ghost.position - up * (size * .42f);
    // Model-profile faces provide a substantial silver-blue body; the softer
    // additive light is now a surface sheen rather than the entire car.
    bool solid = PatronumShell(world, origin, forward, right, up, size, fade,
        wizard::patronum_shell::DetailedVertices, wizard::patronum_shell::DetailedFaces, 100);
    if (!solid) solid = PatronumShell(world, origin, forward, right, up, size, fade,
        wizard::patronum_shell::CoarseVertices, wizard::patronum_shell::CoarseFaces, 24);
    if (world.HasGlow()) {
        // Surface samples come from the actual supplied model. Overlapping
        // soft additive images reconstruct its luminous body; a sparse set of
        // specular contours supplies wheel/cabin definition without drawing
        // the previous wireframe over the glow.
        const int available = world.budget;
        const int contours = available >= 160 ? 40 : available >= 80 ? 24 : available >= 40 ? 12 : 0;
        const int wakes = available >= 120 ? 8 : available >= 50 ? 4 : available >= 24 ? 2 : 0;
        const int haze = available >= 80 ? 1 : 0;
        const int contact = struck && impactAge >= 0 ? (available >= 100 ? 12 : available >= 40 ? 5 : 0) : 0;
        const int cloudBudget = std::max(0, available - contours - wakes - haze - contact);
        const int samples = std::min(static_cast<int>(wizard::patronum_volume::Samples.size()), cloudBudget);
        const int cores = std::min(samples, std::max(0, cloudBudget - samples));
        for (int index = 0; index < samples; ++index) {
            const auto& sample = wizard::patronum_volume::Samples[index];
            const Vec3 point = origin + (forward * sample.x + right * sample.y + up * sample.z) * size;
            const Vec3 normal = forward * sample.nx + right * sample.ny + up * sample.nz;
            const Vec3 eye = (world.camera - point).Normalized();
            const float facing = std::max(0.0f, normal.Dot(eye));
            const float edge = 1 - std::abs(normal.Dot(eye));
            const bool glass = sample.part == wizard::patronum_volume::Part::Glass;
            const bool wheel = sample.part != wizard::patronum_volume::Part::Body && !glass;
            const float shimmer = .91f + .09f * std::sin(age * 2.1f - sample.x * 3.7f + sample.z * 2.0f);
            const float light = fade * shimmer * (.04f + .96f * std::sqrt(facing)) * (.78f + .22f * edge);
            const float radius = sample.radius * size;
            world.Glow(point, radius * (glass ? 1.05f : 1.18f),
                glass ? Ink{106, 209, 255, 255} : Ink{151, 229, 255, 255},
                light * (glass ? .006f : wheel ? .045f : .055f));
            if (index < cores)
                world.Glow(point, radius * .83f, {231, 252, 255, 255},
                    light * (glass ? .004f : wheel ? .022f : .026f));
        }
        // A restrained edge haze keeps the magic without washing out the
        // solid body's shading, darker windows or separate wheel shapes.
        for (int puff = 0; puff < haze; ++puff) {
            const float offset = haze > 1 ? -.4f + puff * .4f : 0;
            const Vec3 p = origin + forward * (offset * size) + up * (size * .36f);
            world.Glow(p, size * (haze > 1 ? .66f : .85f), {121, 211, 255, 255},
                fade * .008f, true, 1.1f);
        }
        const float stroke = std::clamp(world.height / 1080.0f, .7f, 1.8f);
        for (int accent = 0; accent < contours; ++accent) {
            // Small highlights distributed across all four treads, then
            // sample the cabin/body features. These are accents, not a second
            // complete mesh pass. Progressive prefixes keep all four wheels.
            const int wheelContours = std::min(contours, 32);
            const std::size_t edgeIndex = accent < wheelContours ?
                static_cast<std::size_t>(accent) * 64 / std::max(1, wheelContours) :
                64 + static_cast<std::size_t>(accent - wheelContours) *
                    (wizard::patronum_mesh::Edges.size() - 64) / std::max(1, contours - wheelContours);
            const auto& edge = wizard::patronum_mesh::Edges[edgeIndex];
            const auto& a = wizard::patronum_mesh::Vertices[edge.a];
            const auto& b = wizard::patronum_mesh::Vertices[edge.b];
            const Vec3 p = origin + (forward * a.x + right * a.y + up * a.z) * size;
            const Vec3 q = origin + (forward * b.x + right * b.y + up * b.z) * size;
            world.Segment(p, q, Alpha({193, 241, 255, 125}, fade * .7f), .8f * stroke);
        }
        for (int puff = 0; puff < wakes; ++puff) {
            const float t = (puff + .6f) / std::max(1, wakes);
            const float phase = t * Tau * .85f - age * 1.5f;
            const Vec3 p = ghost.position - forward * (40 + t * 120) +
                right * (std::sin(phase) * (4 + t * 12)) + up * (t * t * 22);
            const float strength = fade * (1 - t) * (1 - t);
            world.Glow(p, size * (.17f + t * .20f),
                puff % 3 == 0 ? Ink{211, 248, 255, 255} : Ink{107, 208, 255, 255},
                strength * .055f, true, 1.25f);
        }
        for (int spark = 0; spark < contact; ++spark) {
            const Vec3 ray = Scatter((spark + 2) % 18, .3f);
            const Vec3 p = ghost.contact + ray * (12 + Out(impactAge / .4f) * 90);
            world.Glow(p, 22 + Out(impactAge / .4f) * 45,
                spark % 3 ? Ink{126, 221, 255, 255} : Ink{234, 254, 255, 255},
                fade * (spark == 0 ? .42f : .10f), spark % 2 == 0);
        }
        return;
    }
    std::array<Vec3, wizard::patronum_mesh::Vertices.size()> points{};
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& v = wizard::patronum_mesh::Vertices[i];
        points[i] = origin + (forward * v.x + right * v.y + up * v.z) * size;
    }
    const Ink silver = Alpha({219, 250, 255, 235}, fade), cyan = Alpha({97, 211, 255, 195}, fade);
    const float stroke = std::clamp(world.height / 1080.0f, .7f, 1.8f);
    const int available = world.budget;
    const bool detailed = available >= 350;
    const int trailSegments = detailed ? 12 : 6;
    const int trailStrands = detailed ? 4 : 2;
    const int reserve = trailSegments * trailStrands + (struck ? 34 : 0);
    const int meshCount = std::min(solid ? 80 : static_cast<int>(wizard::patronum_mesh::Edges.size()),
        std::max(0, available - reserve));
    const int fullMesh = static_cast<int>(wizard::patronum_mesh::Edges.size());
    const bool glow = available >= fullMesh + (fullMesh + 15) / 16 + reserve;
    // This is the supplied Octane's open feature mesh, not an opaque mesh tint.
    // The pitch remains visible through its windows, body and wheels even on
    // native Canvas builds whose filled-triangle alpha is ignored.
    // A crowded scene keeps all four actual tread contours first, then samples
    // across the entire body/window/wing stream. It never spends a small slice
    // on just the mesh prefix and loses both its cabin and contact feedback.
    constexpr int wheelEdges = 64;
    for (int sample = 0; sample < meshCount; ++sample) {
        const std::size_t edgeIndex = meshCount >= 96 ? (sample < wheelEdges ? sample : wheelEdges +
            static_cast<std::size_t>(sample - wheelEdges) * (wizard::patronum_mesh::Edges.size() - wheelEdges) / (meshCount - wheelEdges)) :
            static_cast<std::size_t>(sample) * wizard::patronum_mesh::Edges.size() / std::max(1, meshCount);
        const auto& edge = wizard::patronum_mesh::Edges[edgeIndex];
        const auto& va = wizard::patronum_mesh::Vertices[edge.a];
        const auto& vb = wizard::patronum_mesh::Vertices[edge.b];
        const float shimmer = .84f + .16f * std::sin(age * 3.1f - (va.x + vb.x) * 2.4f);
        const bool contour = edgeIndex < wheelEdges || (va.z + vb.z) > .9f;
        // Silvery tread/cabin contours anchor a softer cyan feature mesh. Do
        // not drop random edges during fade: that caused a choppy dissolve.
        if (glow && edgeIndex % 16 == 0)
            world.Segment(points[edge.a], points[edge.b], Alpha(cyan, .25f * shimmer), 3.5f * stroke);
        world.Segment(points[edge.a], points[edge.b], Alpha(contour ? silver : cyan, shimmer),
            (contour ? 1.1f : .85f) * stroke);
    }
    for (int strand = 0; strand < trailStrands; ++strand) {
        Vec3 previous = ghost.position - forward * 40;
        for (int i = 1; i <= trailSegments; ++i) {
            const float t = i / static_cast<float>(trailSegments);
            const float angle = t * Tau * .72f - age * 4.2f + strand * Tau / trailStrands;
            const float spread = (12 + t * 28) * fade;
            const Vec3 point = ghost.position - forward * (40 + t * (180 + strand * 18)) +
                (right * std::cos(angle) + up * std::sin(angle)) * spread + up * (t * t * 42);
            world.Segment(previous, point, Alpha(strand == 1 ? silver : cyan,
                (1 - t) * (1 - t) * fade), strand == 1 ? 1.1f : 1.65f);
            previous = point;
        }
    }
    if (struck && impactAge >= 0 && impactAge < .45f) {
        const float radius = 30 + Out(impactAge / .4f) * 180;
        for (int sector = 0; sector < 3; ++sector) {
            WorldArc(world, ghost.contact, right, up, radius, Tau * sector / 3,
                Pi * .48f, Alpha({210, 250, 255, 245}, fade), 1.8f, 6);
        }
        for (int spark = 0; spark < 8; ++spark) {
            const Vec3 ray = Scatter(spark, .2f);
            const Vec3 head = ghost.contact + ray * (40 + impactAge * 420);
            world.Segment(head - ray * 7, head + ray * 32, Alpha(silver, fade), spark % 3 ? 1.25f : 2.3f);
            if (spark % 2 == 0)
                world.Segment(head, head + ray * 12, Alpha({255, 255, 255, 250}, fade), .8f);
        }
    }
}

void SummonAccio(WorldCanvas& world, const SpellVisuals::Effect& effect,
    const SpellVisuals::Frame& frame, float age) {
    const float duration = static_cast<float>(effect.duration > 0 ? effect.duration : 1.2);
    if (age < 0 || age >= duration) return;
    const bool opening = !effect.snapshot && age < 1.1f;
    const float reveal = effect.snapshot ? 1.0f : Out(age / 0.28f);
    const float fade = std::min(1.0f, (duration - age) / 0.18f);
    const Vec3 start = Position(frame, effect.casterId, effect.start, effect.casterKey) + Vec3{0, 0, 50};
    const Vec3 end = frame.ballValid ? frame.ball : effect.end;
    Vec3 across, up; Plane(end - start, across, up);
    const float clock = static_cast<float>(frame.time);
    const float distance = (end - start).Length();
    const float spread = std::clamp(distance * .025f, 22.0f, opening ? 76.0f : 47.0f);
    const Vec3 towardBall = (end - start).Normalized();
    const auto centerline = [&](float t) { return Lerp(start, end, t) + up * (std::sin(t * Pi) * 65); };
    // The ribbons converge at both ends. Slow phase motion keeps their shape
    // legible, while separate inward comets communicate the pull direction.
    for (int strand = 0; strand < (opening ? 3 : 2); ++strand) {
        Vec3 previous = start;
        const Ink ink = Alpha(strand == 1 ? Ink{224, 251, 255, 250} : Ink{73, 199, 255, 230}, fade);
        for (int i = 1; i <= 20; ++i) {
            const float t = reveal * i / 20, envelope = std::sin(t * Pi);
            const float phase = t * Tau * 1.7f - clock * 5.4f + strand * Tau / (opening ? 3 : 2);
            const Vec3 point = centerline(t) +
                (across * std::cos(phase) + up * std::sin(phase)) * (envelope * spread);
            if (strand == 0) world.Segment(previous, point, Alpha(ink, .24f), opening ? 7.0f : 4.4f);
            world.Segment(previous, point, Alpha(ink, .65f + .35f * envelope), strand == 1 ? 1.3f : 1.8f);
            previous = point;
        }
    }
    for (int i = 0; i < 6; ++i) {
        const float t = 1 - std::fmod(clock * .8f + i / 6.0f, 1.0f);
        if (t > reveal) continue;
        const float light = fade * std::sin(t * Pi);
        const Vec3 head = centerline(t);
        world.Glow(head, 28, {101,216,255,255}, light*.18f);
        world.Segment(head, head + towardBall * (34 + spread * .2f), Alpha({115, 222, 255, 245}, light), 2.5f);
        world.Segment(head, head + towardBall * 12, Alpha({239, 254, 255, 255}, light), 1.1f);
        if (i % 2 == 0) LineSpark(world, head, 5, Alpha({217, 252, 255, 235}, light), .35f);
    }
    // The moving magnetic tether persists only
    // until the host ends the summon or this bounded duration expires.
    if (opening) {
        for (int streak = 0; streak < 4; ++streak) {
            const float t = 1 - std::fmod(streak / 4.0f + clock * .55f, 1.0f);
            const Vec3 radial = across * std::cos(streak * Tau / 4) + up * std::sin(streak * Tau / 4);
            const Vec3 p = centerline(t) + radial * (std::sin(t * Pi) * spread * 1.7f);
            world.Segment(p + towardBall * 40, p, Alpha({135, 230, 255, 230}, fade * std::sin(t * Pi)), 1.9f);
        }
    }
    if (frame.ballValid && reveal > .98f) {
        const float radius = std::clamp(frame.ballRadius, 10.0f, 1000.0f) + 10;
        for (int crescent = 0; crescent < 2; ++crescent)
            WorldArc(world, end, world.cameraRight, world.cameraUp, radius, clock * .7f + crescent * Pi,
                Pi * .58f, Alpha({153, 234, 255, 215}, fade * .8f), 1.25f, 10);
    }
}

void SlugProjectiles(WorldCanvas& world, const SpellVisuals::Effect& effect,
    const SpellVisuals::Frame& frame, float age) {
    const float full=static_cast<float>(wizard::SlugFlightSeconds),clock=age;
    if(effect.charging||age<0||clock>=full) return;
    const Vec3 start = Position(frame, effect.casterId, effect.start, effect.casterKey) + Vec3{0, 0, 65};
    const Vec3 end = Position(frame, effect.targetId, effect.end, effect.targetKey) + Vec3{0, 0, 42};
    const Vec3 direction = (end - start).Normalized();
    Vec3 across, up; Plane(direction, across, up);
    // Shaded organic globs retain their short delivery window. Their outline
    // deforms continuously; the same recorded time always yields the same skin.
    for (int blob = 0; blob < 10; ++blob) {
        const float delay = blob * 0.012f;
        const float flight = (clock - delay) / (full - delay);
        if (flight <= 0 || flight >= 1) continue;
        const float t = Ease(flight), envelope = std::sin(t * Pi);
        const Vec3 center = Lerp(start, end, t) + up * (envelope * (165 + blob % 3 * 90.0f)) +
            across * (envelope * (blob % 5 - 2) * 80);
        const float radius = (82 + blob % 4 * 18.0f) * (.94f + .06f * std::sin(clock * 13 + blob));
        if ((center - world.camera).Dot(world.direction) < radius * 1.3f) continue;
        const float roll = blob * .71f + std::sin(clock * 7 + blob) * .16f;
        const Vec3 horizontal = world.cameraRight * std::cos(roll) + world.cameraUp * std::sin(roll);
        const Vec3 vertical = world.cameraUp * std::cos(roll) - world.cameraRight * std::sin(roll);
        const auto edge = [&](float angle, float size, float verticalScale = 1) {
            const float lobe = 1 + .075f * std::sin(angle * 3 + clock * 8 + blob) +
                .04f * std::cos(angle * 5 - clock * 6);
            return center + horizontal * (std::cos(angle) * radius * size * lobe) +
                vertical * (std::sin(angle) * radius * size * lobe * verticalScale);
        };
        for (int i = 0; i < 16; ++i) {
            const float angle = Tau * (i + .5f) / 16;
            const int light = static_cast<int>(9 * std::max(0.0f, std::sin(angle - .4f)));
            world.Facet(center, edge(Tau * i / 16, 1.1f, .8f), edge(Tau * (i + 1) / 16, 1.1f, .8f),
                {15 + light / 2, 37 + blob % 3 * 5 + light, 17 + light / 2, 255});
        }
        const Vec3 shine = center - horizontal * radius * .17f + vertical * radius * .16f;
        for (int i = 0; i < 10; ++i)
            world.Facet(shine, edge(Tau * i / 10, .76f, .63f), edge(Tau * (i + 1) / 10, .76f, .63f),
                {34, 71 + blob % 3 * 6, 29, 255});
        // A broken, narrow specular crescent makes the mucus read as wet, not
        // a flat green disc. It does not introduce another opaque outer shell.
        for (int glint = 0; glint < 5; ++glint) {
            const float a = .57f + glint * .19f;
            const Vec3 p = center + horizontal * (std::cos(a) * radius * .65f) + vertical * (std::sin(a) * radius * .52f);
            const Vec3 q = center + horizontal * (std::cos(a + .13f) * radius * .65f) + vertical * (std::sin(a + .13f) * radius * .52f);
            world.Segment(p, q, glint == 2 ? Ink{205, 222, 145, 255} : Ink{118, 151, 74, 255}, glint == 2 ? 2.4f : 1.3f);
        }
        const Vec3 tail = center - direction * (95 + envelope * 105);
        world.Segment(center, tail, {23, 54, 22, 255}, 10 + blob % 3 * 2.0f);
        for (int droplet = 0; droplet < 2; ++droplet) {
            const Vec3 p = tail - direction * (droplet * 42.0f) + up * std::sin(clock * 13 + droplet + blob) * 23;
            world.Segment(p, p - direction * (15 + droplet * 10.0f), {64, 99, 39, 255}, 5 - droplet * 1.8f);
        }
    }
}

// Clip the intentionally opaque screen effect separately from world-space
// strokes. Falling slugs never send enormous off-screen triangles to Canvas.
void ScreenTriangle(CanvasWrapper& canvas, Point a, Point b, Point c, Ink ink, float width, float height) {
    std::vector<Point> polygon{a, b, c};
    for (int edge = 0; edge < 4 && !polygon.empty(); ++edge) {
        auto value = [&](Point p) { return edge < 2 ? p.x : p.y; };
        const float bound = edge == 1 ? width : edge == 3 ? height : 0;
        auto inside = [&](Point p) { return edge % 2 ? value(p) <= bound : value(p) >= bound; };
        std::vector<Point> clipped;
        Point previous = polygon.back(); bool previousInside = inside(previous);
        for (const auto point : polygon) {
            const bool pointInside = inside(point);
            if (pointInside != previousInside) {
                const float span = value(point) - value(previous);
                if (std::abs(span) > 0.00001f) {
                    const float t = (bound - value(previous)) / span;
                    clipped.push_back(previous + (point - previous) * t);
                }
            }
            if (pointInside) clipped.push_back(point);
            previous = point; previousInside = pointInside;
        }
        polygon.swap(clipped);
    }
    if (polygon.size() >= 3) Polygon(canvas, polygon, ink);
}

void ScreenDisc(CanvasWrapper& canvas, Point center, float radius, Ink ink, float width, float height) {
    for (int i = 0; i < 10; ++i)
        ScreenTriangle(canvas, center, Polar(center, radius, Tau * i / 10),
            Polar(center, radius, Tau * (i + 1) / 10), ink, width, height);
}

void CameraSlug(CanvasWrapper& canvas, Point center, float halfLength, float halfWidth,
    float angle, float phase, int palette, float screenWidth, float screenHeight) {
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const auto local = [&](float x, float y) {
        return center + Point{x * cosine - y * sine, x * sine + y * cosine};
    };
    const auto bodyAt = [&](float t, float across) {
        // Only the rear tapers. The front bends around a broad semicircular
        // cap, giving the head a swollen, rounded silhouette.
        const float head = std::clamp((t - 0.45f) / 0.55f, 0.0f, 1.0f);
        const float taper = t > 0.45f ? std::cos(head * Pi * 0.5f) :
            std::pow(std::clamp((t + 1) / 0.8f, 0.0f, 1.0f), 0.7f);
        const float x = t > 0.45f ? (0.45f + std::sin(head * Pi * 0.5f) * 0.55f) * halfLength : t * halfLength;
        const float bend = std::sin(t * Pi * 1.15f + phase) * halfWidth * .14f;
        return local(x, bend + across * taper * halfWidth);
    };
    const auto strip = [&](float low, float high, Ink ink) {
        for (int slice = 0; slice < 14; ++slice) {
            const float t0 = -1 + slice / 7.0f, t1 = -1 + (slice + 1) / 7.0f;
            const Point a = bodyAt(t0, low), b = bodyAt(t1, low);
            const Point c = bodyAt(t1, high), d = bodyAt(t0, high);
            ScreenTriangle(canvas, a, b, c, ink, screenWidth, screenHeight);
            ScreenTriangle(canvas, a, c, d, ink, screenWidth, screenHeight);
        }
    };
    const std::array<Ink, 3> colors{{{47, 76, 31, 255}, {57, 82, 32, 255}, {39, 65, 28, 255}}};
    const Ink body = colors[palette % colors.size()];
    strip(-1.12f, 1.12f, {23, 43, 23, 255}); // wet contact seam around the foot
    strip(-1.0f, 1.0f, body);
    strip(-.82f, -.16f, {body.r + 28, body.g + 33, body.b + 14, 255});
    strip(.48f, .91f, {body.r - 15, body.g - 17, body.b - 7, 255});
    // A raised oval mantle behind the head distinguishes a slug from a leaf.
    const auto mantle = [&](float scale, Ink ink) {
        const Point middle = local(halfLength * 0.32f, -halfWidth * 0.04f);
        for (int i = 0; i < 14; ++i) {
            const float a = Tau * i / 14, b = Tau * (i + 1) / 14;
            const Point p = local(halfLength * (0.32f + std::cos(a) * 0.27f * scale),
                halfWidth * (-0.04f + std::sin(a) * 0.79f * scale));
            const Point q = local(halfLength * (0.32f + std::cos(b) * 0.27f * scale),
                halfWidth * (-0.04f + std::sin(b) * 0.79f * scale));
            ScreenTriangle(canvas, middle, p, q, ink, screenWidth, screenHeight);
        }
    };
    mantle(1, {body.r - 18, body.g - 20, body.b - 10, 255});
    mantle(.88f, {body.r + 16, body.g + 22, body.b + 10, 255});
    // Short transverse creases and irregular skin dimples distinguish the
    // muscular foot from a leaf or a flat painted shape.
    for (int mark = 0; mark < 10; ++mark) {
        const float t = -.82f + mark * .15f;
        const float band = std::sin(mark * 2.71f + palette) * .5f;
        Point a = bodyAt(t, band), b = bodyAt(t + .009f, band + .035f);
        if (ClipLine(a, b, screenWidth, screenHeight)) {
            Line(canvas, a, b, {body.r - 18, body.g - 22, body.b - 10, 255}, halfWidth * .035f);
            Point highlightA = a + Point{0, -halfWidth * .012f}, highlightB = b + Point{0, -halfWidth * .012f};
            if (ClipLine(highlightA, highlightB, screenWidth, screenHeight))
                Line(canvas, highlightA, highlightB, {body.r + 34, body.g + 38, body.b + 20, 255}, halfWidth * .012f);
        }
    }
    for (int crease = 0; crease < 6; ++crease) {
        const float t = -.74f + crease * .16f;
        Point a = bodyAt(t, .04f), b = bodyAt(t + .015f, .34f), c = bodyAt(t, .57f);
        Point low = a, high = b;
        if (ClipLine(low, high, screenWidth, screenHeight))
            Line(canvas, low, high, {body.r - 11, body.g - 15, body.b - 5, 255}, std::max(.8f, halfWidth * .009f));
        low = b; high = c;
        if (ClipLine(low, high, screenWidth, screenHeight))
            Line(canvas, low, high, {body.r - 11, body.g - 15, body.b - 5, 255}, std::max(.8f, halfWidth * .009f));
    }
    // Broken pearl-green highlights track the curved top instead of filling
    // the entire animal with a flat light band. Shape survives opaque Canvas.
    for (int shine = 0; shine < 9; ++shine) {
        if (shine == 3 || shine == 6) continue;
        const float t = -.73f + shine * .14f;
        Point a = bodyAt(t, -.52f), b = bodyAt(t + .085f, -.53f);
        if (ClipLine(a, b, screenWidth, screenHeight)) {
            Line(canvas, a, b, {128, 160, 83, 255}, std::max(1.3f, halfWidth * .025f));
            Line(canvas, a, b, shine % 3 == 0 ? Ink{215, 229, 167, 255} : Ink{166, 195, 112, 255}, std::max(.9f, halfWidth * .011f));
        }
    }
    // Paired long eye stalks and shorter feelers make the bodies read as slugs.
    for (int eye = 0; eye < 2; ++eye) {
        const float side = eye ? 1.0f : -1.0f;
        Point root = bodyAt(0.73f, side * 0.42f);
        Point tip = local(halfLength * 1.12f, side * halfWidth * (1.02f + .04f * std::sin(phase + eye)) - halfWidth * .08f);
        Point a = root, b = tip;
        if (ClipLine(a, b, screenWidth, screenHeight)) {
            Line(canvas, a, b, {20, 31, 17, 255}, halfWidth * 0.12f);
            Line(canvas, a, b, {body.r + 24, body.g + 26, body.b + 8, 255}, halfWidth * 0.07f);
        }
        ScreenDisc(canvas, tip, halfWidth * 0.092f, {14, 22, 13, 255}, screenWidth, screenHeight);
        ScreenDisc(canvas, tip + Point{-halfWidth * 0.025f, -halfWidth * 0.025f}, halfWidth * 0.027f,
            {225, 238, 161, 255}, screenWidth, screenHeight);
        a = bodyAt(0.77f, side * 0.3f);
        b = local(halfLength * 1.08f, side * halfWidth * 0.45f);
        if (ClipLine(a, b, screenWidth, screenHeight)) Line(canvas, a, b, {body.r, body.g, body.b, 255}, halfWidth * 0.035f);
    }
}

void SlugOverlay(CanvasWrapper& canvas, const SpellVisuals::Effect& effect, const SpellVisuals::Frame& frame) {
    if (!frame.enabled || !frame.localSlugged || frame.localPlayerId < 0 ||
        !SameActor(effect.targetId, effect.targetKey, frame.localPlayerId, frame.localPlayerKey)) return;
    const double duration=wizard::EatSlugsDurationSeconds;
    const double elapsed=frame.time-effect.began-wizard::SlugFlightSeconds;
    if(elapsed<0||elapsed>=duration) return;
    const float age=static_cast<float>(elapsed);
    const auto screen = canvas.GetSize();
    const float width = static_cast<float>(screen.X), height = static_cast<float>(screen.Y);
    if (!std::isfinite(width) || !std::isfinite(height) || width < 160 || height < 90) return;
    const float splat = Out(age / 0.16f);
    if (splat <= 0) return;
    // The mucus holds for most of the spell, then the bodies accelerate down
    // and completely leave the camera by its 3.75-second deadline.
    const float slide = Ease((age - 2.85f) / 0.9f);
    const std::array<Point, 9> placement{{{.12f, .14f}, {.48f, .2f}, {.86f, .13f},
        {.16f, .47f}, {.5f, .48f}, {.84f, .52f},
        {.12f, .81f}, {.49f, .79f}, {.88f, .83f}}};
    for (int slug = 0; slug < 9; ++slug) {
        const float stagger = slug * 0.014f;
        const float fall = Ease((age - 2.85f - stagger) / (0.9f - stagger));
        const float wobble = std::sin(age * (1.7f + slug * 0.05f) + slug * 2.7f);
        const bool left = slug % 4 == 1 || slug % 4 == 2;
        const float angle = (left ? Pi : 0) + (slug % 3 - 1) * .24f + wobble * .025f;
        const float size = splat * (1 - slide * 0.12f);
        const float halfLength = width * (0.244f + (slug % 2) * 0.012f) * size;
        const float halfWidth = height * (.11f + (slug % 3) * .0035f) * size;
        Point center{width * placement[slug].x + width * wobble * 0.006f * (1 - slide),
            height * (placement[slug].y + fall * 1.2f)};
        const float extentY = std::abs(std::sin(angle) * halfLength) + halfWidth * 1.3f;
        if (center.y - extentY > height + 2) continue;
        // A few narrow wet streaks stretch behind each falling body. They
        // stay in the slug's own track and never become a full-screen tint.
        if (fall > .005f && fall < .9f) {
            for (int track = 0; track < 3; ++track) {
                const float x = center.x + (track - 1) * halfWidth * .28f;
                const float length = std::min(height * fall * .38f, height * .25f);
                Point a{x, center.y - halfWidth * .25f}, b{x + wobble * width * .003f,
                    center.y - halfWidth * .25f - length};
                if (!ClipLine(a, b, width, height)) continue;
                Line(canvas, a, b, {37, 64, 35, 255}, std::max(2.0f, height * .004f));
                Line(canvas, a, b, {93, 128, 65, 255}, std::max(.9f, height * .0012f));
            }
        }
        CameraSlug(canvas, center, halfLength, halfWidth, angle, age * 1.2f + slug * 1.7f,
            slug, width, height);
    }
}

double Lifetime(const SpellVisuals::Effect& effect) {
    if (effect.charging) return std::clamp(effect.duration, 0.0, wizard::SpellChargeSeconds(effect.spell));
    if (effect.spell == Spell::Accio) return effect.duration > 0 ? effect.duration : 1.2;
    if (effect.spell == Spell::PetrificusTotalus) return (effect.duration > 0 ? effect.duration : wizard::PetrificusDurationSeconds) + 0.15;
    if (effect.spell == Spell::Leviosa) return (effect.duration > 0 ? effect.duration : wizard::LeviosaSafetySeconds) + 0.4;
    if (effect.spell == Spell::Obliviate) return effect.duration>0?effect.duration:wizard::ObliviateDurationSeconds;
    if (effect.spell == Spell::Imperio) return effect.duration>0?effect.duration:wizard::ImperioDurationSeconds;
    if (effect.spell == Spell::EatSlugs) return wizard::SlugFlightSeconds+wizard::EatSlugsDurationSeconds;
    return std::max(1.2, effect.duration);
}

enum class EffectLayer { Both, Cast, Status };

void DrawEffect(WorldCanvas& world, const SpellVisuals::Effect& effect, const SpellVisuals::Frame& frame,
    EffectLayer layer = EffectLayer::Both) {
    const float age = static_cast<float>(frame.time - effect.began);
    if (age < 0 || age > Lifetime(effect)) return;
    if (effect.charging) {
        if (layer != EffectLayer::Status) {
            ChargeSpell(world, effect, frame, age);
        }
        return;
    }
    if (effect.spell == Spell::EatSlugs) {
        if(layer!=EffectLayer::Status) {
            if(age<wizard::SlugFlightSeconds) SlugProjectiles(world,effect,frame,age);
            else Burst(world,effect.end+Vec3{0,0,40},{45,105,31,255},
                age-static_cast<float>(wizard::SlugFlightSeconds),1.7f);
        }
        return;
    }
    if (effect.spell == Spell::Accio) {
        const bool initial = !effect.snapshot && age < 1.2f;
        if (layer == EffectLayer::Both || (initial ? layer == EffectLayer::Cast : layer == EffectLayer::Status))
            SummonAccio(world, effect, frame, age);
        return;
    }
    if (effect.spell == Spell::ExpectoPatronum) return;
    if(effect.spell==Spell::Imperio) {
        if(layer!=EffectLayer::Cast) ImperioControl(world,effect,frame,age);
        return;
    }
    if (layer != EffectLayer::Cast) {
        if (effect.spell == Spell::PetrificusTotalus) Freeze(world, effect, frame, age);
        else if (effect.spell == Spell::Leviosa) Levitate(world, effect, frame, age);
        else if (effect.spell == Spell::Obliviate) MemoryDrift(world, effect, frame, age);
        else if (effect.spell == Spell::BombardaMaxima) BombardaRicochetGlow(world,effect,frame,age);
    }
    if (layer == EffectLayer::Status || effect.snapshot || age > 1.2f) return;
    Vec3 start = effect.start + Vec3{0, 0, 50}, end = effect.end;
    const float fade = 1 - Unit((age - 0.35f) / 0.85f);
    if (effect.spell == Spell::PetrificusTotalus) {
        Bolt(world, start, end + Vec3{0, 0, 30}, {100, 218, 255, 255}, age, 40);
        if (age < .7f) for (int ray=0;ray<10;++ray) {
            const Vec3 dir=Scatter(ray,age*.5f);
            const Vec3 p=end+dir*(60+Out(age/.6f)*215);
            world.Segment(p,p+dir*50,Alpha({220,251,255,255},1-age/.7f),2.5f);
        }
        // The binding flash belongs to the cast only. The held car continues
        // to use just the player's requested thin light-blue body contour.
        if (age > 0.12f && age < 0.85f) {
            const float bind = 1 - Unit((age - 0.12f) / 0.73f);
            const float expansion = Out((age - .12f) / .5f);
            for (int shard = 0; shard < 8; ++shard) {
                const Vec3 ray = Scatter(shard, .4f);
                const Vec3 p = end + Vec3{0, 0, 30} + ray * (70 + expansion * 145);
                const Vec3 axis = world.cameraRight * (shard % 2 ? 1.f : -1.f);
                world.Segment(p - ray * 16, p + ray * 20, Alpha({130, 231, 255, 245}, bind), 1.4f);
                world.Segment(p, p - ray * 5 + axis * 10, Alpha({226, 251, 255, 255}, bind), .85f);
                world.Segment(p + ray * 7, p + ray * 2 - axis * 7, Alpha({226, 251, 255, 255}, bind), .85f);
            }
        }
        return;
    }
    if (effect.spell == Spell::Leviosa) {
        SpiralCast(world, start, end + Vec3{0, 0, 70}, {182, 107, 255, 255}, {248, 216, 255, 255}, age, 215, 3.4f);
        for (int feather=0;feather<8;++feather) {
            const float t=std::fmod(age*.9f+feather/8.0f,1.0f), phase=feather*2.4f-age*3;
            const Vec3 p=end+Vec3{std::cos(phase)*210,std::sin(phase)*210,t*450};
            const float featherFade=fade*std::sin(Pi*t);
            world.Segment(p-world.cameraUp*20,p+world.cameraUp*20,Alpha({247,218,255,255},featherFade),2);
            world.Segment(p,p+world.cameraRight*12+world.cameraUp*7,Alpha({191,113,255,255},featherFade),2);
        }
        return;
    }
    if(effect.spell==Spell::Obliviate) {
        ObliviateLaser(world,start,end+Vec3{0,0,45},age); return;
    }
    if (effect.spell == Spell::BombardaMaxima) {
        const Vec3 direction = (end - start).Normalized();
        Vec3 right, up; Plane(direction, right, up);
        const float travel = Out(age / .2f);
        const Vec3 head = Lerp(start, end, travel);
        if (age < 0.65f) {
            const float beam = 1 - Ease((age - .22f) / .43f);
            Vec3 previous = start;
            for (int segment = 1; segment <= 18; ++segment) {
                const float t = travel * segment / 18.f, flare = std::sin(Pi * t);
                const Vec3 p = Lerp(start, end, t) + right * (std::sin(t * 22 - age * 9) * flare * 12) +
                    up * (std::sin(t * 13 + age * 7) * flare * 8);
                world.Segment(previous, p, Alpha({255, 108, 24, 255}, beam * .24f), 11);
                world.Segment(previous, p, Alpha({255, 169, 57, 255}, beam), 4);
                world.Segment(previous, p, Alpha({255, 244, 203, 255}, beam), 1.25f);
                previous = p;
            }
            LineSpark(world, head, 29, Alpha({255, 241, 191, 255}, beam), .5f + age);
        }
        for (int front = 0; front < 3; ++front) {
            const float wave = age - front * .075f;
            if (wave < 0 || wave > 0.95f) continue;
            const Vec3 center = end + direction * (wave * 540);
            const float radius = 40 + Out(wave / .85f) * (410 - front * 55.f);
            for (int petal = 0; petal < 4; ++petal)
                WorldArc(world, center, right, up, radius, petal * Pi * .5f + .16f + front * .13f,
                    .72f, Alpha(front == 1 ? Ink{255, 236, 176, 245} : Ink{255, 142, 47, 255}, fade),
                    front ? 1.35f : 2.4f, 6);
        }
        Burst(world, end, {255, 153, 58, 255}, age - 0.1f, 1.9f);
        for (int ray=0;ray<12;++ray) {
            const float angle=Tau*ray/12+age;
            const Vec3 radial=right*std::cos(angle)+up*std::sin(angle);
            const Vec3 front=end+direction*(age*700)+radial*(45+Out(age/.8f)*360);
            world.Segment(end+radial*40,front,Alpha({255,182,82,255},fade),ray%3?1.4f:3);
        }
    } else if (effect.spell == Spell::Protego) {
        SpiralCast(world, start, end, {96, 246, 219, 245}, {255, 233, 152, 240}, age, 45, 0.8f);
        Vec3 right, up; Plane(end - start, right, up);
        const float radius = 350 * Out(age / 0.3f);
        for (int mote=0;mote<10;++mote) {
            const float phase=Tau*mote/10+age*1.5f;
            const Vec3 radial=right*std::cos(phase)+up*std::sin(phase);
            const Vec3 p=end+radial*(radius+80+age*95);
            world.Segment(p,p-radial*35,Alpha({154,255,228,255},fade),2);
        }
        world.Circle(end, right, up, radius, Alpha({191, 255, 235, 245}, fade), 2.4f, age, 28);
        // Opening energy stays at the rim; the persistent circular ward is
        // drawn separately and retains its clear, unfilled center.
        for (int sector = 0; sector < 6; ++sector) {
            const float angle = Tau * sector / 6 + age * 1.5f;
            WorldArc(world, end, right, up, radius * 0.88f, angle, Pi * 0.18f,
                Alpha({255, 236, 167, 240}, fade), 2, 4);
            const Vec3 radial = right * std::cos(angle) + up * std::sin(angle);
            world.Segment(end + radial * radius * 0.78f, end + radial * radius * 1.04f,
                Alpha({212, 255, 240, 240}, fade), 1.4f);
        }
    } else if (effect.spell == Spell::Stupefy) {
        StupefyFlash(world, start, end + Vec3{0, 0, 45}, age);
    } else if (effect.spell == Spell::AvadaKedavra) {
        // The endpoint is the captured cast location. A demoed victim's new
        // respawn position must never pull the killing curse across the pitch.
        AvadaBeam(world, start, end + Vec3{0, 0, 45}, age);
    }
}

std::string ShortName(Spell spell) {
    if (spell == Spell::Leviosa) return "WINGARDIUM LEVIOSA";
    if (spell == Spell::AvadaKedavra) return "AVADA KEDAVRA";
    if (spell == Spell::PetrificusTotalus) return "PETRIFICUS TOTALUS";
    std::string result = wizard::GetSpellInfo(spell).name;
    for (char& c : result) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return result;
}

void HudLabel(CanvasWrapper& canvas, Point at, const std::string& text, float height, Ink ink, float /*uiScale*/) {
    // Measure the game's actual font instead of relying on tiny fractional scales.
    // This keeps every spell at the same readable height, including two-line names.
    const auto metrics = canvas.GetStringSize("WINGARDIUM", 1, 1);
    const float textScale = height / std::max(1.0f, metrics.Y);
    Label(canvas, at, text, textScale, ink, true);
}

void Hud(CanvasWrapper& canvas, const SpellVisuals::Frame& frame, double revealAt) {
    const auto screen = canvas.GetSize();
    if (screen.X < 480 || screen.Y < 320) return;
    const float resolution = std::clamp(screen.Y / 1080.0f, 0.6f, 2.5f);
    const float requestedScale = resolution * (std::isfinite(frame.hudScale) ? std::clamp(frame.hudScale, 0.7f, 1.6f) : 1.0f);
    const float scale = std::min({requestedScale, screen.X / 320.0f, screen.Y / 280.0f});
    // Anchor to the left safe area and leave space below for the native ball-cam label.
    const Point center{154 * scale, screen.Y - 160 * scale};
    const bool connected = frame.isHost || (frame.synchronized && frame.transportReady);
    const bool active = frame.enabled && connected;
    const bool held = active && frame.spell != Spell::None;
    const bool charging = held && wizard::IsChargedSpell(frame.spell) && (frame.spellCharging || frame.avadaCharging) &&
        std::isfinite(frame.chargeRemaining);
    const double fullCharge = wizard::SpellChargeSeconds(frame.spell);
    const double chargeRemaining = charging ? std::clamp(frame.chargeRemaining, 0.0, fullCharge) : 0;
    const Ink accent = held ? SpellInk(frame.spell) : Ink{103, 222, 222, 255};
    const float reveal = held ? 1 - Unit((frame.time - revealAt) / 0.3) : 0;
    const float radius = 44 * scale;

    // One flat disc and one progress ring keep the spell slot easy to scan.
    Disc(canvas, center, radius, {8, 18, 26, 222}, 56);
    Arc(canvas, center, radius, 0, Tau, {70, 104, 114, 155}, 2 * scale, 64);
    if (held) {
        if (charging) {
            const float progress = 1 - Unit(chargeRemaining / fullCharge);
            const bool liftCharge = frame.spell == Spell::Leviosa;
            const bool stunCharge = frame.spell == Spell::Stupefy;
            const bool bindCharge = frame.spell == Spell::PetrificusTotalus;
            const Ink chargeInk = liftCharge ? Ink{213, 145, 255, 255} : stunCharge ? Ink{255, 65, 82, 255} :
                bindCharge ? Ink{156, 181, 255, 255} : frame.spell == Spell::EatSlugs ? Ink{145, 198, 61, 255} :
                frame.spell == Spell::BombardaMaxima ? Ink{255, 143, 53, 255} :
                frame.spell == Spell::Protego ? Ink{125, 245, 218, 255} :
                frame.spell == Spell::Obliviate ? Ink{255, 222, 48, 255} :
                frame.spell == Spell::Accio ? Ink{85, 195, 255, 255} :
                frame.spell == Spell::Imperio ? Ink{188, 145, 255, 255} :
                frame.spell == Spell::ExpectoPatronum ? Ink{139, 235, 255, 255} : Ink{54, 255, 91, 255};
            const Ink chargePale = liftCharge ? Ink{248, 221, 255, 255} : stunCharge ? Ink{255, 229, 223, 255} :
                bindCharge ? Ink{222, 247, 255, 255} : frame.spell == Spell::BombardaMaxima ? Ink{255, 246, 213, 255} :
                frame.spell == Spell::Protego ? Ink{238, 255, 213, 255} :
                frame.spell == Spell::Obliviate ? Ink{255, 255, 204, 255} :
                frame.spell == Spell::Accio ? Ink{221, 248, 255, 255} :
                frame.spell == Spell::Imperio ? Ink{246, 224, 255, 255} :
                frame.spell == Spell::ExpectoPatronum ? Ink{238, 255, 255, 255} : Ink{169, 255, 188, 255};
            const Ink chargeTrack{chargeInk.r / 4, chargeInk.g / 4, chargeInk.b / 4, 245};
            Arc(canvas, center, radius, -Pi * 0.5f, Tau, chargeTrack, 3.2f * scale, 64);
            Arc(canvas, center, radius, -Pi * 0.5f, Tau * progress, chargeInk, 3.2f * scale, 64);
            for (int arc = 0; arc < 4; ++arc) {
                const float angle = Tau * arc / 4 + static_cast<float>(frame.time) * 0.7f;
                Arc(canvas, center, radius - 6 * scale, angle, 0.38f, Alpha(chargePale, 0.8f), scale, 48);
            }
            HudLabel(canvas, center + Point{0, -26 * scale}, "CASTING", 11 * scale, chargePale, scale);
            // Round upward so a live charge never reads 0.0 before release.
            std::ostringstream countdown;
            const double tenths = chargeRemaining > 0 ? std::max(0.1, std::ceil(chargeRemaining * 10 - 0.00001) / 10) : 0;
            countdown << std::fixed << std::setprecision(1) << tenths;
            HudLabel(canvas, center + Point{0, -10 * scale}, countdown.str(), 31 * scale,
                chargePale, scale);
        } else {
            Arc(canvas, center, radius, -Pi * 0.5f, Tau, Alpha(accent, 0.9f), 2.3f * scale, 64);
            Glyph(canvas, frame.spell, center, 28 * scale * (1 + reveal * 0.06f),
                {235, 250, 245, 255}, static_cast<float>(frame.time));
        }
    } else if (active) {
        const double remaining = std::isfinite(frame.remaining) ? std::max(0.0, frame.remaining) : 0;
        const double cooldown = std::isfinite(frame.cooldown) ? std::max(1.0, frame.cooldown) : 10;
        const float progress = 1 - Unit(remaining / cooldown);
        Arc(canvas, center, radius, -Pi * 0.5f, Tau * progress, {116, 227, 225, 235}, 2.3f * scale, 64);
        HudLabel(canvas, center + Point{0, -27 * scale}, "NEXT", 11 * scale, {166, 196, 201, 255}, scale);
        const int seconds = std::max(0, static_cast<int>(std::ceil(std::min(remaining, 999.0))));
        HudLabel(canvas, center + Point{0, -12 * scale}, seconds ? std::to_string(seconds) : "...", 38 * scale,
            {235, 245, 237, 255}, scale);
    } else {
        Glyph(canvas, Spell::None, center + Point{0, -7 * scale}, 21 * scale, {113, 153, 165, 160}, static_cast<float>(frame.time));
        HudLabel(canvas, center + Point{0, 20 * scale}, connected ? "PAUSED" : "WAITING", 12 * scale, {190, 209, 211, 250}, scale);
    }

    const float plateWidth = 260 * scale, plateHeight = 48 * scale, plateY = center.y + 56 * scale;
    const std::vector<Point> plate{{center.x - plateWidth / 2 + 4 * scale, plateY},
        {center.x + plateWidth / 2 - 4 * scale, plateY}, {center.x + plateWidth / 2, plateY + 4 * scale},
        {center.x + plateWidth / 2, plateY + plateHeight - 4 * scale},
        {center.x + plateWidth / 2 - 4 * scale, plateY + plateHeight},
        {center.x - plateWidth / 2 + 4 * scale, plateY + plateHeight},
        {center.x - plateWidth / 2, plateY + plateHeight - 4 * scale}, {center.x - plateWidth / 2, plateY + 4 * scale}};
    Polygon(canvas, plate, {7, 17, 25, 220});
    const std::string name = held ? ShortName(frame.spell) : active ? "WIZARD RUMBLE" : !connected ? "CONNECTING TO HOST" : "WIZARD RUMBLE";
    const Ink nameInk = held ? Ink{245, 252, 248, 255} : Ink{205, 224, 225, 255};
    if (held && (frame.spell == Spell::Leviosa || frame.spell == Spell::PetrificusTotalus ||
        frame.spell == Spell::ExpectoPatronum || frame.spell == Spell::BombardaMaxima)) {
        const auto split = name.find(' ');
        HudLabel(canvas, {center.x, plateY + 3 * scale}, name.substr(0, split), 20 * scale, nameInk, scale);
        HudLabel(canvas, {center.x, plateY + 25 * scale}, name.substr(split + 1), 20 * scale, nameInk, scale);
    } else {
        HudLabel(canvas, {center.x, plateY + 14 * scale}, name, 20 * scale, nameInk, scale);
    }

    const Point key{center.x + 37 * scale, center.y + 25 * scale};
    const std::vector<Point> keyBox{{key.x, key.y}, {key.x + 22 * scale, key.y},
        {key.x + 22 * scale, key.y + 21 * scale}, {key.x, key.y + 21 * scale}};
    Polygon(canvas, keyBox, {13, 29, 37, 245});
    Polyline(canvas, keyBox, held ? Alpha(accent, 0.7f) : Ink{78, 111, 120, 180}, scale, true);
    HudLabel(canvas, key + Point{11 * scale, 3 * scale}, "R", 15 * scale,
        held ? Ink{233, 249, 243, 255} : Ink{153, 188, 193, 255}, scale);
    if (!frame.feedback.empty()) {
        std::string feedback = frame.feedback;
        if (feedback.size() > 52) feedback = feedback.substr(0, 49) + "...";
        const auto metrics = canvas.GetStringSize("WINGARDIUM", 1, 1);
        const float fontScale = 14 * scale / std::max(1.0f, metrics.Y);
        const float maxWidth = std::min(360 * scale, screen.X - 48 * scale);
        auto width = [&](const std::string& text) { return canvas.GetStringSize(text, fontScale, fontScale).X; };
        if (width(feedback) > maxWidth) {
            while (!feedback.empty() && width(feedback + "...") > maxWidth) feedback.pop_back();
            feedback += "...";
        }
        const float feedbackX = std::clamp(center.x, 24 * scale + width(feedback) / 2,
            screen.X - 24 * scale - width(feedback) / 2);
        HudLabel(canvas, {feedbackX, center.y - 72 * scale}, feedback, 14 * scale, {219, 232, 226, 235}, scale);
    }
}
}

void SpellVisuals::Reset() {
    effects_.clear(); slugOverlays_.clear(); lastSpell_ = Spell::None; revealAt_ = -100;
}

void SpellVisuals::ClearEffects(bool preserveSlugs) {
    effects_.clear();
    if (!preserveSlugs) slugOverlays_.clear();
}

void SpellVisuals::StartEffect(const Effect& incoming) {
    auto effect=incoming;
    if (effect.spell == Spell::None || !wizard::IsValidSpell(effect.spell) ||
        !effect.start.IsFinite() || !effect.end.IsFinite() || !std::isfinite(effect.began) ||
        !std::isfinite(effect.duration) || effect.duration < 0 || effect.duration > 30) return;
    if (effect.charging && (!wizard::IsChargedSpell(effect.spell) || effect.duration <= 0 || effect.duration > wizard::SpellChargeSeconds(effect.spell) + 0.001)) return;
    if(!effect.charging&&(effect.spell==Spell::EatSlugs||effect.spell==Spell::Imperio||
        effect.spell==Spell::Obliviate||effect.spell==Spell::BombardaMaxima)) {
        const double total=effect.spell==Spell::EatSlugs?wizard::SlugFlightSeconds+wizard::EatSlugsDurationSeconds:
            effect.spell==Spell::Imperio?wizard::ImperioDurationSeconds:
            effect.spell==Spell::Obliviate?wizard::ObliviateDurationSeconds:wizard::BombardaRicochetDurationSeconds;
        if(effect.duration<=0||effect.duration>total+.001) return;
        // P carries remaining time, while effects use their original release
        // clock so a refresh never restarts a status animation or its fade.
        if(effect.snapshot) effect.began-=total-effect.duration;
        effect.duration=total;
    }
    if (!effect.charging && effect.snapshot &&
        (effect.spell == Spell::Accio || effect.spell == Spell::Leviosa || effect.spell == Spell::PetrificusTotalus)) {
        const double total = effect.spell == Spell::Accio ? wizard::AccioDurationSeconds :
            effect.spell == Spell::Leviosa ? wizard::LeviosaSafetySeconds : wizard::PetrificusTotalusDurationSeconds;
        if (effect.duration <= 0 || effect.duration > total + .001) return;
        // The host sends remaining life. Recover the original cosmetic age so
        // joining/refreshing does not restart an updraft or its frost glints.
        effect.began -= total - effect.duration;
        effect.duration = total;
    }
    if (wizard::IsChargedSpell(effect.spell)) {
        // A completion bolt replaces its caster's channel. A charge snapshot
        // replaces only a previous channel, preserving unrelated flying bolts.
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [&](const Effect& existing) {
            return existing.charging && SameActor(existing.casterId, existing.casterKey, effect.casterId, effect.casterKey);
        }), effects_.end());
    }
    if (effect.spell == Spell::EatSlugs && !effect.charging) {
        if (effect.targetId < 0) return;
        slugOverlays_.erase(std::remove_if(slugOverlays_.begin(), slugOverlays_.end(), [&](const Effect& existing) {
            return SameActor(existing.targetId, existing.targetKey, effect.targetId, effect.targetKey);
        }), slugOverlays_.end());
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [&](const Effect& existing) {
            return !existing.charging && existing.spell == Spell::EatSlugs &&
                SameActor(existing.targetId, existing.targetKey, effect.targetId, effect.targetKey);
        }), effects_.end());
        // A separate bounded list protects gameplay obstruction from cosmetic
        // eviction when many other spells are rendering at the same time.
        if (slugOverlays_.size() >= 16) slugOverlays_.erase(slugOverlays_.begin());
        slugOverlays_.push_back(effect);
    }
    if (effect.spell == Spell::Accio) {
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [&](const Effect& existing) {
            return existing.spell == Spell::Accio && SameActor(existing.casterId, existing.casterKey, effect.casterId, effect.casterKey);
        }), effects_.end());
    }
    if (effect.spell == Spell::BombardaMaxima && !effect.charging) {
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [&](const Effect& existing) {
            return !existing.charging && existing.spell == Spell::BombardaMaxima &&
                SameActor(existing.casterId, existing.casterKey, effect.casterId, effect.casterKey);
        }), effects_.end());
    }
    // Recasting a hold or curse replaces its visual for the same car.
    if (!effect.charging && (effect.spell == Spell::PetrificusTotalus || effect.spell == Spell::Leviosa || effect.spell == Spell::Obliviate || effect.spell == Spell::Imperio)) {
        effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [&](const Effect& existing) {
            const bool sameTarget = !existing.charging &&
                SameActor(existing.targetId, existing.targetKey, effect.targetId, effect.targetKey) &&
                (effect.spell==Spell::Obliviate?existing.spell==Spell::Obliviate:
                    effect.spell==Spell::Imperio?existing.spell==Spell::Imperio:
                    existing.spell==Spell::PetrificusTotalus||existing.spell==Spell::Leviosa);
            const double physicalDuration = existing.duration > 0 ? existing.duration :
                existing.spell == Spell::Obliviate ? wizard::ObliviateDurationSeconds : existing.spell == Spell::Imperio ? wizard::ImperioDurationSeconds : existing.spell == Spell::Leviosa ? wizard::LeviosaSafetySeconds : wizard::PetrificusDurationSeconds;
            // Preserve an existing hold's brief release fade.
            return sameTarget && existing.began + physicalDuration > effect.began + 0.0001;
        }), effects_.end());
    }
    if (effects_.size() >= 32) effects_.erase(effects_.begin());
    effects_.push_back(effect);
}

void SpellVisuals::StopEffect(Spell spell, int casterId, int targetId, double now, bool shatter,
    std::uint64_t casterKey, std::uint64_t targetKey) {
    if (!std::isfinite(now)) return;
    const auto matchesActor = [](int requestedId, std::uint64_t requestedKey, int existingId, std::uint64_t existingKey) {
        return requestedKey ? requestedKey == existingKey : requestedId < 0 || requestedId == existingId;
    };
    if (spell == Spell::EatSlugs) {
        slugOverlays_.erase(std::remove_if(slugOverlays_.begin(), slugOverlays_.end(), [&](const Effect& existing) {
            return matchesActor(casterId, casterKey, existing.casterId, existing.casterKey) &&
                matchesActor(targetId, targetKey, existing.targetId, existing.targetKey);
        }), slugOverlays_.end());
    }
    for (auto it = effects_.begin(); it != effects_.end();) {
        const bool matches = it->spell == spell && matchesActor(casterId, casterKey, it->casterId, it->casterKey) &&
            matchesActor(targetId, targetKey, it->targetId, it->targetKey);
        if (!matches) { ++it; continue; }
        if (!it->charging && shatter && (spell == Spell::PetrificusTotalus || spell == Spell::Leviosa)) {
            const double physicalDuration = it->duration > 0 ? it->duration :
                spell == Spell::Leviosa ? wizard::LeviosaSafetySeconds : wizard::PetrificusDurationSeconds;
            // Repeated cancellation must not restart a release fade.
            if (now >= it->began + physicalDuration) { ++it; continue; }
            // A small positive duration keeps the original animation clock and
            // starts the thaw/release phase without resurrecting the cast bolt.
            it->duration = std::max(0.001, now - it->began);
            it->snapshot = true;
            ++it;
        } else it = effects_.erase(it);
    }
}

void SpellVisuals::Draw(CanvasWrapper canvas, GameWrapper& game, const Frame& frame, SpellGlow* glow) {
    if (!std::isfinite(frame.time)) return;
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(), [&](const Effect& effect) {
        return frame.time - effect.began > Lifetime(effect) || frame.time < effect.began - 1;
    }), effects_.end());
    slugOverlays_.erase(std::remove_if(slugOverlays_.begin(), slugOverlays_.end(), [&](const Effect& effect) {
        return frame.time - effect.began >= Lifetime(effect) || frame.time < effect.began - 1;
    }), slugOverlays_.end());
    if (frame.spell != lastSpell_) {
        if (frame.spell != Spell::None) revealAt_ = frame.time;
        lastSpell_ = frame.spell;
    }
    // Capture the last held location for the short release fade.
    for (auto& effect : effects_) {
        const double duration = effect.duration > 0 ? effect.duration : wizard::PetrificusDurationSeconds;
        if (frame.time >= effect.began + duration) continue;
        if (effect.spell == Spell::PetrificusTotalus) effect.end = Position(frame, effect.targetId, effect.end, effect.targetKey);
    }
    auto camera = game.GetCamera();
    if (camera && (frame.showEffects || frame.showHud)) {
        const auto rotation = camera.GetRotation();
        const Vec3 forward = Core(RotatorToVector(rotation)).Normalized();
        Vec3 right = Cross({0, 0, 1}, forward).Normalized();
        if (right.LengthSquared() < 0.5f) right = {0, 1, 0};
        const Vec3 up = Cross(forward, right).Normalized();
        WorldCanvas world(canvas, Core(camera.GetLocation()), forward, right, up, glow);
        if (frame.showEffects) {
            if (frame.enabled) {
                double newest = 1;
                for (const auto& effect : effects_) {
                    const double age = frame.time-effect.began;
                    if (effect.spell==Spell::AvadaKedavra && !effect.charging && !effect.snapshot && age>=0)
                        newest=std::min(newest,age);
                }
                AvadaSkyFlash(world,static_cast<float>(newest));
            }
            // Physics barriers have priority over decorative cast trails, so a
            // busy frame can never turn a solid Protego wall invisible.
            int wallCount = 0;
            for (const auto& wall : frame.walls) { if (wallCount++ >= 8) break; Shield(world, wall, frame.time); }
            std::array<const Effect*, 12> casts{};
            int castCount = 0;
            const auto nearby = [&](const Effect& effect) {
                return (effect.end - world.camera).LengthSquared() <= 25000.0f * 25000.0f;
            };
            for (auto it = effects_.rbegin(); it != effects_.rend() && castCount < 12; ++it) {
                const double age = frame.time - it->began;
                const bool channel = it->charging && age < Lifetime(*it);
                const bool fresh = !it->charging && !it->snapshot && it->spell!=Spell::Imperio && age < 1.2;
                const bool slugFlight=!it->charging&&it->spell==Spell::EatSlugs&&age<wizard::SlugFlightSeconds+.85;
                if ((channel || fresh || slugFlight) && age >= 0 && nearby(*it)) casts[castCount++] = &*it;
            }
            std::array<const Patronum*, 8> ghosts{};
            int ghostCount = 0;
            for (const auto& ghost : frame.patronums) {
                if (ghostCount >= 8) break;
                if (VisiblePatronum(ghost, frame.time) &&
                    (ghost.position - world.camera).LengthSquared() <= 25000.0f * 25000.0f)
                    ghosts[ghostCount++] = &ghost;
            }
            // The ball-striking summon has priority over decorative cast
            // ornaments, while leaving a small reserve for held-car feedback.
            int ghostBudget = std::min(1200, std::max(0, world.budget - (castCount ? 144 : 0) - 144));
            for (int i = 0; i < ghostCount && ghostBudget > 0; ++i) {
                const int allowance = std::min(500, ghostBudget / (ghostCount - i));
                const int before = world.budget;
                { VisualBudget slice(world, allowance); PatronumGhost(world, *ghosts[i], frame.time); }
                ghostBudget -= before - world.budget;
            }
            // Preserve held-car feedback while reserving enough strokes for a
            // fresh cast, even with eight simultaneous shield impact ripples.
            const int castReserve = castCount ? 144 : 0;
            int held = 0;
            for (auto it = effects_.rbegin(); it != effects_.rend() && held < 12; ++it) {
                if (it->spell != Spell::PetrificusTotalus || !nearby(*it)) continue;
                const int allowance = std::min(24, world.budget - castReserve);
                if (allowance <= 0) break;
                VisualBudget slice(world, allowance);
                DrawEffect(world, *it, frame, EffectLayer::Status); ++held;
            }
            for (int i = 0; i < castCount && world.budget > 0; ++i) {
                const int share = std::min(520, std::max(72, world.budget / (castCount - i)));
                VisualBudget slice(world, share);
                DrawEffect(world, *casts[i], frame, EffectLayer::Cast);
            }
            // Sustained tethers, levitation and confusion use the remaining budget
            // after new lightning, tethers and shockwaves have been drawn.
            int ornaments = 0;
            for (auto it = effects_.rbegin(); it != effects_.rend() && ornaments < 12 && world.budget > 0; ++it) {
                if ((it->spell != Spell::Accio && it->spell != Spell::Leviosa && it->spell != Spell::Obliviate && it->spell != Spell::Imperio && it->spell != Spell::BombardaMaxima) || !nearby(*it)) continue;
                VisualBudget slice(world, 180);
                DrawEffect(world, *it, frame, EffectLayer::Status); ++ornaments;
            }
        }
        TargetReticle(world, frame);
    }
    // Obstruction is a gameplay effect on the actual victim, so hiding the HUD
    // or cosmetic spell effects never turns it off. The HUD stays readable.
    for (const auto& effect : slugOverlays_) SlugOverlay(canvas, effect, frame);
    if (frame.showHud) Hud(canvas, frame, revealAt_);
}
