#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#if __has_include(<SDL.h>)
#include <SDL.h>
#include <SDL_ttf.h>
#elif __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#else
#error "SDL2 headers were not found. Install sdl2 and sdl2-ttf with vcpkg."
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

// some utilities

constexpr double kPi = 3.14159265358979323846;
constexpr double kGridSize = 20.0;
constexpr double kPinHoverRadius = 7.0;
constexpr double kLogicLowMax = 1.0;
constexpr double kLogicHighMin = 3.0;
constexpr double kLogicHighVoltage = 5.0;
constexpr double kLedThreshold = 2.0;
constexpr double kAmmeterResistance = 1e-4;
constexpr double kEpsilon = 1e-9;

struct Vec2 {
    double x{ 0.0 };
    double y{ 0.0 };

    Vec2() = default;
    Vec2(double px, double py) : x(px), y(py) {}

    Vec2 operator+(const Vec2& rhs) const { 
        return { x + rhs.x, y + rhs.y }; 
    }
    Vec2 operator-(const Vec2& rhs) const { 
        return { x - rhs.x, y - rhs.y }; 
    }
    Vec2 operator*(double s) const { 
        return { x * s, y * s }; 
    }
    Vec2 operator/(double s) const { 
        return { x / s, y / s }; 
    }
    Vec2& operator+=(const Vec2& rhs) { 
        x += rhs.x; y += rhs.y; return *this; 
    }
    Vec2& operator-=(const Vec2& rhs) { 
        x -= rhs.x; y -= rhs.y; return *this; 
    }
};

static double dot(const Vec2& a, const Vec2& b) { 
    return a.x * b.x + a.y * b.y; 
}
static double lengthSquared(const Vec2& v) { 
    return dot(v, v); 
}
static double length(const Vec2& v) { 
    return std::sqrt(lengthSquared(v)); 
}
static double distance(const Vec2& a, const Vec2& b) { 
    return length(a - b); 
}
static bool nearlyEqual(double a, double b, double eps = 1e-6) { 
    return std::abs(a - b) <= eps; 
}
static bool nearlyEqual(const Vec2& a, const Vec2& b, double eps = 1e-6) {
    return nearlyEqual(a.x, b.x, eps) && nearlyEqual(a.y, b.y, eps);
}

template <typename T>
static T clampValue(T value, T lo, T hi) {
    return std::max(lo, std::min(value, hi));
}

static Vec2 snapToGrid(Vec2 p, double grid = kGridSize) {
    return { std::round(p.x / grid) * grid, std::round(p.y / grid) * grid };
}

struct RectD {
    double x{ 0.0 };
    double y{ 0.0 };
    double w{ 0.0 };
    double h{ 0.0 };

    double left() const { 
        return std::min(x, x + w); 
    }
    double right() const { 
        return std::max(x, x + w); 
    }
    double top() const { 
        return std::min(y, y + h); 
    }
    double bottom() const { 
        return std::max(y, y + h); 
    }
    Vec2 center() const { 
        return { (left() + right()) * 0.5, (top() + bottom()) * 0.5 }; 
    }
    bool contains(Vec2 p) const {
        return p.x >= left() && p.x <= right() && p.y >= top() && p.y <= bottom();
    }
    bool intersects(const RectD& other) const {
        return !(right() < other.left() || other.right() < left() || bottom() < other.top() || other.bottom() < top());
    }
};

static RectD normalizedRect(Vec2 a, Vec2 b) {
    return { std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x), std::abs(b.y - a.y) };
}

static double pointSegmentDistance(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const double denom = lengthSquared(ab);
    if (denom < kEpsilon) {
        return distance(p, a);
    }
    const double t = clampValue(dot(p - a, ab) / denom, 0.0, 1.0);
    return distance(p, a + ab * t);
}

struct Color {
    Uint8 r{ 0 }, g{ 0 }, b{ 0 }, a{ 255 };
};

namespace Palette {
    const Color Background{ 255, 192, 203, 255 };
    const Color Panel{ 245, 160, 180, 255 };
    const Color Panel2{ 235, 140, 165, 255 };
    const Color Canvas{ 255, 240, 245, 255 };
    const Color GridMinor{ 245, 215, 225, 255 };
    const Color GridMajor{ 230, 190, 205, 255 };
    const Color Text{ 220, 225, 230, 255 };
    const Color DarkText{ 20, 10, 15, 255 };
    const Color Muted{ 140, 95, 110, 255 };
    const Color Accent{ 219, 112, 147, 255 };
    const Color Accent2{ 255, 105, 180, 255 };
    const Color Warning{ 245, 180, 50, 255 };
    const Color Error{ 235, 80, 75, 255 };
    const Color WireFloat{ 115, 120, 125, 255 };
    const Color WireLow{ 40, 105, 220, 255 };
    const Color WireHigh{ 220, 55, 45, 255 };
    const Color WireAnalog{ 45, 150, 70, 255 };
    const Color Selection{ 255, 20, 147, 255 };
    const Color Pin{ 190, 40, 40, 255 };
    const Color PinHover{ 255, 205, 20, 255 };
}

static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
        });
    return s;
}

static bool containsCaseInsensitive(const std::string& text, const std::string& needle) {
    return toLower(text).find(toLower(needle)) != std::string::npos;
}

static std::string formatDouble(double value, int precision = 6) {
    if (!std::isfinite(value)) return "0";
    std::ostringstream out;
    out << std::setprecision(precision) << std::defaultfloat << value;
    return out.str();
}

static double parseEngineering(const std::string& input, double fallback,
    const std::map<std::string, double>& suffixes = {}) {
    std::string s = toLower(trim(input));
    if (s.empty()) return fallback;
    for (char& c : s) {
        if (static_cast<unsigned char>(c) == 0xB5) c = 'u';
    }
    std::size_t end = 0;
    try {
        const double base = std::stod(s, &end);
        std::string suffix = trim(s.substr(end));
        if (suffix.empty()) return base;
        auto it = suffixes.find(suffix);
        if (it != suffixes.end()) return base * it->second;
        return fallback;
    }
    catch (...) {
        return fallback;
    }
}

static std::string nowTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return out.str();
}

// SDL2 drawing and text helpers

static void setRenderColor(SDL_Renderer* renderer, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

static void drawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color color) {
    setRenderColor(renderer, color);
    for (int dy = -radius; dy <= radius; ++dy) {
        const int dx = static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void drawCircle(SDL_Renderer* renderer, int cx, int cy, int radius, Color color) {
    setRenderColor(renderer, color);
    int x = radius;
    int y = 0;
    int err = 0;
    while (x >= y) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);
        ++y;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) { 
            --x; err -= 2 * x + 1; 
        }
    }
}

static void drawThickLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2,
    int thickness, Color color) {
    setRenderColor(renderer, color);
    if (thickness <= 1) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    const double dx = static_cast<double>(x2 - x1);
    const double dy = static_cast<double>(y2 - y1);
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0) {
        drawFilledCircle(renderer, x1, y1, thickness / 2, color);
        return;
    }
    const double nx = -dy / len;
    const double ny = dx / len;
    const int half = thickness / 2;
    for (int i = -half; i <= half; ++i) {
        SDL_RenderDrawLine(renderer,
            static_cast<int>(std::lround(x1 + nx * i)),
            static_cast<int>(std::lround(y1 + ny * i)),
            static_cast<int>(std::lround(x2 + nx * i)),
            static_cast<int>(std::lround(y2 + ny * i)));
    }
}

static void drawDashedRect(SDL_Renderer* renderer, SDL_Rect rect, Color color, int dash = 6) {
    setRenderColor(renderer, color);
    for (int x = rect.x; x < rect.x + rect.w; x += dash * 2) {
        SDL_RenderDrawLine(renderer, x, rect.y, std::min(x + dash, rect.x + rect.w), rect.y);
        SDL_RenderDrawLine(renderer, x, rect.y + rect.h,
            std::min(x + dash, rect.x + rect.w), rect.y + rect.h);
    }
    for (int y = rect.y; y < rect.y + rect.h; y += dash * 2) {
        SDL_RenderDrawLine(renderer, rect.x, y, rect.x, std::min(y + dash, rect.y + rect.h));
        SDL_RenderDrawLine(renderer, rect.x + rect.w, y, rect.x + rect.w,
            std::min(y + dash, rect.y + rect.h));
    }
}

class FontBook {
public:
    ~FontBook() { clear(); }

    bool initialize() {
        const std::vector<std::string> candidates = {
#ifdef _WIN32
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
#elif __APPLE__
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/SFNS.ttf",
#else
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
#endif
            "DejaVuSans.ttf"
        };
        for (const auto& candidate : candidates) {
            if (fs::exists(candidate)) {
                fontPath_ = candidate;
                return true;
            }
        }
        std::cerr << "Warning: no TrueType font found. UI text may be unavailable.\n";
        return false;
    }

    TTF_Font* get(int size) {
        if (fontPath_.empty()) return nullptr;
        auto it = fonts_.find(size);
        if (it != fonts_.end()) return it->second;
        TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), size);
        if (!font) {
            std::cerr << "TTF_OpenFont failed: " << TTF_GetError() << "\n";
            return nullptr;
        }
        fonts_[size] = font;
        return font;
    }

    void clear() {
        for (auto& [size, font] : fonts_) {
            (void)size;
            if (font) TTF_CloseFont(font);
        }
        fonts_.clear();
    }

private:
    std::string fontPath_;
    std::map<int, TTF_Font*> fonts_;
};

class TextRenderer {
public:
    TextRenderer(SDL_Renderer* renderer, FontBook* fonts) : renderer_(renderer), fonts_(fonts) {}

    void draw(const std::string& text, int x, int y, Color color, int size = 14,
        int maxWidth = 0, bool centered = false) {
        if (text.empty()) return;
        TTF_Font* font = fonts_ ? fonts_->get(size) : nullptr;
        if (!font) return;
        SDL_Color sc{ color.r, color.g, color.b, color.a };
        SDL_Surface* surface = nullptr;
        if (maxWidth > 0) {
            surface = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), sc,
                static_cast<Uint32>(maxWidth));
        }
        else {
            surface = TTF_RenderUTF8_Blended(font, text.c_str(), sc);
        }
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        if (!texture) { SDL_FreeSurface(surface); return; }
        SDL_Rect dst{ x, y, surface->w, surface->h };
        if (centered) dst.x -= dst.w / 2;
        SDL_RenderCopy(renderer_, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }

    std::pair<int, int> measure(const std::string& text, int size = 14) {
        TTF_Font* font = fonts_ ? fonts_->get(size) : nullptr;
        int w = 0, h = 0;
        if (font) TTF_SizeUTF8(font, text.c_str(), &w, &h);
        return { w, h };
    }

private:
    SDL_Renderer* renderer_{ nullptr };
    FontBook* fonts_{ nullptr };
};

struct Camera {
    Vec2 pan{ 0.0, 0.0 };
    double zoom{ 1.0 };
    SDL_Rect viewport{ 0, 0, 100, 100 };

    Vec2 worldToScreen(Vec2 world) const {
        return { viewport.x + pan.x + world.x * zoom,
                viewport.y + pan.y + world.y * zoom };
    }
    Vec2 screenToWorld(Vec2 screen) const {
        return { (screen.x - viewport.x - pan.x) / zoom,
                (screen.y - viewport.y - pan.y) / zoom };
    }
};

class CanvasPainter {
public:
    CanvasPainter(SDL_Renderer* renderer, TextRenderer* text, const Camera* camera)
        : renderer_(renderer), text_(text), camera_(camera) {
    }

    Vec2 screen(Vec2 world) const { return camera_->worldToScreen(world); }

    void line(Vec2 a, Vec2 b, Color color, int thickness = 1) {
        const Vec2 sa = screen(a);
        const Vec2 sb = screen(b);
        drawThickLine(renderer_, static_cast<int>(std::lround(sa.x)), static_cast<int>(std::lround(sa.y)),
            static_cast<int>(std::lround(sb.x)), static_cast<int>(std::lround(sb.y)),
            std::max(1, static_cast<int>(std::lround(thickness * camera_->zoom))), color);
    }

    void circle(Vec2 c, double radius, Color color, bool filled = false) {
        const Vec2 s = screen(c);
        const int r = std::max(1, static_cast<int>(std::lround(radius * camera_->zoom)));
        if (filled) drawFilledCircle(renderer_, static_cast<int>(s.x), static_cast<int>(s.y), r, color);
        else drawCircle(renderer_, static_cast<int>(s.x), static_cast<int>(s.y), r, color);
    }

    void rect(RectD r, Color outline, bool filled = false, Color fill = {}) {
        const Vec2 a = screen({ r.left(), r.top() });
        const Vec2 b = screen({ r.right(), r.bottom() });
        SDL_Rect sr{ static_cast<int>(std::lround(a.x)), static_cast<int>(std::lround(a.y)),
                    static_cast<int>(std::lround(b.x - a.x)), static_cast<int>(std::lround(b.y - a.y)) };
        if (filled) { setRenderColor(renderer_, fill); SDL_RenderFillRect(renderer_, &sr); }
        setRenderColor(renderer_, outline); SDL_RenderDrawRect(renderer_, &sr);
    }

    void polyline(const std::vector<Vec2>& points, Color color, int thickness = 1) {
        for (std::size_t i = 1; i < points.size(); ++i) line(points[i - 1], points[i], color, thickness);
    }

    void textWorld(const std::string& value, Vec2 world, Color color, int size = 12, bool centered = true) {
        if (!text_) return;
        const Vec2 s = screen(world);
        text_->draw(value, static_cast<int>(s.x), static_cast<int>(s.y), color,
            std::max(8, static_cast<int>(std::lround(size * std::sqrt(camera_->zoom)))), 0, centered);
    }

private:
    SDL_Renderer* renderer_{ nullptr };
    TextRenderer* text_{ nullptr };
    const Camera* camera_{ nullptr };
};

//  domain model: pins, components, wires and junctions
using ComponentId = std::uint64_t;
using WireId = std::uint64_t;
using JunctionId = std::uint64_t;

enum class PinType { Input, Output, Bidirectional, Passive, Power, Ground };
enum class LogicState { Low = 0, High = 1, Undefined = -1 };

enum class ComponentCategory {
    Sources,
    Passive,
    Interactive,
    Digital,
    Advanced,
    Peripheral,
    Measurement
};

static std::string categoryName(ComponentCategory category) {
    switch (category) {
    case ComponentCategory::Sources: return "Sources";
    case ComponentCategory::Passive: return "Analog / Passive";
    case ComponentCategory::Interactive: return "Interactive / Output";
    case ComponentCategory::Digital: return "Digital Logic";
    case ComponentCategory::Advanced: return "Converters / MCU";
    case ComponentCategory::Peripheral: return "Peripherals / Memory";
    case ComponentCategory::Measurement: return "Measurement";
    }
    return "Other";
}

struct Pin {
    std::string name;
    PinType type{ PinType::Passive };
    Vec2 localPosition;
    Vec2 worldPosition;
    bool highlighted{ false };
    ComponentId ownerId{ 0 };
    int net{ -1 };
};

struct PropertyDescriptor {
    std::string key;
    std::string label;
    std::string value;
    bool readOnly{ false };
};

class Component {
public:
    explicit Component(std::string typeName, ComponentCategory category)
        : id_(nextId_++), type_(std::move(typeName)), category_(category), label_(type_) {
    }
    virtual ~Component() = default;

    ComponentId id() const { return id_; }
    const std::string& type() const { return type_; }
    ComponentCategory category() const { return category_; }
    const std::string& label() const { return label_; }
    Vec2 position() const { return position_; }
    int rotation() const { return rotation_; }
    bool mirroredHorizontal() const { return mirrorH_; }
    bool mirroredVertical() const { return mirrorV_; }
    const std::vector<std::shared_ptr<Pin>>& pins() const { return pins_; }

    void setIdForLoad(ComponentId id) {
        id_ = id;
        nextId_ = std::max(nextId_, id_ + 1);
        for (auto& pin : pins_) if (pin) pin->ownerId = id_;
    }

    void setLabel(std::string value) { label_ = std::move(value); }
    void setTransformForLoad(Vec2 p, int rotation, bool mirrorH, bool mirrorV) {
        position_ = p; rotation_ = ((rotation % 360) + 360) % 360; mirrorH_ = mirrorH; mirrorV_ = mirrorV; updatePinWorldPositions();
    }
    void moveTo(Vec2 newPosition) { position_ = newPosition; updatePinWorldPositions(); }
    void rotate90() { rotation_ = (rotation_ + 90) % 360; updatePinWorldPositions(); }
    void mirrorHorizontal() { mirrorH_ = !mirrorH_; updatePinWorldPositions(); }
    void mirrorVertical() { mirrorV_ = !mirrorV_; updatePinWorldPositions(); }

    std::shared_ptr<Pin> pinByName(const std::string& name) const {
        for (const auto& pin : pins_) if (pin && pin->name == name) return pin;
        return nullptr;
    }

    Vec2 transformLocal(Vec2 local) const {
        if (mirrorH_) local.x = -local.x;
        if (mirrorV_) local.y = -local.y;
        const double radians = rotation_ * kPi / 180.0;
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        return position_ + Vec2{ local.x * c - local.y * s, local.x * s + local.y * c };
    }

    Vec2 inverseTransform(Vec2 world) const {
        Vec2 local = world - position_;
        const double radians = -rotation_ * kPi / 180.0;
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        local = { local.x * c - local.y * s, local.x * s + local.y * c };
        if (mirrorH_) local.x = -local.x;
        if (mirrorV_) local.y = -local.y;
        return local;
    }

    virtual RectD localBounds() const { return { -32.0, -22.0, 64.0, 44.0 }; }

    RectD worldBounds() const {
        const RectD b = localBounds();
        const std::array<Vec2, 4> corners = {
            transformLocal({b.left(), b.top()}), transformLocal({b.right(), b.top()}),
            transformLocal({b.right(), b.bottom()}), transformLocal({b.left(), b.bottom()})
        };
        double minX = corners[0].x, maxX = corners[0].x;
        double minY = corners[0].y, maxY = corners[0].y;
        for (const auto& p : corners) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        return { minX, minY, maxX - minX, maxY - minY };
    }

    virtual bool hitTest(Vec2 world) const { return localBounds().contains(inverseTransform(world)); }

    virtual void draw(CanvasPainter& painter, bool selected) const;

    virtual std::vector<PropertyDescriptor> properties() const {
        return { {"label", "Label", label_, false} };
    }

    virtual bool setProperty(const std::string& key, const std::string& value) {
        if (key == "label") { label_ = value; return true; }
        return false;
    }

    virtual std::map<std::string, std::string> persistentState() const {
        return { {"label", label_} };
    }

    virtual void loadPersistentState(const std::map<std::string, std::string>& state) {
        auto it = state.find("label");
        if (it != state.end()) label_ = it->second;
    }

    virtual std::string compactStatus() const { return {}; }

protected:
    void addPin(std::string name, PinType type, Vec2 local) {
        auto pin = std::make_shared<Pin>();
        pin->name = std::move(name);
        pin->type = type;
        pin->localPosition = local;
        pin->ownerId = id_;
        pins_.push_back(std::move(pin));
    }

    void replacePins(std::vector<std::shared_ptr<Pin>> newPins) {
        pins_ = std::move(newPins);
        for (auto& pin : pins_) if (pin) pin->ownerId = id_;
        updatePinWorldPositions();
    }

    void updatePinWorldPositions() {
        for (auto& pin : pins_) if (pin) pin->worldPosition = transformLocal(pin->localPosition);
    }

    void drawPins(CanvasPainter& painter) const {
        for (const auto& pin : pins_) {
            if (!pin) continue;
            painter.circle(pin->worldPosition, 3.2,
                pin->highlighted ? Palette::PinHover : Palette::Pin, true);
        }
    }

    ComponentId id_{ 0 };
    std::string type_;
    ComponentCategory category_{ ComponentCategory::Passive };
    std::string label_;
    Vec2 position_{ 0.0, 0.0 };
    int rotation_{ 0 };
    bool mirrorH_{ false };
    bool mirrorV_{ false };
    std::vector<std::shared_ptr<Pin>> pins_;

private:
    inline static ComponentId nextId_{ 1 };
};

// common drawable component. individual device classes inherit this and add domain-specific behavior/state while sharing a clean schematic symbol renderer

class GenericComponent : public Component {
public:
    GenericComponent(std::string typeName, ComponentCategory category)
        : Component(std::move(typeName), category) {
    }
    void draw(CanvasPainter& painter, bool selected) const override;
};

// Source components
class Ground final : public GenericComponent {
public:
    Ground() : GenericComponent("Ground", ComponentCategory::Sources) {
        setLabel("GND");
        addPin("GND", PinType::Ground, { 0, -20 });
        updatePinWorldPositions();
    }
    RectD localBounds() const override { return { -18, -24, 36, 40 }; }
};

class DCVoltageSource : public GenericComponent {
public:
    DCVoltageSource() : GenericComponent("DCVoltageSource", ComponentCategory::Sources) {
        setLabel("V?");
        addPin("POS", PinType::Power, { 0, -32 });
        addPin("NEG", PinType::Passive, { 0, 32 });
        updatePinWorldPositions();
    }
    double voltage() const { return voltage_; }
    RectD localBounds() const override { return { -24, -36, 48, 72 }; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false},
                {"voltage", "Voltage (V)", formatDouble(voltage_), false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "voltage") { voltage_ = std::stod(value); return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["voltage"] = formatDouble(voltage_, 12); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("voltage"); it != s.end()) voltage_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(voltage_) + " V"; }
protected:
    double voltage_{ 5.0 };
};

class Battery final : public DCVoltageSource {
public:
    Battery() : DCVoltageSource() {
        type_ = "Battery";
        category_ = ComponentCategory::Sources;
        setLabel("BAT?");
        voltage_ = 9.0;
    }
};
class ClockGenerator final : public GenericComponent {
public:
    ClockGenerator() : GenericComponent("ClockGenerator", ComponentCategory::Sources) {
        setLabel("CLK?");
        addPin("OUT", PinType::Output, { 34, 0 });
        updatePinWorldPositions();
    }
    void tick(double dt) {
        if (frequency_ <= 0.0) return;
        accumulator_ += dt;
        const double halfPeriod = 0.5 / frequency_;
        while (accumulator_ >= halfPeriod) {
            accumulator_ -= halfPeriod;
            output_ = !output_;
        }
    }
    bool output() const { return output_; }
    double frequency() const { return frequency_; }
    void reset() { accumulator_ = 0.0; output_ = false; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false},
                {"frequency", "Frequency (Hz)", formatDouble(frequency_), false},
                {"output", "Current output", output_ ? "HIGH" : "LOW", true} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "frequency") { frequency_ = std::max(0.001, std::stod(value)); return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["frequency"] = formatDouble(frequency_, 12);
        s["accumulator"] = formatDouble(accumulator_, 12);
        s["output"] = output_ ? "1" : "0";
        return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("frequency"); it != s.end()) frequency_ = std::stod(it->second);
        if (auto it = s.find("accumulator"); it != s.end()) accumulator_ = std::stod(it->second);
        if (auto it = s.find("output"); it != s.end()) output_ = it->second == "1";
    }
    std::string compactStatus() const override {
        return formatDouble(frequency_) + " Hz  " + (output_ ? "HIGH" : "LOW");
    }
private:
    double frequency_{ 1.0 };
    double accumulator_{ 0.0 };
    bool output_{ false };
};
// Passive and interactive components
class Resistor final : public GenericComponent {
public:
    Resistor() : GenericComponent("Resistor", ComponentCategory::Passive) {
        setLabel("R?"); addPin("A", PinType::Passive, { -34, 0 }); addPin("B", PinType::Passive, { 34, 0 });
        updatePinWorldPositions();
    }
    double resistance() const { return resistance_; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false},
                {"resistance", "Resistance (ohm)", formatDouble(resistance_), false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "resistance") {
            const std::map<std::string, double> suffixes{ {"k", 1e3}, {"kohm", 1e3}, {"m", 1e6}, {"mohm", 1e6}, {"ohm", 1.0} };
            resistance_ = std::max(1e-9, parseEngineering(value, resistance_, suffixes)); return true;
        }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["resistance"] = formatDouble(resistance_, 12); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("resistance"); it != s.end()) resistance_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(resistance_) + " ohm"; }
private:
    double resistance_{ 1000.0 };
};

class Capacitor final : public GenericComponent {
public:
    Capacitor() : GenericComponent("Capacitor", ComponentCategory::Passive) {
        setLabel("C?"); addPin("A", PinType::Passive, { -34, 0 }); addPin("B", PinType::Passive, { 34, 0 });
        updatePinWorldPositions();
    }
    double capacitance() const { return capacitance_; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false},
                {"capacitance", "Capacitance (F/nF/uF)", formatDouble(capacitance_ * 1e9) + " nF", false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "capacitance") {
            const std::map<std::string, double> suffixes{ {"f", 1.0}, {"mf", 1e-3}, {"uf", 1e-6}, {"nf", 1e-9}, {"pf", 1e-12} };
            capacitance_ = std::max(0.0, parseEngineering(value, capacitance_, suffixes)); return true;
        }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["capacitance"] = formatDouble(capacitance_, 14); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("capacitance"); it != s.end()) capacitance_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(capacitance_ * 1e9) + " nF"; }
private:
    double capacitance_{ 100e-9 };
};

class Inductor final : public GenericComponent {
public:
    Inductor() : GenericComponent("Inductor", ComponentCategory::Passive) {
        setLabel("L?"); addPin("A", PinType::Passive, { -34, 0 }); addPin("B", PinType::Passive, { 34, 0 });
        updatePinWorldPositions();
    }
    double inductance() const { return inductance_; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false},
                {"inductance", "Inductance (H/mH/uH)", formatDouble(inductance_ * 1e3) + " mH", false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "inductance") {
            const std::map<std::string, double> suffixes{ {"h", 1.0}, {"mh", 1e-3}, {"uh", 1e-6}, {"nh", 1e-9} };
            inductance_ = std::max(0.0, parseEngineering(value, inductance_, suffixes)); return true;
        }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["inductance"] = formatDouble(inductance_, 14); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("inductance"); it != s.end()) inductance_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(inductance_ * 1e3) + " mH"; }
private:
    double inductance_{ 1e-3 };
};
class Potentiometer final : public GenericComponent {
public:
    Potentiometer() : GenericComponent("Potentiometer", ComponentCategory::Passive) {
        setLabel("RV?");
        addPin("A", PinType::Passive, { -36, 0 });
        addPin("B", PinType::Passive, { 36, 0 });
        addPin("W", PinType::Passive, { 0, -30 });
        updatePinWorldPositions();
    }
    double resistance() const { return resistance_; }
    double wiper() const { return wiper_; }
    void adjustWiper(double delta) { wiper_ = clampValue(wiper_ + delta, 0.002, 0.998); }
    RectD localBounds() const override { return { -40, -34, 80, 58 }; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false},
                {"resistance", "Total resistance (ohm)", formatDouble(resistance_), false},
                {"wiper", "Wiper (0..1)", formatDouble(wiper_), false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "resistance") { resistance_ = std::max(1.0, std::stod(value)); return true; }
        if (key == "wiper") { wiper_ = clampValue(std::stod(value), 0.002, 0.998); return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["resistance"] = formatDouble(resistance_, 12);
        s["wiper"] = formatDouble(wiper_, 12);
        return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("resistance"); it != s.end()) resistance_ = std::stod(it->second);
        if (auto it = s.find("wiper"); it != s.end()) wiper_ = clampValue(std::stod(it->second), 0.002, 0.998);
    }
    std::string compactStatus() const override {
        return formatDouble(resistance_) + " ohm, w=" + formatDouble(wiper_, 3);
    }
private:
    double resistance_{ 10000.0 };
    double wiper_{ 0.5 };
};

class Switch final : public GenericComponent {
public:
    Switch() : GenericComponent("Switch", ComponentCategory::Interactive) {
        setLabel("SW?");
        addPin("A", PinType::Passive, { -32, 0 });
        addPin("B", PinType::Passive, { 32, 0 });
        updatePinWorldPositions();
    }
    bool closed() const { return closed_; }
    void toggle() { closed_ = !closed_; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"closed", "Closed", closed_ ? "true" : "false", false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "closed") { closed_ = toLower(value) == "true" || value == "1"; return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["closed"] = closed_ ? "1" : "0";
        return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("closed"); it != s.end()) closed_ = it->second == "1";
    }
    std::string compactStatus() const override { return closed_ ? "CLOSED" : "OPEN"; }
private:
    bool closed_{ false };
};

class PushButton final : public GenericComponent {
public:
    PushButton() : GenericComponent("PushButton", ComponentCategory::Interactive) {
        setLabel("BTN?");
        addPin("A", PinType::Passive, { -32, 0 });
        addPin("B", PinType::Passive, { 32, 0 });
        updatePinWorldPositions();
    }
    bool pressed() const { return pressed_; }
    void setPressed(bool pressed) { pressed_ = pressed; }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["pressed"] = pressed_ ? "1" : "0";
        return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("pressed"); it != s.end()) pressed_ = it->second == "1";
    }
    std::string compactStatus() const override { return pressed_ ? "PRESSED" : "RELEASED"; }
private:
    bool pressed_{ false };
};

class LED final : public GenericComponent {
public:
    LED() : GenericComponent("LED", ComponentCategory::Interactive) {
        setLabel("D?");
        addPin("A", PinType::Input, { -30, 0 });
        addPin("K", PinType::Passive, { 30, 0 });
        updatePinWorldPositions();
    }
    bool on() const { return on_; }
    void setOn(bool on) { on_ = on; }
    Color ledColor() const { return color_; }
    std::vector<PropertyDescriptor> properties() const override {
        std::ostringstream c;
        c << static_cast<int>(color_.r) << "," << static_cast<int>(color_.g) << "," << static_cast<int>(color_.b);
        return { {"label", "Label", label_, false}, {"color", "Color R,G,B", c.str(), false},
                {"on", "Current state", on_ ? "ON" : "OFF", true} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "color") {
            int r = 255, g = 0, b = 0;
            char comma;
            std::istringstream in(value);
            if (in >> r >> comma >> g >> comma >> b)
                color_ = { static_cast<Uint8>(clampValue(r, 0, 255)), static_cast<Uint8>(clampValue(g, 0, 255)), static_cast<Uint8>(clampValue(b, 0, 255)), 255 };
            return true;
        }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["on"] = on_ ? "1" : "0";
        s["r"] = std::to_string(color_.r);
        s["g"] = std::to_string(color_.g);
        s["b"] = std::to_string(color_.b);
        return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("on"); it != s.end()) on_ = it->second == "1";
        if (auto it = s.find("r"); it != s.end()) color_.r = static_cast<Uint8>(std::stoi(it->second));
        if (auto it = s.find("g"); it != s.end()) color_.g = static_cast<Uint8>(std::stoi(it->second));
        if (auto it = s.find("b"); it != s.end()) color_.b = static_cast<Uint8>(std::stoi(it->second));
    }
    std::string compactStatus() const override { return on_ ? "ON" : "OFF"; }
private:
    bool on_{ false };
    Color color_{ 245, 45, 35, 255 };
};

class SevenSegment final : public GenericComponent {
public:
    SevenSegment() : GenericComponent("SevenSegment", ComponentCategory::Interactive) {
        setLabel("7SEG?");
        const std::array<const char*, 8> names{ {"A", "B", "C", "D", "E", "F", "G", "DP"} };
        for (int i = 0; i < 8; ++i) addPin(names[i], PinType::Input, { -46 + i * 13.0, 38 });
        addPin("COM", PinType::Passive, { 46, 0 });
        updatePinWorldPositions();
    }
    void setMask(std::uint8_t mask) { mask_ = mask; }
    std::uint8_t mask() const { return mask_; }
    RectD localBounds() const override { return { -50, -42, 100, 84 }; }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["mask"] = std::to_string(mask_); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s); if (auto it = s.find("mask"); it != s.end()) mask_ = static_cast<std::uint8_t>(std::stoi(it->second));
    }
private:
    std::uint8_t mask_{ 0x3F };
};

// Digital logic
enum class GateKind { And, Or, Not, Xor, Nand };

class LogicGate : public GenericComponent {
public:
    LogicGate(std::string typeName, GateKind kind, int inputCount)
        : GenericComponent(std::move(typeName), ComponentCategory::Digital), kind_(kind), inputCount_(std::max(1, inputCount)) {
        rebuildPins();
    }

    int inputCount() const { return inputCount_; }
    LogicState outputState() const { return outputValid_ ? (output_ ? LogicState::High : LogicState::Low) : LogicState::Undefined; }
    bool output() const { return output_; }
    bool outputValid() const { return outputValid_; }
    double propagationDelayMs() const { return propagationDelayMs_; }

    void evaluate(const std::vector<LogicState>& inputs, double dt) {
        if (static_cast<int>(inputs.size()) < inputCount_ ||
            std::any_of(inputs.begin(), inputs.begin() + inputCount_, [](LogicState s) { return s == LogicState::Undefined; })) {
            outputValid_ = false;
            transition_.active = false;
            return;
        }
        bool result = false;
        switch (kind_) {
        case GateKind::And:
        case GateKind::Nand:
            result = true;
            for (int i = 0; i < inputCount_; ++i) result = result && inputs[i] == LogicState::High;
            if (kind_ == GateKind::Nand) result = !result;
            break;
        case GateKind::Or:
            for (int i = 0; i < inputCount_; ++i) result = result || inputs[i] == LogicState::High;
            break;
        case GateKind::Not:
            result = inputs[0] != LogicState::High;
            break;
        case GateKind::Xor: {
            int count = 0;
            for (int i = 0; i < inputCount_; ++i) count += inputs[i] == LogicState::High ? 1 : 0;
            result = (count % 2) != 0;
            break;
        }
        }
        applyTarget(result, dt);
    }

    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false},
                {"inputCount", "Input count", std::to_string(inputCount_), kind_ == GateKind::Not},
                {"propagationDelayMs", "Propagation delay (ms)", formatDouble(propagationDelayMs_), false},
                {"output", "Output", outputValid_ ? (output_ ? "HIGH" : "LOW") : "Undefined", true} };
    }

    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "inputCount" && kind_ != GateKind::Not) {
            inputCount_ = clampValue(std::stoi(value), 1, 8); rebuildPins(); return true;
        }
        if (key == "propagationDelayMs") {
            propagationDelayMs_ = std::max(0.0, std::stod(value)); return true;
        }
        return false;
    }

    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["inputCount"] = std::to_string(inputCount_);
        s["propagationDelayMs"] = formatDouble(propagationDelayMs_, 12);
        s["output"] = output_ ? "1" : "0";
        s["outputValid"] = outputValid_ ? "1" : "0";
        return s;
    }

    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("inputCount"); it != s.end()) inputCount_ = clampValue(std::stoi(it->second), 1, 8);
        rebuildPins();
        if (auto it = s.find("propagationDelayMs"); it != s.end()) propagationDelayMs_ = std::stod(it->second);
        if (auto it = s.find("output"); it != s.end()) output_ = it->second == "1";
        if (auto it = s.find("outputValid"); it != s.end()) outputValid_ = it->second == "1";
    }

    std::string gateText() const {
        switch (kind_) {
        case GateKind::And: return "&";
        case GateKind::Or: return ">=1";
        case GateKind::Not: return "1";
        case GateKind::Xor: return "=1";
        case GateKind::Nand: return "N&";
        }
        return "?";
    }

private:
    void rebuildPins() {
        std::vector<std::shared_ptr<Pin>> pins;
        auto out = std::make_shared<Pin>();
        out->name = "OUT"; out->type = PinType::Output; out->localPosition = { 36, 0 }; pins.push_back(out);
        const double spacing = inputCount_ > 1 ? 18.0 : 0.0;
        const double startY = -(inputCount_ - 1) * spacing * 0.5;
        for (int i = 0; i < inputCount_; ++i) {
            auto pin = std::make_shared<Pin>();
            pin->name = "IN" + std::to_string(i + 1); pin->type = PinType::Input;
            pin->localPosition = { -36, startY + i * spacing }; pins.push_back(pin);
        }
        replacePins(std::move(pins));
    }

    void applyTarget(bool value, double dt) {
        if (propagationDelayMs_ <= 0.0 || !outputValid_) {
            output_ = value;
            outputValid_ = true;
            transition_.active = false;
            return;
        }

        if (value == output_) {
            transition_.active = false;
            outputValid_ = true;
            return;
        }

        if (!transition_.active || transition_.targetValue != value) {
            transition_.active = true;
            transition_.targetValue = value;
            transition_.remainingMs = propagationDelayMs_;
        }
        else {
            transition_.remainingMs -= std::max(0.0, dt) * 1000.0;
            if (transition_.remainingMs <= 0.0) {
                output_ = transition_.targetValue;
                outputValid_ = true;
                transition_.active = false;
            }
        }
    }

    struct TransitionEvent {
        bool active{ false };
        bool targetValue{ false };
        double remainingMs{ 0.0 };
    };

    GateKind kind_{ GateKind::And };
    int inputCount_{ 2 };
    bool output_{ false };
    bool outputValid_{ true };
    double propagationDelayMs_{ 1.0 };

    TransitionEvent transition_;
};

class AndGate final : public LogicGate {
public: AndGate() : LogicGate("AndGate", GateKind::And, 2) { setLabel("AND"); }
};
class OrGate final : public LogicGate {
public: OrGate() : LogicGate("OrGate", GateKind::Or, 2) { setLabel("OR"); }
};
class NotGate final : public LogicGate {
public: NotGate() : LogicGate("NotGate", GateKind::Not, 1) { setLabel("NOT"); }
};
class XorGate final : public LogicGate {
public: XorGate() : LogicGate("XorGate", GateKind::Xor, 2) { setLabel("XOR"); }
};
class NandGate final : public LogicGate {
public: NandGate() : LogicGate("NandGate", GateKind::Nand, 2) { setLabel("NAND"); }
};

class DFlipFlop final : public GenericComponent {
public:
    DFlipFlop() : GenericComponent("DFlipFlop", ComponentCategory::Digital) {
        setLabel("DFF?");
        addPin("D", PinType::Input, { -38, -11 }); addPin("CLK", PinType::Input, { -38, 12 });
        addPin("Q", PinType::Output, { 38, -11 }); addPin("QB", PinType::Output, { 38, 12 });
        updatePinWorldPositions();
    }
    void update(LogicState d, LogicState clock) {
        if (d == LogicState::Undefined || clock == LogicState::Undefined) {
            valid_ = false; previousClock_ = clock; return;
        }
        const bool clk = clock == LogicState::High;
        const bool rising = previousClock_ == LogicState::Low && clk;
        if (rising) { q_ = d == LogicState::High; valid_ = true; }
        previousClock_ = clock;
    }
    bool q() const { return q_; }
    bool valid() const { return valid_; }
    RectD localBounds() const override { return { -42, -30, 84, 60 }; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"Q", "Q", valid_ ? (q_ ? "HIGH" : "LOW") : "Undefined", true} };
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["q"] = q_ ? "1" : "0"; s["valid"] = valid_ ? "1" : "0";
        s["previousClock"] = std::to_string(static_cast<int>(previousClock_)); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("q"); it != s.end()) q_ = it->second == "1";
        if (auto it = s.find("valid"); it != s.end()) valid_ = it->second == "1";
        if (auto it = s.find("previousClock"); it != s.end()) previousClock_ = static_cast<LogicState>(std::stoi(it->second));
    }
    std::string compactStatus() const override { return valid_ ? (q_ ? "Q=HIGH" : "Q=LOW") : "Q=Undefined"; }
private:
    bool q_{ false };
    bool valid_{ true };
    LogicState previousClock_{ LogicState::Low };
};
// ADC and DAC
class SimpleADC final : public GenericComponent {
public:
    SimpleADC() : GenericComponent("SimpleADC", ComponentCategory::Advanced) {
        setLabel("ADC?"); rebuildPins();
    }
    int bits() const { return bits_; }
    std::uint32_t outputCode() const { return outputCode_; }
    void setAnalogInput(double input, double vMinus, double vPlus) {
        inputVoltage_ = input; vrefMinus_ = vMinus; vrefPlus_ = vPlus;
    }
    void tick(double dt) {
        const double span = vrefPlus_ - vrefMinus_;
        std::uint32_t target = 0;
        if (span > 1e-12) {
            const double normalized = clampValue((inputVoltage_ - vrefMinus_) / span, 0.0, 1.0);
            const std::uint32_t maxCode = (bits_ >= 31) ? std::numeric_limits<std::uint32_t>::max() : ((1u << bits_) - 1u);
            target = static_cast<std::uint32_t>(std::llround(normalized * maxCode));
        }

        if (!task_.active || task_.targetCode != target) {
            task_.active = true;
            task_.targetCode = target;
            task_.timeRemainingMs = conversionDelayMs_;
        }

        if (task_.active) {
            task_.timeRemainingMs -= std::max(0.0, dt) * 1000.0;
            if (task_.timeRemainingMs <= 0.0) {
                outputCode_ = task_.targetCode;
                task_.active = false;
            }
        }
    }
    RectD localBounds() const override { return { -54, -44, 108, 88 }; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"bits", "Output bits", std::to_string(bits_), false},
                {"conversionDelayMs", "Conversion delay (ms)", formatDouble(conversionDelayMs_), false},
                {"inputVoltage", "VIN", formatDouble(inputVoltage_), true},
                {"outputCode", "Code", std::to_string(outputCode_), true} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "bits") { bits_ = clampValue(std::stoi(value), 1, 16); rebuildPins(); return true; }
        if (key == "conversionDelayMs") { conversionDelayMs_ = std::max(0.0, std::stod(value)); return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["bits"] = std::to_string(bits_);
        s["conversionDelayMs"] = formatDouble(conversionDelayMs_, 12); s["outputCode"] = std::to_string(outputCode_); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("bits"); it != s.end()) bits_ = clampValue(std::stoi(it->second), 1, 16);
        rebuildPins();
        if (auto it = s.find("conversionDelayMs"); it != s.end()) conversionDelayMs_ = std::stod(it->second);
        if (auto it = s.find("outputCode"); it != s.end()) outputCode_ = static_cast<std::uint32_t>(std::stoul(it->second));
    }
    std::string compactStatus() const override { return "code=" + std::to_string(outputCode_); }
private:
    void rebuildPins() {
        std::vector<std::shared_ptr<Pin>> pins;
        auto make = [&](std::string name, PinType type, Vec2 local) {
            auto p = std::make_shared<Pin>(); p->name = std::move(name); p->type = type; p->localPosition = local; pins.push_back(p);
            };
        make("VIN", PinType::Input, { -52, -22 }); make("VREF+", PinType::Input, { -52, 0 }); make("VREF-", PinType::Input, { -52, 22 });
        const double startY = -(bits_ - 1) * 7.0 * 0.5;
        for (int i = 0; i < bits_; ++i) make("D" + std::to_string(i), PinType::Output, { 52, startY + i * 7.0 });
        replacePins(std::move(pins));
    }

    struct ConversionTask {
        bool active{ false };
        std::uint32_t targetCode{ 0 };
        double timeRemainingMs{ 0.0 };
    };

    int bits_{ 8 };
    double conversionDelayMs_{ 1.0 };
    double inputVoltage_{ 0.0 };
    double vrefMinus_{ 0.0 };
    double vrefPlus_{ 5.0 };
    std::uint32_t outputCode_{ 0 };

    ConversionTask task_;
};

class SimpleDAC final : public GenericComponent {
public:
    SimpleDAC() : GenericComponent("SimpleDAC", ComponentCategory::Advanced) {
        setLabel("DAC?"); rebuildPins();
    }
    int bits() const { return bits_; }
    double analogOutput() const { return outputVoltage_; }
    void setDigitalInput(std::uint32_t code, double vMinus, double vPlus) {
        inputCode_ = code; vrefMinus_ = vMinus; vrefPlus_ = vPlus;
    }
    void tick(double dt) {
        const std::uint32_t maxCode = (1u << bits_) - 1u;
        const double target = vrefMinus_ + (vrefPlus_ - vrefMinus_) * (maxCode ? static_cast<double>(inputCode_ & maxCode) / maxCode : 0.0);

        if (!event_.active || std::abs(target - event_.targetVoltage) > 1e-12) {
            event_.active = true;
            event_.targetVoltage = target;
            event_.countdownMs = conversionDelayMs_;
        }

        if (event_.active) {
            event_.countdownMs -= std::max(0.0, dt) * 1000.0;
            if (event_.countdownMs <= 0.0) {
                outputVoltage_ = event_.targetVoltage;
                event_.active = false;
            }
        }
    }
    RectD localBounds() const override { return { -54, -44, 108, 88 }; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"bits", "Input bits", std::to_string(bits_), false},
                {"conversionDelayMs", "Conversion delay (ms)", formatDouble(conversionDelayMs_), false},
                {"outputVoltage", "VOUT", formatDouble(outputVoltage_), true} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "bits") { bits_ = clampValue(std::stoi(value), 1, 16); rebuildPins(); return true; }
        if (key == "conversionDelayMs") { conversionDelayMs_ = std::max(0.0, std::stod(value)); return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["bits"] = std::to_string(bits_);
        s["conversionDelayMs"] = formatDouble(conversionDelayMs_, 12); s["outputVoltage"] = formatDouble(outputVoltage_, 12); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("bits"); it != s.end()) bits_ = clampValue(std::stoi(it->second), 1, 16);
        rebuildPins();
        if (auto it = s.find("conversionDelayMs"); it != s.end()) conversionDelayMs_ = std::stod(it->second);
        if (auto it = s.find("outputVoltage"); it != s.end()) outputVoltage_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(outputVoltage_) + " V"; }
private:
    void rebuildPins() {
        std::vector<std::shared_ptr<Pin>> pins;
        auto make = [&](std::string name, PinType type, Vec2 local) {
            auto p = std::make_shared<Pin>(); p->name = std::move(name); p->type = type; p->localPosition = local; pins.push_back(p);
            };
        const double startY = -(bits_ - 1) * 7.0 * 0.5;
        for (int i = 0; i < bits_; ++i) make("D" + std::to_string(i), PinType::Input, { -52, startY + i * 7.0 });
        make("VREF+", PinType::Input, { 0, -44 }); make("VREF-", PinType::Input, { 0, 44 }); make("VOUT", PinType::Output, { 52, 0 });
        replacePins(std::move(pins));
    }

    struct OutputEvent {
        bool active{ false };
        double targetVoltage{ 0.0 };
        double countdownMs{ 0.0 };
    };

    int bits_{ 8 };
    double conversionDelayMs_{ 1.0 };
    std::uint32_t inputCode_{ 0 };
    double vrefMinus_{ 0.0 };
    double vrefPlus_{ 5.0 };
    double outputVoltage_{ 0.0 };

    OutputEvent event_;
};

// wire model, orthogonal routing, wire lookup, add, remove and reroute

class Wire {
public:
    Wire() : id_(nextId_++) {}
    Wire(std::shared_ptr<Pin> start, std::shared_ptr<Pin> end)
        : id_(nextId_++), start_(std::move(start)), end_(std::move(end)) {
        routeSimple();
    }

    WireId id() const { return id_; }
    void setIdForLoad(WireId id) { id_ = id; nextId_ = std::max(nextId_, id_ + 1); }
    const std::shared_ptr<Pin>& startPin() const { return start_; }
    const std::shared_ptr<Pin>& endPin() const { return end_; }
    void setPins(std::shared_ptr<Pin> start, std::shared_ptr<Pin> end) { start_ = std::move(start); end_ = std::move(end); }
    const std::vector<Vec2>& path() const { return path_; }
    void setPath(std::vector<Vec2> path, bool manual = true) {
        path_ = cleanPath(std::move(path));
        manualRoute_ = manual;
    }
    bool selected() const { return selected_; }
    void setSelected(bool selected) { selected_ = selected; }
    bool manualRoute() const { return manualRoute_; }

    static std::vector<Vec2> orthogonalPath(Vec2 start, const std::vector<Vec2>& waypoints, Vec2 end) {
        std::vector<Vec2> routed;
        routed.reserve(2 + waypoints.size() * 2);
        routed.push_back(start);

        for (const Vec2& waypoint : waypoints) appendOrthogonalTarget(routed, waypoint);
        appendOrthogonalTarget(routed, end);

        return cleanPath(std::move(routed));
    }

    void routeSimple() {
        manualRoute_ = false;
        path_.clear();
        if (!hasEndpoints()) return;
        path_ = orthogonalPath(start_->worldPosition, {}, end_->worldPosition);
    }

    void reroutePreservingWaypoints() {
        if (!hasEndpoints()) return;
        if (!manualRoute_) {
            routeSimple();
            return;
        }

        std::vector<Vec2> preservedWaypoints;
        if (path_.size() > 2) {
            preservedWaypoints.assign(path_.begin() + 1, path_.end() - 1);
        }

        path_ = orthogonalPath(start_->worldPosition, preservedWaypoints, end_->worldPosition);
        manualRoute_ = true;
    }

    bool hitTest(Vec2 world, double tolerance = 5.0) const {
        for (std::size_t i = 1; i < path_.size(); ++i) {
            if (pointSegmentDistance(world, path_[i - 1], path_[i]) <= tolerance) return true;
        }
        return false;
    }

private:
    static constexpr double kRouteTolerance = 0.2;

    bool hasEndpoints() const { return static_cast<bool>(start_) && static_cast<bool>(end_); }

    static bool samePoint(const Vec2& a, const Vec2& b) {
        return nearlyEqual(a, b, kRouteTolerance);
    }

    static bool sameVerticalLine(const Vec2& a, const Vec2& b) {
        return nearlyEqual(a.x, b.x, kRouteTolerance);
    }

    static bool sameHorizontalLine(const Vec2& a, const Vec2& b) {
        return nearlyEqual(a.y, b.y, kRouteTolerance);
    }

    static void appendOrthogonalTarget(std::vector<Vec2>& routed, Vec2 target) {
        if (routed.empty()) {
            routed.push_back(target);
            return;
        }

        const Vec2 current = routed.back();
        if (samePoint(current, target)) {
            return;
        }

        if (!sameVerticalLine(current, target) && !sameHorizontalLine(current, target)) {
            routed.push_back({ target.x, current.y });
        }
        routed.push_back(target);
    }

    static bool middlePointIsRedundant(const Vec2& a, const Vec2& b, const Vec2& c) {
        const bool vertical = sameVerticalLine(a, b) && sameVerticalLine(b, c);
        const bool horizontal = sameHorizontalLine(a, b) && sameHorizontalLine(b, c);
        return vertical || horizontal;
    }

    static std::vector<Vec2> cleanPath(std::vector<Vec2> points) {
        std::vector<Vec2> compact;
        compact.reserve(points.size());

        for (const Vec2& point : points) {
            if (!compact.empty() && samePoint(compact.back(), point)) {
                continue;
            }
            compact.push_back(point);

            while (compact.size() >= 3) {
                const std::size_t n = compact.size();
                if (!middlePointIsRedundant(compact[n - 3], compact[n - 2], compact[n - 1])) {
                    break;
                }
                compact.erase(compact.end() - 2);
            }
        }
        return compact;
    }

    inline static WireId nextId_{ 1 };
    WireId id_{ 0 };
    std::shared_ptr<Pin> start_;
    std::shared_ptr<Pin> end_;
    std::vector<Vec2> path_;
    bool selected_{ false };
    bool manualRoute_{ false };
};

void removeComponent(ComponentId id) {
    std::unordered_set<const Pin*> removedPins;
    for (const auto& c : components_) if (c && c->id() == id) for (const auto& pin : c->pins()) if (pin) removedPins.insert(pin.get());
    wires_.erase(std::remove_if(wires_.begin(), wires_.end(), [&](const std::shared_ptr<Wire>& wire) {
        return !wire || (wire->startPin() && removedPins.count(wire->startPin().get())) || (wire->endPin() && removedPins.count(wire->endPin().get()));
        }), wires_.end());
    components_.erase(std::remove_if(components_.begin(), components_.end(), [id](const auto& c) { return !c || c->id() == id; }), components_.end());
    pruneUnusedJunctions(); modified_ = true;
}

bool addWire(std::shared_ptr<Wire> wire) {
    if (!wire || !wire->startPin() || !wire->endPin() || wire->startPin() == wire->endPin()) return false;
    for (const auto& existing : wires_) {
        if (!existing) continue;
        const bool same = existing->startPin() == wire->startPin() && existing->endPin() == wire->endPin();
        const bool reverse = existing->startPin() == wire->endPin() && existing->endPin() == wire->startPin();
        if (same || reverse) return false;
    }
    if (wire->path().size() < 2) wire->routeSimple();
    wires_.push_back(std::move(wire)); modified_ = true; return true;
}

void removeWire(WireId id) {
    wires_.erase(std::remove_if(wires_.begin(), wires_.end(), [id](const auto& wire) { return !wire || wire->id() == id; }), wires_.end());
    pruneUnusedJunctions(); modified_ = true;
}


std::shared_ptr<Wire> wireById(WireId id) const {
    for (const auto& w : wires_) if (w && w->id() == id) return w;
    return nullptr;
}
std::shared_ptr<Pin> pinByReference(ComponentId componentId, const std::string& pinName) const {
    auto component = componentById(componentId); return component ? component->pinByName(pinName) : nullptr;
}

std::shared_ptr<Wire> wireAt(Vec2 world, double tolerance = 5.0) const {
    for (auto it = wires_.rbegin(); it != wires_.rend(); ++it) if (*it && (*it)->hitTest(world, tolerance)) return *it;
    return nullptr;
}

std::shared_ptr<Pin> pinAt(Vec2 world, double radius = kPinHoverRadius * 1.6) const {
    std::shared_ptr<Pin> best; double bestDistance = radius;
    for (const auto& component : components_) if (component) for (const auto& pin : component->pins()) if (pin) {
        const double d = distance(world, pin->worldPosition); if (d <= bestDistance) { bestDistance = d; best = pin; }
    }
    return best;
}

void rerouteConnectedWires() {
    for (auto& wire : wires_) if (wire) wire->reroutePreservingWaypoints();
}

class Junction {
public:
    Junction() : id_(nextId_++) {}
    explicit Junction(Vec2 position) : id_(nextId_++), position_(position) {}
    JunctionId id() const { return id_; }
    void setIdForLoad(JunctionId id) { id_ = id; nextId_ = std::max(nextId_, id_ + 1); }
    Vec2 position() const { return position_; }
    void setPosition(Vec2 p) { position_ = p; }
    bool selected() const { return selected_; }
    void setSelected(bool selected) { selected_ = selected; }
    bool hitTest(Vec2 p) const { return distance(position_, p) <= 7.0; }
private:
    inline static JunctionId nextId_{ 1 };
    JunctionId id_{ 0 };
    Vec2 position_;
    bool selected_{ false };
};

class NetNode {
public:
    explicit NetNode(int id = -1) : id_(id) {}

    int id() const { return id_; }
    const std::vector<std::shared_ptr<Pin>>& pins() const { return pins_; }
    const std::vector<std::shared_ptr<Wire>>& wires() const { return wires_; }
    const std::vector<std::shared_ptr<Junction>>& junctions() const { return junctions_; }

    void addPin(const std::shared_ptr<Pin>& pin) {
        if (pin) pins_.push_back(pin);
    }

    void addWire(const std::shared_ptr<Wire>& wire) {
        if (wire) wires_.push_back(wire);
    }

    void addJunction(const std::shared_ptr<Junction>& junction) {
        if (junction) junctions_.push_back(junction);
    }

    bool touches(Vec2 position, double tolerance = 2.0) const {
        return std::any_of(wires_.begin(), wires_.end(), [&](const std::shared_ptr<Wire>& wire) {
            return wire && wire->hitTest(position, tolerance);
            });
    }

private:
    int id_{ -1 };
    std::vector<std::shared_ptr<Pin>> pins_;
    std::vector<std::shared_ptr<Wire>> wires_;
    std::vector<std::shared_ptr<Junction>> junctions_;
};

class DisjointSet {
public:
    explicit DisjointSet(int n = 0) { reset(n); }
    void reset(int n) { parent_.resize(n); rank_.assign(n, 0); std::iota(parent_.begin(), parent_.end(), 0); }
    int find(int x) { return parent_[x] == x ? x : parent_[x] = find(parent_[x]); }
    void unite(int a, int b) {
        a = find(a); b = find(b); if (a == b) return;
        if (rank_[a] < rank_[b]) std::swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
    }
private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

    void addJunction(Vec2 position) {
        position = snapToGrid(position);
        for (const auto& junction : junctions_) if (junction && distance(junction->position(), position) < 1.0) return;
        int touching = 0; for (const auto& wire : wires_) if (wire && wire->hitTest(position, 2.0)) ++touching;
        if (touching >= 2) { junctions_.push_back(std::make_shared<Junction>(position)); modified_ = true; }
    }

    void removeJunction(JunctionId id) {
        junctions_.erase(std::remove_if(junctions_.begin(), junctions_.end(), [id](const auto& j) { return !j || j->id() == id; }), junctions_.end());
        modified_ = true;
    }


    std::shared_ptr<Junction> junctionAt(Vec2 world) const {
        for (auto it = junctions_.rbegin(); it != junctions_.rend(); ++it) if (*it && (*it)->hitTest(world)) return *it;
        return nullptr;
    }

    void buildNetlist() {
        nets_.clear();
        std::vector<std::shared_ptr<Pin>> allPins;
        std::unordered_map<const Pin*, int> index;
        for (const auto& component : components_) if (component) for (const auto& pin : component->pins()) if (pin) {
            index[pin.get()] = static_cast<int>(allPins.size()); allPins.push_back(pin); pin->net = -1;
        }
        DisjointSet dsu(static_cast<int>(allPins.size()));
        int firstGround = -1;
        for (int i = 0; i < static_cast<int>(allPins.size()); ++i) if (allPins[i]->type == PinType::Ground) {
            if (firstGround < 0) firstGround = i; else dsu.unite(firstGround, i);
        }

        for (const auto& wire : wires_) if (wire && wire->startPin() && wire->endPin()) {
            auto a = index.find(wire->startPin().get()), b = index.find(wire->endPin().get()); if (a != index.end() && b != index.end()) dsu.unite(a->second, b->second);
        }

        for (const auto& junction : junctions_) if (junction) {
            std::vector<std::shared_ptr<Wire>> touching;
            for (const auto& wire : wires_) if (wire && wire->hitTest(junction->position(), 2.0)) touching.push_back(wire);
            if (touching.size() >= 2) {
                std::shared_ptr<Pin> anchor = touching.front()->startPin();
                if (anchor) for (const auto& wire : touching) {
                    if (wire->startPin()) dsu.unite(index[anchor.get()], index[wire->startPin().get()]);
                    if (wire->endPin()) dsu.unite(index[anchor.get()], index[wire->endPin().get()]);
                }
            }
        }

        std::unordered_map<int, int> rootToNet;
        for (int i = 0; i < static_cast<int>(allPins.size()); ++i) {
            const int root = dsu.find(i);
            if (!rootToNet.count(root)) { const int id = static_cast<int>(nets_.size()); rootToNet[root] = id; nets_.emplace_back(id); }
            const int net = rootToNet[root]; allPins[i]->net = net; nets_[net].addPin(allPins[i]);
        }
        for (const auto& wire : wires_) if (wire && wire->startPin() && wire->startPin()->net >= 0) nets_[wire->startPin()->net].addWire(wire);
        for (const auto& junction : junctions_) if (junction) {
            for (auto& net : nets_) {
                if (net.touches(junction->position(), 2.0)) { net.addJunction(junction); break; }
            }
        }
    }

private:
    void pruneUnusedJunctions() {
        junctions_.erase(std::remove_if(junctions_.begin(), junctions_.end(), [&](const auto& junction) {
            if (!junction) return true;
            int touching = 0;
            for (const auto& wire : wires_) if (wire && wire->hitTest(junction->position(), 2.0)) ++touching;
            return touching < 2;
            }), junctions_.end());
    }


// simulation lifecycle, voltage lookup, analog matrix, linear solver, source/R/C stamping and solution update

    enum class SimulationState { Stopped, Running, Paused };

    class SimulationEngine {
    public:
        explicit SimulationEngine(CircuitDocument* document) : document_(document) {}

        SimulationState state() const { return state_; }
        double time() const { return time_; }
        double timeStep() const { return dt_; }
        const std::vector<std::string>& messages() const { return messages_; }
        const std::unordered_map<WireId, int>& wireLogicValues() const { return wireLogicValues_; }

        void start() { state_ = SimulationState::Running; accumulatorWall_ = 0.0; }
        void pause() { state_ = SimulationState::Paused; }
        void stop() {
            state_ = SimulationState::Stopped; time_ = 0.0; accumulatorWall_ = 0.0;
            netVoltages_.clear(); pinVoltages_.clear(); wireLogicValues_.clear(); capacitorVoltageHistory_.clear(); inductorCurrentHistory_.clear(); ammeterCurrent_.clear();

        }

        void update(double elapsedSeconds) {
            if (state_ != SimulationState::Running) return;
            accumulatorWall_ += clampValue(elapsedSeconds, 0.0, 0.1);
            int guard = 0;
            while (accumulatorWall_ >= dt_ && guard++ < 10) { accumulatorWall_ -= dt_; tick(); }
        }

        void step() {
            if (state_ == SimulationState::Running) return;
            state_ = SimulationState::Paused; tick();
        }

        double voltageAtPin(const std::shared_ptr<Pin>& pin) const {
            if (!pin) return 0.0;
            auto it = pinVoltages_.find(pin.get());
            if (it != pinVoltages_.end()) return it->second;
            if (pin->net >= 0 && pin->net < static_cast<int>(netVoltages_.size())) return netVoltages_[pin->net];
            return 0.0;
        }

        double voltageAtWorld(Vec2 world) const {
            if (!document_) return 0.0;
            if (auto pin = document_->pinAt(world, 12.0)) return voltageAtPin(pin);
            if (auto wire = document_->wireAt(world, 8.0)) {
                if (wire->startPin()) return voltageAtPin(wire->startPin());
            }
            return 0.0;
        }

        struct AnalogSystem {
            explicit AnalogSystem(const std::vector<NetNode>& nets, int groundNet)
                : variableOfNet(nets.size(), -1) {
                int nextVariable = 0;
                for (const auto& net : nets) {
                    if (net.id() == groundNet) continue;
                    if (net.id() >= 0 && net.id() < static_cast<int>(variableOfNet.size()))
                        variableOfNet[net.id()] = nextVariable++;
                }
                matrix.assign(nextVariable, std::vector<double>(nextVariable, 0.0));
                rhs.assign(nextVariable, 0.0);
            }

            int variable(int net) const {
                if (net < 0 || net >= static_cast<int>(variableOfNet.size())) return -1;
                return variableOfNet[net];
            }

            void addConductance(int netA, int netB, double conductance) {
                if (!std::isfinite(conductance) || conductance <= 0.0) return;
                const double g = clampValue(conductance, 1e-12, 1e9);
                const int a = variable(netA);
                const int b = variable(netB);

                if (a >= 0) matrix[a][a] += g;
                if (b >= 0) matrix[b][b] += g;
                if (a >= 0 && b >= 0) {
                    matrix[a][b] -= g;
                    matrix[b][a] -= g;
                }
            }

            void addToRhs(int net, double value) {
                const int row = variable(net);
                if (row >= 0) rhs[row] += value;
            }

            void addCurrentSource(int fromNet, int toNet, double current) {
                addToRhs(fromNet, -current);
                addToRhs(toNet, current);
            }

            void addDrivenVoltage(int positiveNet, int negativeNet, double voltage) {
                constexpr double kStrongConductance = 1e6;
                addConductance(positiveNet, negativeNet, kStrongConductance);
                addToRhs(positiveNet, kStrongConductance * voltage);
                addToRhs(negativeNet, -kStrongConductance * voltage);
            }

            void regularize(double epsilon) {
                for (std::size_t i = 0; i < matrix.size(); ++i) matrix[i][i] += epsilon;
            }

            std::vector<int> variableOfNet;
            std::vector<std::vector<double>> matrix;
            std::vector<double> rhs;
        };

        static bool solveLinearSystem(std::vector<std::vector<double>> matrix,
            std::vector<double> rhs,
            std::vector<double>& solution) {
            const int n = static_cast<int>(rhs.size());
            solution.assign(n, 0.0);
            if (n == 0) return true;
            if (static_cast<int>(matrix.size()) != n) return false;
            for (const auto& row : matrix) if (static_cast<int>(row.size()) != n) return false;

            // forward elimination with partial pivoting
            for (int column = 0; column < n; ++column) {
                int pivotRow = column;
                double pivotMagnitude = std::abs(matrix[column][column]);

                for (int row = column + 1; row < n; ++row) {
                    const double candidate = std::abs(matrix[row][column]);
                    if (candidate > pivotMagnitude) {
                        pivotMagnitude = candidate;
                        pivotRow = row;
                    }
                }

                if (pivotMagnitude < 1e-14) return false;
                if (pivotRow != column) {
                    std::swap(matrix[pivotRow], matrix[column]);
                    std::swap(rhs[pivotRow], rhs[column]);
                }

                const double pivot = matrix[column][column];
                for (int row = column + 1; row < n; ++row) {
                    const double factor = matrix[row][column] / pivot;
                    if (std::abs(factor) < 1e-18) {
                        matrix[row][column] = 0.0;
                        continue;
                    }

                    matrix[row][column] = 0.0;
                    for (int j = column + 1; j < n; ++j)
                        matrix[row][j] -= factor * matrix[column][j];
                    rhs[row] -= factor * rhs[column];
                }
            }

            // back substitution
            for (int row = n - 1; row >= 0; --row) {
                double value = rhs[row];
                for (int column = row + 1; column < n; ++column)
                    value -= matrix[row][column] * solution[column];

                const double diagonal = matrix[row][row];
                if (std::abs(diagonal) < 1e-14) return false;
                solution[row] = value / diagonal;
            }
            return true;
        }

        void solveAnalog() {
            const auto& nets = document_->nets();
            if (nets.empty()) {
                netVoltages_.clear();
                return;
            }

            int groundNet = -1;
            for (const auto& net : nets) {
                const bool containsGround = std::any_of(net.pins().begin(), net.pins().end(), [](const std::shared_ptr<Pin>& pin) {
                    return pin && pin->type == PinType::Ground;
                    });
                if (containsGround) groundNet = net.id();
            }
            if (groundNet < 0) {
                groundNet = 0;
                addMessageUnique("Ground reference missing. Net 0 assumed as virtual ground.");
            }

            AnalogSystem system(nets, groundNet);
            auto netOf = [](const std::shared_ptr<Pin>& pin) { return pin ? pin->net : -1; };

            for (const auto& component : document_->components()) {
                if (auto source = std::dynamic_pointer_cast<DCVoltageSource>(component)) {
                    system.addDrivenVoltage(netOf(component->pinByName("POS")),
                        netOf(component->pinByName("NEG")),
                        source->voltage());
                }
                else if (auto resistor = std::dynamic_pointer_cast<Resistor>(component)) {
                    const double r = std::max(1e-6, resistor->resistance());
                    system.addConductance(netOf(component->pinByName("A")), netOf(component->pinByName("B")), 1.0 / r);
                }
                else if (auto capacitor = std::dynamic_pointer_cast<Capacitor>(component)) {
                    if (capacitor->capacitance() > 0.0) {
                        const int nA = netOf(component->pinByName("A"));
                        const int nB = netOf(component->pinByName("B"));
                        const double eqConductance = capacitor->capacitance() / dt_;
                        const double pastV = capacitorVoltageHistory_[component->id()];

                        system.addConductance(nA, nB, eqConductance);
                        system.addToRhs(nA, eqConductance * pastV);
                        system.addToRhs(nB, -eqConductance * pastV);
                    }
                }

                const double pivotStabilizer = 1e-10;
                system.regularize(pivotStabilizer);

                std::vector<double> solution;
                if (!solveLinearSystem(system.matrix, system.rhs, solution)) {
                    addMessageUnique("Convergence error: Matrix solver could not find a solution.");
                    return;
                }

                netVoltages_.assign(nets.size(), 0.0);
                for (const auto& net : nets) {
                    if (net.id() == groundNet) continue;
                    const int varIndex = system.variable(net.id());
                    if (varIndex >= 0) netVoltages_[net.id()] = solution[varIndex];
                }

                pinVoltages_.clear();
                for (const auto& net : nets) {
                    for (const auto& pin : net.pins()) {
                        if (pin) pinVoltages_[pin.get()] = netVoltages_[net.id()];
                    }
                }

                CircuitDocument* document_{ nullptr };
                SimulationState state_{ SimulationState::Stopped };
                double dt_{ 0.01 };
                double time_{ 0.0 };
                double accumulatorWall_{ 0.0 };
                std::vector<double> netVoltages_;
                std::unordered_map<const Pin*, double> pinVoltages_;

                std::vector<std::string> messages_;

            else if (auto inductor = std::dynamic_pointer_cast<Inductor>(component)) {
                if (inductor->inductance() > 0.0) {
                    const int nA = netOf(component->pinByName("A"));
                    const int nB = netOf(component->pinByName("B"));
                    system.addConductance(nA, nB, dt_ / inductor->inductance());
                    system.addCurrentSource(nA, nB, inductorCurrentHistory_[component->id()]);
                }
            }
            else if (auto pot = std::dynamic_pointer_cast<Potentiometer>(component)) {
                const double rTotal = std::max(1e-3, pot->resistance());
                const double w = pot->wiper();
                system.addConductance(netOf(component->pinByName("A")), netOf(component->pinByName("W")), 1.0 / (rTotal * w));
                system.addConductance(netOf(component->pinByName("W")), netOf(component->pinByName("B")), 1.0 / (rTotal * (1.0 - w)));
            }
            else if (auto sw = std::dynamic_pointer_cast<Switch>(component)) {
                const double switchCond = sw->closed() ? 100.0 : 1e-9;
                system.addConductance(netOf(component->pinByName("A")), netOf(component->pinByName("B")), switchCond);
            }
            else if (auto button = std::dynamic_pointer_cast<PushButton>(component)) {
                const double btnCond = button->pressed() ? 100.0 : 1e-9;
                system.addConductance(netOf(component->pinByName("A")), netOf(component->pinByName("B")), btnCond);
            }

            else if (std::dynamic_pointer_cast<LED>(component)) {
                const double vDiff = voltageAtPin(component->pinByName("A")) - voltageAtPin(component->pinByName("K"));
                const double effectiveResistance = (vDiff > 1.8) ? 330.0 : 1e8;
                system.addConductance(netOf(component->pinByName("A")), netOf(component->pinByName("K")), 1.0 / effectiveResistance);
            }

        for (const auto& component : document_->components()) {
            if (std::dynamic_pointer_cast<Capacitor>(component)) {
                capacitorVoltageHistory_[component->id()] =
                    voltageAtPin(component->pinByName("A")) - voltageAtPin(component->pinByName("B"));
            }
            else if (auto inductor = std::dynamic_pointer_cast<Inductor>(component)) {
                const double vL = voltageAtPin(component->pinByName("A")) - voltageAtPin(component->pinByName("B"));
                inductorCurrentHistory_[component->id()] +=
                    dt_ / std::max(1e-9, inductor->inductance()) * vL;
            }

    void updateConsumers() {
        for (const auto& component : document_->components()) {
            if (auto led = std::dynamic_pointer_cast<LED>(component)) led->setOn(voltageAtPin(component->pinByName("A")) - voltageAtPin(component->pinByName("K")) > kLedThreshold);

    void updateWireValues() {
        wireLogicValues_.clear();
        for (const auto& wire : document_->wires()) if (wire) {
            const LogicState state = wire->startPin() ? logicAtPin(wire->startPin()) : LogicState::Undefined;
            wireLogicValues_[wire->id()] = static_cast<int>(state);
        }
    }

    void addMessageUnique(const std::string& message) {
        if (std::find(messages_.begin(), messages_.end(), message) == messages_.end()) messages_.push_back(message);
    }

    std::unordered_map<ComponentId, double> capacitorVoltageHistory_;
    std::unordered_map<ComponentId, double> inductorCurrentHistory_;
    std::unordered_map<WireId, int> wireLogicValues_;



