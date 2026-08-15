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


// MCU, memory and peripherals

class Microcontroller final : public GenericComponent {
public:
    class Port {
    public:
        bool readBit(int index) const {
            if (index < 0 || index >= 8) return false;
            const std::size_t i = static_cast<std::size_t>(index);
            return outputEnabled_[i] ? outputBits_[i] : inputBits_[i];
        }

        void sampleInputBit(int index, bool high) {
            if (index >= 0 && index < 8) inputBits_[static_cast<std::size_t>(index)] = high;
        }

        void writeBit(int index, bool high) {
            if (index < 0 || index >= 8) return;
            const std::size_t i = static_cast<std::size_t>(index);
            outputBits_[i] = high;
            outputEnabled_[i] = true;
        }

        bool drivesBit(int index, bool& high) const {
            if (index < 0 || index >= 8) return false;
            const std::size_t i = static_cast<std::size_t>(index);
            if (!outputEnabled_[i]) return false;
            high = outputBits_[i];
            return true;
        }

        void reset() {
            inputBits_.fill(false);
            outputBits_.fill(false);
            outputEnabled_.fill(false);
        }

        std::uint8_t valueByte() const {
            std::uint8_t value = 0;
            for (int bitIndex = 0; bitIndex < 8; ++bitIndex)
                if (readBit(bitIndex)) value |= static_cast<std::uint8_t>(1u << bitIndex);
            return value;
        }

        std::uint8_t outputByte() const {
            std::uint8_t value = 0;
            for (int bitIndex = 0; bitIndex < 8; ++bitIndex)
                if (outputBits_[static_cast<std::size_t>(bitIndex)]) value |= static_cast<std::uint8_t>(1u << bitIndex);
            return value;
        }

        std::uint8_t driveMask() const {
            std::uint8_t value = 0;
            for (int bitIndex = 0; bitIndex < 8; ++bitIndex)
                if (outputEnabled_[static_cast<std::size_t>(bitIndex)]) value |= static_cast<std::uint8_t>(1u << bitIndex);
            return value;
        }

        void loadOutputState(std::uint8_t value, std::uint8_t mask) {
            for (int bitIndex = 0; bitIndex < 8; ++bitIndex) {
                const std::size_t i = static_cast<std::size_t>(bitIndex);
                outputBits_[i] = ((value >> bitIndex) & 1u) != 0;
                outputEnabled_[i] = ((mask >> bitIndex) & 1u) != 0;
            }
        }

    private:
        std::array<bool, 8> inputBits_{};
        std::array<bool, 8> outputBits_{};
        std::array<bool, 8> outputEnabled_{};
    };

    static constexpr std::size_t kInternalRamSize = 256;
    static constexpr std::uint8_t kPortAOperand = 0xA0;
    static constexpr std::uint8_t kPortBOperand = 0xB0;

    Microcontroller()
        : GenericComponent("Microcontroller", ComponentCategory::Advanced),
        ram_(kInternalRamSize, 0) {
        setLabel("MCU?");
        for (int i = 0; i < 8; ++i) addPin(pinName('A', i), PinType::Bidirectional, { -58, -35 + i * 10.0 });
        for (int i = 0; i < 8; ++i) addPin(pinName('B', i), PinType::Bidirectional, { 58, -35 + i * 10.0 });
        addPin("VCC", PinType::Power, { -20, -52 }); addPin("GND", PinType::Ground, { 20, 52 });
        updatePinWorldPositions();
    }

    static std::string pinName(char port, int bit) {
        return "Port" + std::string(1, port) + "." + std::to_string(bit);
    }

    static bool decodePinName(const std::string& name, char& port, int& bit) {
        if (name.size() != 7 || name.rfind("Port", 0) != 0 || name[5] != '.') return false;
        if (name[4] != 'A' && name[4] != 'B') return false;
        if (name[6] < '0' || name[6] > '7') return false;
        port = name[4];
        bit = name[6] - '0';
        return true;
    }

    RectD localBounds() const override { return { -62, -56, 124, 112 }; }

    bool loadIntelHex(const std::string& path, std::string& error) {
        std::ifstream input(path);
        if (!input) { error = "Cannot open Intel HEX file: " + path; return false; }
        std::vector<std::uint8_t> image;
        std::string line;
        std::uint32_t base = 0;
        int lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber; line = trim(line); if (line.empty()) continue;
            if (line.front() != ':') { error = "HEX line " + std::to_string(lineNumber) + " has no ':'"; return false; }
            auto byteAt = [&](std::size_t offset, bool& ok) -> int {
                if (offset + 2 > line.size()) { ok = false; return 0; }
                try { return std::stoi(line.substr(offset, 2), nullptr, 16); }
                catch (...) { ok = false; return 0; }
                };
            bool ok = true;
            const int count = byteAt(1, ok);
            const int address = std::stoi(line.substr(3, 4), nullptr, 16);
            const int type = byteAt(7, ok);
            if (!ok || line.size() < static_cast<std::size_t>(11 + count * 2)) {
                error = "Invalid HEX record at line " + std::to_string(lineNumber); return false;
            }
            int checksum = count + ((address >> 8) & 0xff) + (address & 0xff) + type;
            std::vector<std::uint8_t> data;
            for (int i = 0; i < count; ++i) {
                const int byte = byteAt(9 + i * 2, ok); if (!ok) { error = "Invalid HEX byte"; return false; }
                data.push_back(static_cast<std::uint8_t>(byte)); checksum += byte;
            }
            const int recordChecksum = byteAt(9 + count * 2, ok);
            if (!ok || ((checksum + recordChecksum) & 0xff) != 0) {
                error = "HEX checksum failure at line " + std::to_string(lineNumber); return false;
            }
            if (type == 0x00) {
                const std::uint32_t absolute = base + static_cast<std::uint32_t>(address);
                if (image.size() < absolute + data.size()) image.resize(absolute + data.size(), 0);
                std::copy(data.begin(), data.end(), image.begin() + absolute);
            }
            else if (type == 0x01) {
                break;
            }
            else if (type == 0x04 && data.size() == 2) {
                base = (static_cast<std::uint32_t>(data[0]) << 24) |
                    (static_cast<std::uint32_t>(data[1]) << 16);
            }
        }
        flash_ = std::move(image); firmwarePath_ = path; resetCpu(); return true;
    }

    void resetCpu() {
        pc_ = 0;
        accumulator_ = 0;
        portA_.reset();
        portB_.reset();
        std::fill(ram_.begin(), ram_.end(), 0);
    }

    bool readPortBit(char port, int bit) const {
        const Port* selected = portFor(port);
        return selected ? selected->readBit(bit) : false;
    }

    void sampleInputPortBit(char port, int bit, bool high) {
        Port* selected = portFor(port);
        if (selected) selected->sampleInputBit(bit, high);
    }

    void writePortBit(char port, int bit, bool high) {
        Port* selected = portFor(port);
        if (selected) selected->writeBit(bit, high);
    }

    bool drivesPortBit(char port, int bit, bool& high) const {
        const Port* selected = portFor(port);
        return selected ? selected->drivesBit(bit, high) : false;
    }

    void tickCpu() {
        if (pc_ >= flash_.size()) return;

        auto fetch8 = [&]() -> std::uint8_t {
            return pc_ < flash_.size() ? flash_[pc_++] : 0;
            };
        auto fetch16 = [&]() -> std::uint16_t {
            const std::uint16_t low = fetch8();
            const std::uint16_t high = fetch8();
            return static_cast<std::uint16_t>(low | (high << 8));
            };
        auto decodePortOperand = [&](std::uint8_t encoded) -> char {
            if (encoded == kPortAOperand) return 'A';
            if (encoded == kPortBOperand) return 'B';
            return '\0';
            };

        const std::uint8_t opcode = fetch8();
        switch (opcode) {
        case 0x00:
            break; // NOP

        case 0x10:
            accumulator_ = fetch8();
            break; // MOV A, #imm8

        case 0x20: {
            const std::uint16_t address = fetch16();
            if (address < ram_.size()) ram_[address] = accumulator_;
            break; // MOV RAM[addr16], A
        }

        case 0x21: {
            const std::uint16_t address = fetch16();
            if (address < ram_.size()) accumulator_ = ram_[address];
            break; // MOV A, RAM[addr16]
        }

        case 0x30:
            accumulator_ = static_cast<std::uint8_t>(accumulator_ + fetch8());
            break; // ADD A, #imm8

        case 0x40:
            pc_ = fetch16();
            break; // JMP addr16

        case 0x50: {
            const char port = decodePortOperand(fetch8());
            const std::uint8_t bit = fetch8();
            if (port != '\0' && bit < 8) writePortBit(port, bit, true);
            break; // SETB PortA/PortB, bit
        }

        case 0x51: {
            const char port = decodePortOperand(fetch8());
            const std::uint8_t bit = fetch8();
            if (port != '\0' && bit < 8) writePortBit(port, bit, false);
            break; // CLR PortA/PortB, bit
        }

        default:
            break;
        }
    }

    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"firmwarePath", "Intel HEX path", firmwarePath_, false},
                {"programSize", "Program bytes", std::to_string(flash_.size()), true},
                {"PC", "Program Counter", std::to_string(pc_), false}, {"ACC", "Accumulator", std::to_string(accumulator_), false},
                {"PortA", "Port A", std::to_string(portA_.valueByte()), true},
                {"PortB", "Port B", std::to_string(portB_.valueByte()), true},
                {"ramSize", "Internal RAM (bytes)", std::to_string(ram_.size()), true} };
    }

    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "firmwarePath") { std::string error; return loadIntelHex(value, error); }
        if (key == "PC") { pc_ = static_cast<std::uint16_t>(std::stoul(value)); return true; }
        if (key == "ACC") { accumulator_ = static_cast<std::uint8_t>(std::stoul(value)); return true; }
        return false;
    }

    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["firmwarePath"] = firmwarePath_; s["pc"] = std::to_string(pc_); s["acc"] = std::to_string(accumulator_);
        s["portA"] = std::to_string(portA_.outputByte()); s["portB"] = std::to_string(portB_.outputByte());
        s["portADriveMask"] = std::to_string(portA_.driveMask()); s["portBDriveMask"] = std::to_string(portB_.driveMask());
        std::ostringstream flash; flash << std::hex << std::setfill('0');
        for (auto byte : flash_) flash << std::setw(2) << static_cast<int>(byte);
        s["flash"] = flash.str();
        std::ostringstream ram; ram << std::hex << std::setfill('0');
        for (auto byte : ram_) ram << std::setw(2) << static_cast<int>(byte);
        s["ram"] = ram.str();
        return s;
    }

    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("firmwarePath"); it != s.end()) firmwarePath_ = it->second;
        if (auto it = s.find("pc"); it != s.end()) pc_ = static_cast<std::uint16_t>(std::stoul(it->second));
        if (auto it = s.find("acc"); it != s.end()) accumulator_ = static_cast<std::uint8_t>(std::stoul(it->second));
        if (auto it = s.find("portA"); it != s.end()) {
            const auto mask = s.find("portADriveMask");
            portA_.loadOutputState(static_cast<std::uint8_t>(std::stoul(it->second)),
                mask != s.end() ? static_cast<std::uint8_t>(std::stoul(mask->second)) : 0xffu);
        }
        else if (auto it = s.find("p0"); it != s.end()) {
            portA_.loadOutputState(static_cast<std::uint8_t>(std::stoul(it->second)), 0xffu);
        }
        if (auto it = s.find("portB"); it != s.end()) {
            const auto mask = s.find("portBDriveMask");
            portB_.loadOutputState(static_cast<std::uint8_t>(std::stoul(it->second)),
                mask != s.end() ? static_cast<std::uint8_t>(std::stoul(mask->second)) : 0xffu);
        }
        else if (auto it = s.find("p1"); it != s.end()) {
            portB_.loadOutputState(static_cast<std::uint8_t>(std::stoul(it->second)), 0xffu);
        }

        auto parseHex = [](const std::string& hex) {
            std::vector<std::uint8_t> out;
            for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
                out.push_back(static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
            return out;
            };
        if (auto it = s.find("flash"); it != s.end()) flash_ = parseHex(it->second);
        if (auto it = s.find("ram"); it != s.end()) ram_ = parseHex(it->second);
        ram_.resize(kInternalRamSize, 0);
    }

    std::string compactStatus() const override {
        return "PC=" + std::to_string(pc_) + " ACC=" + std::to_string(accumulator_);
    }

private:
    Port* portFor(char port) {
        if (port == 'A') return &portA_;
        if (port == 'B') return &portB_;
        return nullptr;
    }

    const Port* portFor(char port) const {
        if (port == 'A') return &portA_;
        if (port == 'B') return &portB_;
        return nullptr;
    }

    std::vector<std::uint8_t> flash_;
    std::vector<std::uint8_t> ram_;
    std::string firmwarePath_;
    std::uint16_t pc_{ 0 };
    std::uint8_t accumulator_{ 0 };
    Port portA_;
    Port portB_;
};
class ExternalMemory final : public GenericComponent {
public:
    ExternalMemory() : GenericComponent("ExternalMemory", ComponentCategory::Advanced) {
        setLabel("RAM?");
        rebuildPins();
    }

    uint8_t readData() const {
        if (!readActive_) return 0;
        auto it = storage_.find(currentAddress_);
        return (it != storage_.end()) ? it->second : 0x00;
    }

    void writeData(uint8_t data) {
        if (writeActive_) {
            storage_[currentAddress_] = data;
        }
    }

    void setBusState(uint16_t addr, bool readEn, bool writeEn) {
        currentAddress_ = addr & 0x01FF;
        readActive_ = readEn;
        writeActive_ = writeEn;
    }
    void setReadBus(uint16_t addr, bool enable) { currentAddress_ = addr & 0x01FF; readActive_ = enable; }
    void write(uint16_t addr, uint8_t data) { currentAddress_ = addr & 0x01FF; storage_[currentAddress_] = data; }

    bool readActive() const { return readActive_; }
    bool writeActive() const { return writeActive_; }

    std::vector<PropertyDescriptor> properties() const override {
        return {
            {"label", "Label", label_, false},
            {"capacity", "Capacity", std::to_string(storageCapacity_) + " Bytes", true},
            {"usedCells", "Allocated Cells", std::to_string(storage_.size()), true}
        };
    }

    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "capacity") {
            storageCapacity_ = clampValue(std::stoi(value), 64, 4096);
            return true;
        }
        return false;
    }

    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["capacity"] = std::to_string(storageCapacity_);
        std::string memDump;
        for (const auto& [addr, val] : storage_) {
            memDump += std::to_string(addr) + ":" + std::to_string(val) + ";";
        }
        s["memoryDump"] = memDump;
        return s;
    }

    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("capacity"); it != s.end()) {
            storageCapacity_ = clampValue(std::stoi(it->second), 64, 4096);
        }
        storage_.clear();
        if (auto it = s.find("memoryDump"); it != s.end()) {
            std::stringstream ss(it->second);
            std::string item;
            while (std::getline(ss, item, ';')) {
                if (item.empty()) continue;
                auto pos = item.find(':');
                if (pos != std::string::npos) {
                    uint16_t addr = static_cast<uint16_t>(std::stoi(item.substr(0, pos)));
                    uint8_t val = static_cast<uint8_t>(std::stoi(item.substr(pos + 1)));
                    storage_[addr] = val;
                }
            }
        }
    }

    std::string compactStatus() const override {
        return "RAM: " + std::to_string(storage_.size()) + " B used";
    }

private:
    void rebuildPins() {
        std::vector<std::shared_ptr<Pin>> pins;
        for (int i = 0; i < 8; ++i) {
            auto pinA = std::make_shared<Pin>();
            pinA->name = "A" + std::to_string(i);
            pinA->type = PinType::Input;
            pinA->localPosition = { -52, -30.0 + i * 8.0 };
            pins.push_back(pinA);

            auto pinD = std::make_shared<Pin>();
            pinD->name = "D" + std::to_string(i);
            pinD->type = PinType::Output;
            pinD->localPosition = { 52, -30.0 + i * 8.0 };
            pins.push_back(pinD);
        }

        auto pinRD = std::make_shared<Pin>();
        pinRD->name = "RD"; pinRD->type = PinType::Input; pinRD->localPosition = { -20, 44 };
        pins.push_back(pinRD);

        auto pinWR = std::make_shared<Pin>();
        pinWR->name = "WR"; pinWR->type = PinType::Input; pinWR->localPosition = { 20, 44 };
        pins.push_back(pinWR);

        replacePins(std::move(pins));
    }

    int storageCapacity_{ 512 };
    uint16_t currentAddress_{ 0 };
    bool readActive_{ false };
    bool writeActive_{ false };
    std::unordered_map<uint16_t, uint8_t> storage_;
};

class LCD16x2 final : public GenericComponent {
public:
    LCD16x2() : GenericComponent("LCD16x2", ComponentCategory::Peripheral), line1_(16, ' '), line2_(16, ' ') {
        setLabel("LCD?");
        addPin("RS", PinType::Input, { -66, -30 }); addPin("RW", PinType::Input, { -66, -18 }); addPin("E", PinType::Input, { -66, -6 });
        for (int i = 0; i < 8; ++i) addPin("D" + std::to_string(i), PinType::Bidirectional, { -56 + i * 16.0, 38 });
        addPin("VCC", PinType::Power, { -32, -38 }); addPin("GND", PinType::Ground, { -16, -38 });
        updatePinWorldPositions();
    }
    RectD localBounds() const override { return { -70, -44, 140, 88 }; }
    const std::string& line1() const { return line1_; }
    const std::string& line2() const { return line2_; }
    void tickBus(bool rs, bool rw, bool enable, std::uint8_t data) {
        const bool writeEdge = enable && !previousEnable_;
        previousEnable_ = enable;

        if (!writeEdge || rw) return;

        if (rs) {
            writeData(data);
            return;
        }
        executeCommand(data);
    }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"line1", "Line 1", line1_, false}, {"line2", "Line 2", line2_, false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "line1") { line1_ = value.substr(0, 16); line1_.resize(16, ' '); return true; }
        if (key == "line2") { line2_ = value.substr(0, 16); line2_.resize(16, ' '); return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["line1"] = line1_; s["line2"] = line2_; s["cursor"] = std::to_string(cursor_); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        if (auto it = s.find("line1"); it != s.end()) { line1_ = it->second.substr(0, 16); line1_.resize(16, ' '); }
        if (auto it = s.find("line2"); it != s.end()) { line2_ = it->second.substr(0, 16); line2_.resize(16, ' '); }
        if (auto it = s.find("cursor"); it != s.end()) cursor_ = clampValue(std::stoi(it->second), 0, 31);
    }
private:
    void executeCommand(std::uint8_t command) {
        if (command == 0x01u) {
            clear();
            return;
        }

        if ((command & 0x80u) == 0) return;
        setCursorFromAddress(static_cast<std::uint8_t>(command & 0x7fu));
    }

    void setCursorFromAddress(std::uint8_t address) {
        int position = static_cast<int>(address);
        if (address >= 0x40u) position = 16 + static_cast<int>(address - 0x40u);
        cursor_ = clampValue(position, 0, 31);
    }

    void clear() {
        line1_.assign(16, ' ');
        line2_.assign(16, ' ');
        cursor_ = 0;
    }

    void writeData(std::uint8_t data) {
        const char character = static_cast<char>(data);
        if (cursor_ < 16) line1_[cursor_] = character;
        else line2_[cursor_ - 16] = character;
        cursor_ = (cursor_ + 1) % 32;
    }

    std::string line1_;
    std::string line2_;
    bool previousEnable_{ false };
    int cursor_{ 0 };
};

class Keypad final : public GenericComponent {
public:
    Keypad() : GenericComponent("Keypad", ComponentCategory::Peripheral) {
        setLabel("KPD?");
        for (int i = 0; i < 4; ++i) addPin("R" + std::to_string(i + 1), PinType::Input, { -54, -30 + i * 20.0 });
        for (int i = 0; i < 4; ++i) addPin("C" + std::to_string(i + 1), PinType::Output, { 54, -30 + i * 20.0 });
        updatePinWorldPositions();
    }
    RectD localBounds() const override { return { -58, -46, 116, 92 }; }
    void setPressedKey(std::string key) {
        key = toLower(trim(key)); pressedRow_ = pressedColumn_ = -1;
        static const std::array<std::array<std::string, 4>, 4> keys{ {
            {{"1", "2", "3", "a"}}, {{"4", "5", "6", "b"}}, {{"7", "8", "9", "c"}}, {{"*", "0", "#", "d"}}
        } };
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (keys[r][c] == key) { pressedRow_ = r; pressedColumn_ = c; }
    }
    std::string pressedKey() const {
        static const char* keys[4][4] = { {"1", "2", "3", "A"}, {"4", "5", "6", "B"}, {"7", "8", "9", "C"}, {"*", "0", "#", "D"} };
        return pressedRow_ < 0 ? "none" : keys[pressedRow_][pressedColumn_];
    }
    bool columnActive(int column, const std::array<bool, 4>& rowLow) const {
        return column == pressedColumn_ && pressedRow_ >= 0 && rowLow[pressedRow_];
    }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"pressedKey", "Pressed key", pressedKey(), false} };
    }
    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "pressedKey") { setPressedKey(value); return true; }
        return false;
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["pressedKey"] = pressedKey(); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s); if (auto it = s.find("pressedKey"); it != s.end()) setPressedKey(it->second);
    }
private:
    int pressedRow_{ -1 };
    int pressedColumn_{ -1 };
};
// Measurement components
class VoltageProbe final : public GenericComponent {
public:
    VoltageProbe() : GenericComponent("VoltageProbe", ComponentCategory::Measurement) {
        setLabel("PRB?"); addPin("IN", PinType::Input, { 0, 24 }); updatePinWorldPositions();
    }
    RectD localBounds() const override { return { -24, -24, 48, 52 }; }
    void setVoltage(double v) { voltage_ = v; }
    double voltage() const { return voltage_; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"voltage", "Measured voltage", formatDouble(voltage_) + " V", true} };
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["voltage"] = formatDouble(voltage_, 12); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s); if (auto it = s.find("voltage"); it != s.end()) voltage_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(voltage_) + " V"; }
private:
    double voltage_{ 0.0 };
};

class Voltmeter final : public GenericComponent {
public:
    Voltmeter() : GenericComponent("Voltmeter", ComponentCategory::Measurement) {
        setLabel("VM?"); addPin("POS", PinType::Input, { -34, 0 }); addPin("NEG", PinType::Input, { 34, 0 }); updatePinWorldPositions();
    }
    void setReading(double value) { reading_ = value; }
    double reading() const { return reading_; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"reading", "Reading", formatDouble(reading_) + " V", true} };
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["reading"] = formatDouble(reading_, 12); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s); if (auto it = s.find("reading"); it != s.end()) reading_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(reading_) + " V"; }
private:
    double reading_{ 0.0 };
};

class Ammeter final : public GenericComponent {
public:
    Ammeter() : GenericComponent("Ammeter", ComponentCategory::Measurement) {
        setLabel("AM?"); addPin("IN", PinType::Passive, { -34, 0 }); addPin("OUT", PinType::Passive, { 34, 0 }); updatePinWorldPositions();
    }
    void setReading(double value) { reading_ = value; }
    double reading() const { return reading_; }
    std::vector<PropertyDescriptor> properties() const override {
        return { {"label", "Label", label_, false}, {"reading", "Reading", formatDouble(reading_) + " A", true} };
    }
    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState(); s["reading"] = formatDouble(reading_, 12); return s;
    }
    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s); if (auto it = s.find("reading"); it != s.end()) reading_ = std::stod(it->second);
    }
    std::string compactStatus() const override { return formatDouble(reading_) + " A"; }
private:
    double reading_{ 0.0 };
};

class Oscilloscope final : public GenericComponent {
public:
    Oscilloscope() : GenericComponent("Oscilloscope", ComponentCategory::Measurement) {
        setLabel("OSC?");
        addPin("CH1", PinType::Input, { -66, -14 });
        addPin("CH2", PinType::Input, { -66, 12 });
        addPin("GND", PinType::Ground, { -66, 34 });
        updatePinWorldPositions();
    }

    RectD localBounds() const override { return { -70, -50, 140, 100 }; }

    void pushSample(double ch1, double ch2, double time) {
        if (!times_.empty() && time <= times_.back()) clearSamples();
        if (!times_.empty()) sampleInterval_ = std::max(1e-6, time - times_.back());
        times_.push_back(time);
        ch1_.push_back(ch1);
        ch2_.push_back(ch2);
        trimBuffers();
    }

    void clearSamples() { times_.clear(); ch1_.clear(); ch2_.clear(); }
    int maxSamples() const { return static_cast<int>(timePerDiv_ * 12.0 / sampleInterval_); }
    const std::deque<double>& times() const { return times_; }
    const std::deque<double>& ch1Samples() const { return ch1_; }
    const std::deque<double>& ch2Samples() const { return ch2_; }
    bool ch1Enabled() const { return ch1Enabled_; }
    bool ch2Enabled() const { return ch2Enabled_; }
    double ch1VoltsPerDiv() const { return ch1VoltsPerDiv_; }
    double ch2VoltsPerDiv() const { return ch2VoltsPerDiv_; }
    double timePerDiv() const { return timePerDiv_; }
    double ch1Offset() const { return ch1Offset_; }
    double ch2Offset() const { return ch2Offset_; }

    std::vector<PropertyDescriptor> properties() const override {
        return {
            {"label", "Label", label_, false},
            {"ch1Enabled", "CH1 enabled", ch1Enabled_ ? "true" : "false", false},
            {"ch2Enabled", "CH2 enabled", ch2Enabled_ ? "true" : "false", false},
            {"ch1VoltsPerDiv", "CH1 V/div", formatDouble(ch1VoltsPerDiv_), false},
            {"ch2VoltsPerDiv", "CH2 V/div", formatDouble(ch2VoltsPerDiv_), false},
            {"timePerDiv", "Time/div (s)", formatDouble(timePerDiv_), false},
            {"ch1Offset", "CH1 offset", formatDouble(ch1Offset_), false},
            {"ch2Offset", "CH2 offset", formatDouble(ch2Offset_), false},
            {"memory", "Time window", formatDouble(timePerDiv_ * 12.0) + " s", true}
        };
    }

    bool setProperty(const std::string& key, const std::string& value) override {
        if (Component::setProperty(key, value)) return true;
        if (key == "ch1Enabled") ch1Enabled_ = value == "true" || value == "1";
        else if (key == "ch2Enabled") ch2Enabled_ = value == "true" || value == "1";
        else if (key == "ch1VoltsPerDiv") ch1VoltsPerDiv_ = std::max(1e-6, std::stod(value));
        else if (key == "ch2VoltsPerDiv") ch2VoltsPerDiv_ = std::max(1e-6, std::stod(value));
        else if (key == "timePerDiv") { timePerDiv_ = std::max(1e-6, std::stod(value)); trimBuffers(); }
        else if (key == "ch1Offset") ch1Offset_ = std::stod(value);
        else if (key == "ch2Offset") ch2Offset_ = std::stod(value);
        else return false;
        return true;
    }

    std::map<std::string, std::string> persistentState() const override {
        auto s = Component::persistentState();
        s["ch1Enabled"] = ch1Enabled_ ? "1" : "0";
        s["ch2Enabled"] = ch2Enabled_ ? "1" : "0";
        s["ch1VoltsPerDiv"] = formatDouble(ch1VoltsPerDiv_, 12);
        s["ch2VoltsPerDiv"] = formatDouble(ch2VoltsPerDiv_, 12);
        s["timePerDiv"] = formatDouble(timePerDiv_, 12);
        s["ch1Offset"] = formatDouble(ch1Offset_, 12);
        s["ch2Offset"] = formatDouble(ch2Offset_, 12);
        s["sampleInterval"] = formatDouble(sampleInterval_, 12);

        auto encode = [](const std::deque<double>& values) {
            std::ostringstream out;
            for (double v : values) out << std::setprecision(12) << v << ',';
            return out.str();
            };
        s["times"] = encode(times_);
        s["ch1"] = encode(ch1_);
        s["ch2"] = encode(ch2_);
        return s;
    }

    void loadPersistentState(const std::map<std::string, std::string>& s) override {
        Component::loadPersistentState(s);
        auto getBool = [&](const char* key, bool& target) {
            if (auto it = s.find(key); it != s.end()) target = it->second == "1";
            };
        auto getDouble = [&](const char* key, double& target) {
            if (auto it = s.find(key); it != s.end()) target = std::stod(it->second);
            };

        getBool("ch1Enabled", ch1Enabled_);
        getBool("ch2Enabled", ch2Enabled_);
        getDouble("ch1VoltsPerDiv", ch1VoltsPerDiv_);
        getDouble("ch2VoltsPerDiv", ch2VoltsPerDiv_);
        getDouble("timePerDiv", timePerDiv_);
        getDouble("ch1Offset", ch1Offset_);
        getDouble("ch2Offset", ch2Offset_);
        getDouble("sampleInterval", sampleInterval_);

        auto decode = [](const std::string& value) {
            std::deque<double> out;
            std::stringstream ss(value);
            std::string item;
            while (std::getline(ss, item, ','))
                if (!item.empty()) out.push_back(std::stod(item));
            return out;
            };

        if (auto it = s.find("times"); it != s.end()) times_ = decode(it->second);
        if (auto it = s.find("ch1"); it != s.end()) ch1_ = decode(it->second);
        if (auto it = s.find("ch2"); it != s.end()) ch2_ = decode(it->second);

        trimBuffers();
    }

    std::string compactStatus() const override {
        return "Window: " + formatDouble(timePerDiv_ * 12.0) + "s";
    }

private:
    void trimBuffers() {
        if (times_.empty()) return;
        const double retainWindow = timePerDiv_ * 12.0;
        const double latestTime = times_.back();

        while (!times_.empty() && (latestTime - times_.front() > retainWindow)) {
            times_.pop_front();
            ch1_.pop_front();
            ch2_.pop_front();
        }
    }

    bool ch1Enabled_{ true }, ch2Enabled_{ false };
    double ch1VoltsPerDiv_{ 2.0 }, ch2VoltsPerDiv_{ 2.0 }, timePerDiv_{ 0.05 };
    double ch1Offset_{ 0.0 }, ch2Offset_{ 0.0 };
    double sampleInterval_{ 0.005 };

    std::deque<double> times_, ch1_, ch2_;
};

// Component schematic rendering
void Component::draw(CanvasPainter& painter, bool selected) const {
    const Color outline = selected ? Palette::Selection : Palette::DarkText;
    painter.rect(worldBounds(), outline, false);
    drawPins(painter);
}

void GenericComponent::draw(CanvasPainter& painter, bool selected) const {
    const Color stroke = selected ? Palette::Selection : Palette::DarkText;
    const Color bodyFill{ 235, 238, 242, 255 };
    auto P = [&](double x, double y) { return transformLocal({ x, y }); };
    auto line = [&](double x1, double y1, double x2, double y2, Color c = Color{ 0, 0, 0, 0 }, int thickness = 2) {
        if (c.a == 0) c = stroke;
        painter.line(P(x1, y1), P(x2, y2), c, thickness);
        };
    auto circle = [&](double x, double y, double r, Color c, bool filled = false) { painter.circle(P(x, y), r, c, filled); };
    auto localRect = [&](RectD r, Color c, bool filled = false, Color fill = {}) {
        // Rectangles are rendered as transformed polylines so rotation/mirroring is correct.
        std::vector<Vec2> points{ P(r.left(), r.top()), P(r.right(), r.top()), P(r.right(), r.bottom()), P(r.left(), r.bottom()), P(r.left(), r.top()) };
        if (filled) {
            // SDL_Renderer has no arbitrary polygon fill; use a conservative world bounding fill.
            painter.rect(worldBounds(), c, true, fill);
        }
        painter.polyline(points, c, 2);
        };
    auto label = [&](const std::string& value, double x, double y, int size = 11, Color c = Color{ 0, 0, 0, 0 }) {
        if (c.a == 0) c = stroke;
        painter.textWorld(value, P(x, y), c, size, true);
        };

    if (type_ == "Ground") {
        line(0, -20, 0, 0); line(-14, 0, 14, 0); line(-9, 5, 9, 5); line(-4, 10, 4, 10);
    }
    else if (type_ == "DCVoltageSource") {
        line(0, -32, 0, -16); line(0, 16, 0, 32); circle(0, 0, 15, stroke, false); label("+", 0, -7, 11); label("-", 0, 8, 11);
        label(label_, 0, -45, 11); label(compactStatus(), 0, 46, 9, Palette::Muted);
    }
    else if (type_ == "Battery") {
        line(0, -32, 0, -12); line(0, 12, 0, 32); line(-13, -12, 13, -12); line(-8, -4, 8, -4, stroke, 3);
        line(-13, 5, 13, 5); line(-8, 13, 8, 13, stroke, 3); label(label_, 0, -45, 11); label(compactStatus(), 0, 46, 9, Palette::Muted);
    }
    else if (type_ == "ClockGenerator") {
        localRect({ -30, -20, 60, 40 }, stroke, true, bodyFill); line(-16, 5, -16, -5); line(-16, -5, -6, -5); line(-6, -5, -6, 5);
        line(-6, 5, 5, 5); line(5, 5, 5, -5); line(5, -5, 15, -5); line(30, 0, 34, 0); label(label_, 0, -31, 10); label(compactStatus(), 0, 31, 8, Palette::Muted);
    }
    else if (type_ == "Resistor") {
        line(-34, 0, -24, 0); line(24, 0, 34, 0);
        std::vector<Vec2> zig{ P(-24, 0), P(-18, -9), P(-10, 9), P(-2, -9), P(6, 9), P(14, -9), P(24, 0) };
        painter.polyline(zig, stroke, 2); label(label_, 0, -20, 10); label(compactStatus(), 0, 21, 8, Palette::Muted);
    }
    else if (type_ == "Capacitor") {
        line(-34, 0, -7, 0); line(7, 0, 34, 0); line(-7, -13, -7, 13); line(7, -13, 7, 13); label(label_, 0, -23, 10); label(compactStatus(), 0, 24, 8, Palette::Muted);
    }
    else if (type_ == "Inductor") {
        line(-34, 0, -22, 0); line(22, 0, 34, 0);
        for (int i = 0; i < 4; ++i) {
            const double cx = -16.5 + i * 11.0;
            std::vector<Vec2> arc;
            for (int k = 0; k <= 12; ++k) { const double a = kPi + kPi * k / 12.0; arc.push_back(P(cx + 5.5 * std::cos(a), 5.5 * std::sin(a))); }
            painter.polyline(arc, stroke, 2);
        }
        label(label_, 0, -23, 10); label(compactStatus(), 0, 24, 8, Palette::Muted);
    }
    else if (type_ == "Potentiometer") {
        line(-36, 0, -24, 0); line(24, 0, 36, 0); localRect({ -24, -8, 48, 16 }, stroke, true, bodyFill);
        const auto* pot = dynamic_cast<const Potentiometer*>(this); const double wx = pot ? -20.0 + 40.0 * pot->wiper() : 0.0;
        line(0, -30, wx, -9); line(wx, -9, wx - 4, -14); line(wx, -9, wx + 5, -12); label(label_, 0, 22, 10); label(compactStatus(), 0, 34, 8, Palette::Muted);
    }
    else if (type_ == "Switch" || type_ == "PushButton") {
        const bool active = type_ == "Switch" ? dynamic_cast<const Switch*>(this)->closed() : dynamic_cast<const PushButton*>(this)->pressed();
        line(-32, 0, -12, 0); line(12, 0, 32, 0); circle(-12, 0, 3, stroke, true); circle(12, 0, 3, stroke, true);
        line(-12, 0, 12, active ? 0 : -14, active ? Palette::Accent2 : stroke, 3); label(label_, 0, -26, 10); label(compactStatus(), 0, 24, 8, active ? Palette::Accent2 : Palette::Muted);
    }
    else if (type_ == "LED") {
        const auto* led = dynamic_cast<const LED*>(this); const Color lit = led && led->on() ? led->ledColor() : Color{ 100, 45, 45, 255 };
        line(-30, 0, -13, 0); line(13, 0, 30, 0);
        std::vector<Vec2> tri{ P(-13, -11), P(-13, 11), P(12, 0), P(-13, -11) }; painter.polyline(tri, stroke, 2); line(12, -11, 12, 11);
        circle(0, 0, 9, lit, true); if (led && led->on()) { line(3, -13, 12, -22, lit, 2); line(9, -12, 18, -21, lit, 2); }
        label(label_, 0, 24, 10); label(compactStatus(), 0, 35, 8, led && led->on() ? lit : Palette::Muted);
    }
    else if (type_ == "SevenSegment") {
        const auto* display = dynamic_cast<const SevenSegment*>(this); const std::uint8_t mask = display ? display->mask() : 0;
        localRect({ -42, -36, 84, 68 }, stroke, true, Color{ 45, 35, 35, 255 });
        const Color on{ 250, 45, 35, 255 }, off{ 85, 35, 35, 255 };
        auto seg = [&](int bit, double x1, double y1, double x2, double y2) { line(x1, y1, x2, y2, (mask & (1u << bit)) ? on : off, 5); };
        seg(0, -20, -27, 20, -27); seg(1, 25, -23, 25, -2); seg(2, 25, 3, 25, 24); seg(3, -20, 28, 20, 28);
        seg(4, -25, 3, -25, 24); seg(5, -25, -23, -25, -2); seg(6, -20, 0, 20, 0); circle(35, 27, 3, (mask & 0x80) ? on : off, true);
        label(label_, 0, -48, 10);
    }
    else if (auto gate = dynamic_cast<const LogicGate*>(this)) {
        localRect({ -30, -24, 60, 48 }, stroke, true, Color{ 225, 230, 250, 255 });
        label(gate->gateText(), 0, -6, 14); label(label_, 0, 10, 8, Palette::Muted); line(30, 0, 36, 0);
        for (const auto& pin : pins_) if (pin && pin->name.rfind("IN", 0) == 0) line(-36, pin->localPosition.y, -30, pin->localPosition.y);
        circle(25, 0, 4, gate->outputValid() ? (gate->output() ? Palette::WireHigh : Palette::WireLow) : Palette::Warning, true);
    }
    else if (type_ == "DFlipFlop") {
        const auto* dff = dynamic_cast<const DFlipFlop*>(this); localRect({ -32, -27, 64, 54 }, stroke, true, Color{ 225, 230, 250, 255 });
        label("D  FF", 0, -5, 12); label(label_, 0, 11, 8, Palette::Muted); line(-38, -11, -32, -11); line(-38, 12, -32, 12); line(32, -11, 38, -11); line(32, 12, 38, 12);
        circle(27, -11, 4, dff && dff->valid() ? (dff->q() ? Palette::WireHigh : Palette::WireLow) : Palette::Warning, true);
    }
    else if (type_ == "SimpleADC" || type_ == "SimpleDAC") {
        localRect({ -46, -40, 92, 80 }, stroke, true, Color{ 226, 238, 235, 255 }); label(type_ == "SimpleADC" ? "ADC" : "DAC", 0, -7, 15); label(label_, 0, 12, 9, Palette::Muted); label(compactStatus(), 0, 27, 8, Palette::Muted);
    }
    else if (type_ == "Microcontroller") {
        localRect({ -50, -46, 100, 92 }, stroke, true, Color{ 42, 46, 54, 255 }); label("MCU", 0, -18, 15, Palette::Text); label(label_, 0, 0, 10, Palette::Text); label(compactStatus(), 0, 19, 8, Palette::Muted);
    }
    else if (type_ == "ExternalMemory") {
        localRect({ -46, -44, 92, 88 }, stroke, true, Color{ 235, 225, 245, 255 }); label("EXT RAM", 0, -6, 13); label(label_, 0, 12, 9, Palette::Muted);
    }
    else if (type_ == "LCD16x2") {
        const auto* lcd = dynamic_cast<const LCD16x2*>(this); localRect({ -62, -38, 124, 70 }, stroke, true, Color{ 25, 135, 65, 255 });
        localRect({ -52, -27, 104, 44 }, Color{ 18, 65, 30, 255 }, true, Color{ 170, 215, 160, 255 });
        if (lcd) { label(lcd->line1(), 0, -18, 8, Color{ 25, 70, 30, 255 }); label(lcd->line2(), 0, 1, 8, Color{ 25, 70, 30, 255 }); }
        label(label_, 0, 28, 9, Palette::Text);
    }
    else if (type_ == "Keypad") {
        const auto* keypad = dynamic_cast<const Keypad*>(this); localRect({ -50, -42, 100, 84 }, stroke, true, Color{ 55, 58, 62, 255 });
        static const char* keys[4][4] = { {"1", "2", "3", "A"}, {"4", "5", "6", "B"}, {"7", "8", "9", "C"}, {"*", "0", "#", "D"} };
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
            const double x = -33 + c * 22.0, y = -27 + r * 18.0;
            const bool pressed = keypad && keypad->pressedKey() == keys[r][c]; circle(x, y, 7, pressed ? Palette::Accent : Color{ 220, 225, 230, 255 }, true); label(keys[r][c], x, y - 5, 8, Palette::DarkText);
        }
    }
    else if (type_ == "VoltageProbe") {
        const auto* probe = dynamic_cast<const VoltageProbe*>(this); circle(0, -2, 16, stroke, false); line(0, 14, 0, 24); label("V", 0, -9, 13); label(probe ? probe->compactStatus() : "", 0, 27, 8, Palette::Muted);
    }
    else if (type_ == "Voltmeter" || type_ == "Ammeter") {
        localRect({ -30, -20, 60, 40 }, stroke, true, Color{ 248, 242, 242, 255 }); line(-34, 0, -30, 0); line(30, 0, 34, 0); label(type_ == "Voltmeter" ? "V" : "A", 0, -13, 11); label(compactStatus(), 0, 2, 8, Palette::Muted); label(label_, 0, 21, 8);
    }
    else if (type_ == "Oscilloscope") {
        const auto* scope = dynamic_cast<const Oscilloscope*>(this); localRect({ -60, -44, 120, 88 }, stroke, true, Color{ 15, 22, 18, 255 });
        for (int i = -4; i <= 4; ++i) line(-48, i * 8, 48, i * 8, Color{ 45, 75, 50, 255 }, 1);
        for (int i = -5; i <= 5; ++i) line(i * 9.6, -32, i * 9.6, 32, Color{ 45, 75, 50, 255 }, 1);
        if (scope && scope->ch1Samples().size() > 1) {
            auto drawTrace = [&](const std::deque<double>& samples, double vdiv, double offset, Color color) {
                std::vector<Vec2> pts; const std::size_t n = samples.size(); const std::size_t start = n > 120 ? n - 120 : 0;
                for (std::size_t i = start; i < n; ++i) { const double x = -48.0 + 96.0 * (i - start) / std::max<std::size_t>(1, n - start - 1); const double y = clampValue(-(samples[i] + offset) / (vdiv * 4.0) * 32.0, -32.0, 32.0); pts.push_back(P(x, y)); }
                painter.polyline(pts, color, 2);
                };
            if (scope->ch1Enabled()) drawTrace(scope->ch1Samples(), scope->ch1VoltsPerDiv(), scope->ch1Offset(), Color{ 40, 250, 80, 255 });
            if (scope->ch2Enabled()) drawTrace(scope->ch2Samples(), scope->ch2VoltsPerDiv(), scope->ch2Offset(), Color{ 250, 205, 30, 255 });
        }
        label(label_, 0, -52, 10);
    }
    else {
        localRect(localBounds(), stroke, true, bodyFill); label(type_, 0, -5, 10); label(label_, 0, 10, 8, Palette::Muted);
    }

    if (selected) {
        const RectD bounds = worldBounds();
        painter.rect({ bounds.x - 4, bounds.y - 4, bounds.w + 8, bounds.h + 8 }, Palette::Selection, false);
    }
    drawPins(painter);
}

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
//Component factory and circuit graph/document
class ComponentFactory {
public:
    static ComponentFactory& instance() {
        static ComponentFactory factory;
        return factory;
    }

    std::shared_ptr<Component> create(const std::string& type) const {
        auto it = creators_.find(type);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    ComponentFactory() {
        registerComponent<Ground>("Ground");
        registerComponent<DCVoltageSource>("DCVoltageSource");
        registerComponent<Battery>("Battery");
        registerComponent<ClockGenerator>("ClockGenerator");
        registerComponent<Resistor>("Resistor");
        registerComponent<Capacitor>("Capacitor");
        registerComponent<Inductor>("Inductor");
        registerComponent<Potentiometer>("Potentiometer");
        registerComponent<Switch>("Switch");
        registerComponent<PushButton>("PushButton");
        registerComponent<LED>("LED");
        registerComponent<SevenSegment>("SevenSegment");
        registerComponent<AndGate>("AndGate");
        registerComponent<OrGate>("OrGate");
        registerComponent<NotGate>("NotGate");
        registerComponent<XorGate>("XorGate");
        registerComponent<NandGate>("NandGate");
        registerComponent<DFlipFlop>("DFlipFlop");
        registerComponent<SimpleADC>("SimpleADC");
        registerComponent<SimpleDAC>("SimpleDAC");
        registerComponent<Microcontroller>("Microcontroller");
        registerComponent<ExternalMemory>("ExternalMemory");
        registerComponent<LCD16x2>("LCD16x2");
        registerComponent<Keypad>("Keypad");
        registerComponent<VoltageProbe>("VoltageProbe");
        registerComponent<Voltmeter>("Voltmeter");
        registerComponent<Ammeter>("Ammeter");
        registerComponent<Oscilloscope>("Oscilloscope");
    }

    template<typename T>
    void registerComponent(const std::string& name) {
        creators_[name] = []() { return std::make_shared<T>(); };
    }

    std::unordered_map<std::string, std::function<std::shared_ptr<Component>()>> creators_;
};

static std::shared_ptr<Component> createComponentByType(const std::string& type) {
    return ComponentFactory::instance().create(type);
}

struct LibraryEntry {
    std::string type;
    ComponentCategory category;
    std::string description;
};

static const std::vector<LibraryEntry>& componentLibrary() {
    static const std::vector<LibraryEntry> entries = {
        {"Ground", ComponentCategory::Sources, "Global 0 V reference. At least one ground is required."},
        {"DCVoltageSource", ComponentCategory::Sources, "Ideal adjustable DC source with POS and NEG terminals."},
        {"Battery", ComponentCategory::Sources, "Battery-like DC source."},
        {"ClockGenerator", ComponentCategory::Sources, "0/5 V square-wave digital clock."},
        {"Resistor", ComponentCategory::Passive, "Ohmic two-terminal resistor."},
        {"Capacitor", ComponentCategory::Passive, "Backward-Euler dynamic capacitor."},
        {"Inductor", ComponentCategory::Passive, "Backward-Euler dynamic inductor."},
        {"Potentiometer", ComponentCategory::Passive, "Three-terminal adjustable resistor."},
        {"Switch", ComponentCategory::Interactive, "Persistent open/closed interactive switch."},
        {"PushButton", ComponentCategory::Interactive, "Momentary switch: active only while held."},
        {"LED", ComponentCategory::Interactive, "Colored LED with threshold and visual state."},
        {"SevenSegment", ComponentCategory::Interactive, "Seven-segment display with A-G, DP and COM pins."},
        {"AndGate", ComponentCategory::Digital, "Configurable-input AND gate with propagation delay."},
        {"OrGate", ComponentCategory::Digital, "Configurable-input OR gate with propagation delay."},
        {"NotGate", ComponentCategory::Digital, "Digital inverter with propagation delay."},
        {"XorGate", ComponentCategory::Digital, "Configurable-input XOR gate with propagation delay."},
        {"NandGate", ComponentCategory::Digital, "Configurable-input NAND gate with propagation delay."},
        {"DFlipFlop", ComponentCategory::Digital, "Rising-edge D flip-flop with Q and Q-bar."},
        {"SimpleADC", ComponentCategory::Advanced, "Ideal N-bit ADC with saturation and conversion delay."},
        {"SimpleDAC", ComponentCategory::Advanced, "Ideal N-bit DAC with conversion delay."},
        {"Microcontroller", ComponentCategory::Advanced, "Intel HEX loader and MOV/ADD/JMP/SETB/CLR bytecode MCU."},
        {"ExternalMemory", ComponentCategory::Peripheral, "256-byte external RAM with address/data buses."},
        {"LCD16x2", ComponentCategory::Peripheral, "HD44780-like 16x2 LCD bus model."},
        {"Keypad", ComponentCategory::Peripheral, "Interactive 4x4 matrix keypad."},
        {"VoltageProbe", ComponentCategory::Measurement, "Persistent voltage probe component."},
        {"Voltmeter", ComponentCategory::Measurement, "Differential digital voltmeter."},
        {"Ammeter", ComponentCategory::Measurement, "Series digital ammeter."},
        {"Oscilloscope", ComponentCategory::Measurement, "Two-channel oscilloscope with independent V/div and offsets."}
    };
    return entries;
}

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
class CircuitDocument {
public:
    void clear() {
        components_.clear(); wires_.clear(); junctions_.clear(); nets_.clear();
        canvasWidth_ = 1600; canvasHeight_ = 1000; projectName_ = "Untitled"; modified_ = false;
    }

    const std::vector<std::shared_ptr<Component>>& components() const { return components_; }
    const std::vector<std::shared_ptr<Wire>>& wires() const { return wires_; }
    const std::vector<std::shared_ptr<Junction>>& junctions() const { return junctions_; }
    const std::vector<NetNode>& nets() const { return nets_; }
    int canvasWidth() const { return canvasWidth_; }
    int canvasHeight() const { return canvasHeight_; }
    const std::string& projectName() const { return projectName_; }
    bool modified() const { return modified_; }
    void setModified(bool value) { modified_ = value; }
    void setCanvasSize(int width, int height) { canvasWidth_ = clampValue(width, 400, 10000); canvasHeight_ = clampValue(height, 300, 10000); modified_ = true; }
    void setProjectName(std::string name) { projectName_ = std::move(name); modified_ = true; }

    void addComponent(std::shared_ptr<Component> component) {
        if (!component) return;
        components_.push_back(std::move(component));
        modified_ = true;
    }

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

    std::shared_ptr<Component> componentById(ComponentId id) const {
        for (const auto& c : components_) if (c && c->id() == id) return c;
        return nullptr;
    }
    std::shared_ptr<Wire> wireById(WireId id) const {
        for (const auto& w : wires_) if (w && w->id() == id) return w;
        return nullptr;
    }
    std::shared_ptr<Pin> pinByReference(ComponentId componentId, const std::string& pinName) const {
        auto component = componentById(componentId); return component ? component->pinByName(pinName) : nullptr;
    }

    std::shared_ptr<Component> componentAt(Vec2 world) const {
        for (auto it = components_.rbegin(); it != components_.rend(); ++it) if (*it && (*it)->hitTest(world)) return *it;
        return nullptr;
    }
    std::shared_ptr<Wire> wireAt(Vec2 world, double tolerance = 5.0) const {
        for (auto it = wires_.rbegin(); it != wires_.rend(); ++it) if (*it && (*it)->hitTest(world, tolerance)) return *it;
        return nullptr;
    }
    std::shared_ptr<Junction> junctionAt(Vec2 world) const {
        for (auto it = junctions_.rbegin(); it != junctions_.rend(); ++it) if (*it && (*it)->hitTest(world)) return *it;
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
    std::vector<std::string> runDRC() {
        buildNetlist();
        std::vector<std::string> warnings;
        bool hasGround = false;
        for (const auto& component : components_) if (component) {
            for (const auto& pin : component->pins()) if (pin) {
                if (pin->type == PinType::Ground) hasGround = true;
                if (pin->type == PinType::Input || pin->type == PinType::Bidirectional) {
                    bool connected = false;
                    for (const auto& wire : wires_) if (wire && (wire->startPin() == pin || wire->endPin() == pin)) { connected = true; break; }
                    if (!connected) warnings.push_back("Floating input detected: " + component->label() + "." + pin->name);
                }
            }
        }
        if (!hasGround && !components_.empty()) warnings.push_back("No GND reference exists; analog simulation uses a temporary reference.");
        for (const auto& net : nets_) {
            int outputs = 0, powers = 0, grounds = 0;
            for (const auto& pin : net.pins()) if (pin) {
                outputs += pin->type == PinType::Output ? 1 : 0;
                powers += pin->type == PinType::Power ? 1 : 0;
                grounds += pin->type == PinType::Ground ? 1 : 0;
            }
            if (outputs + powers > 1) warnings.push_back("Possible short circuit: net " + std::to_string(net.id()) + " has multiple drivers.");
            if (powers > 0 && grounds > 0) warnings.push_back("Short circuit: power is directly connected to ground on net " + std::to_string(net.id()) + ".");
        }
        if (components_.empty()) warnings.push_back("Schematic is empty: add components to simulate.");
        return warnings;
    }

    std::string serializeToString() const {
        std::ostringstream out;
        out << "PROTEUS_SDL2 3\n";
        out << "PROJECT " << std::quoted(projectName_) << ' ' << canvasWidth_ << ' ' << canvasHeight_ << "\n";
        out << "COMPONENTS " << components_.size() << "\n";
        for (const auto& component : components_) {
            const auto state = component->persistentState();
            out << "COMP " << component->id() << ' ' << std::quoted(component->type()) << ' '
                << std::setprecision(17) << component->position().x << ' ' << component->position().y << ' '
                << component->rotation() << ' ' << component->mirroredHorizontal() << ' ' << component->mirroredVertical() << ' '
                << state.size() << "\n";
            for (const auto& [key, value] : state) out << "PROP " << std::quoted(key) << ' ' << std::quoted(value) << "\n";
        }
        out << "WIRES " << wires_.size() << "\n";
        for (const auto& wire : wires_) {
            if (!wire || !wire->startPin() || !wire->endPin()) continue;
            out << "WIRE " << wire->id() << ' ' << wire->startPin()->ownerId << ' ' << std::quoted(wire->startPin()->name) << ' '
                << wire->endPin()->ownerId << ' ' << std::quoted(wire->endPin()->name) << ' ' << wire->manualRoute() << ' ' << wire->path().size() << "\n";
            for (const auto& point : wire->path()) out << "POINT " << std::setprecision(17) << point.x << ' ' << point.y << "\n";
        }
        out << "JUNCTIONS " << junctions_.size() << "\n";
        for (const auto& junction : junctions_) if (junction) out << "JUNCTION " << junction->id() << ' ' << junction->position().x << ' ' << junction->position().y << "\n";
        out << "END\n";
        return out.str();
    }

    bool deserializeFromString(const std::string& data, std::string& error) {
        CircuitDocument loaded;
        std::istringstream in(data);
        std::string token; int version = 0;
        if (!(in >> token >> version) || token != "PROTEUS_SDL2" || version < 1 || version > 3) { error = "Unsupported or invalid project file."; return false; }
        if (!(in >> token) || token != "PROJECT") { error = "Missing PROJECT record."; return false; }
        in >> std::quoted(loaded.projectName_) >> loaded.canvasWidth_ >> loaded.canvasHeight_;
        std::size_t componentCount = 0; in >> token >> componentCount; if (token != "COMPONENTS") { error = "Missing COMPONENTS record."; return false; }
        for (std::size_t i = 0; i < componentCount; ++i) {
            ComponentId id; std::string type; double x, y; int rotation; bool mh, mv; std::size_t stateCount;
            in >> token >> id >> std::quoted(type) >> x >> y >> rotation >> mh >> mv >> stateCount;
            if (token != "COMP") { error = "Malformed component record."; return false; }
            auto component = createComponentByType(type); if (!component) { error = "Unknown component type: " + type; return false; }
            std::map<std::string, std::string> state;
            for (std::size_t k = 0; k < stateCount; ++k) { std::string key, value; in >> token >> std::quoted(key) >> std::quoted(value); if (token != "PROP") { error = "Malformed property."; return false; } state[key] = value; }
            component->setIdForLoad(id); component->loadPersistentState(state); component->setTransformForLoad({ x, y }, rotation, mh, mv); loaded.components_.push_back(component);
        }
        std::size_t wireCount = 0; in >> token >> wireCount; if (token != "WIRES") { error = "Missing WIRES record."; return false; }
        for (std::size_t i = 0; i < wireCount; ++i) {
            WireId id; ComponentId sc, ec; std::string sp, ep; bool manual; std::size_t pathCount;
            in >> token >> id >> sc >> std::quoted(sp) >> ec >> std::quoted(ep) >> manual >> pathCount;
            if (token != "WIRE") { error = "Malformed wire record."; return false; }
            std::vector<Vec2> path;
            for (std::size_t k = 0; k < pathCount; ++k) { Vec2 p; in >> token >> p.x >> p.y; if (token != "POINT") { error = "Malformed wire point."; return false; } path.push_back(p); }
            auto start = loaded.pinByReference(sc, sp), end = loaded.pinByReference(ec, ep);
            if (!start || !end) { error = "Wire endpoint refers to a missing pin."; return false; }
            auto wire = std::make_shared<Wire>(); wire->setIdForLoad(id); wire->setPins(start, end); wire->setPath(path, manual); loaded.wires_.push_back(wire);
        }
        std::size_t junctionCount = 0; in >> token >> junctionCount; if (token != "JUNCTIONS") { error = "Missing JUNCTIONS record."; return false; }
        for (std::size_t i = 0; i < junctionCount; ++i) { JunctionId id; Vec2 p; in >> token >> id >> p.x >> p.y; if (token != "JUNCTION") { error = "Malformed junction."; return false; } auto j = std::make_shared<Junction>(p); j->setIdForLoad(id); loaded.junctions_.push_back(j); }
        in >> token; if (token != "END") { error = "Project file ended unexpectedly."; return false; }
        loaded.modified_ = false; loaded.buildNetlist(); *this = std::move(loaded); return true;
    }

    bool saveToFile(const std::string& path, std::string& error) {
        std::ofstream out(path, std::ios::binary); if (!out) { error = "Cannot write project file: " + path; return false; }
        out << serializeToString(); if (!out.good()) { error = "Write failure: " + path; return false; } modified_ = false; return true;
    }
    bool loadFromFile(const std::string& path, std::string& error) {
        std::ifstream in(path, std::ios::binary); if (!in) { error = "Cannot open project file: " + path; return false; }
        std::ostringstream data; data << in.rdbuf(); return deserializeFromString(data.str(), error);
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
    int canvasWidth_{ 1600 };
    int canvasHeight_{ 1000 };
    std::string projectName_{ "Untitled" };
    bool modified_{ false };
    std::vector<std::shared_ptr<Component>> components_;
    std::vector<std::shared_ptr<Wire>> wires_;
    std::vector<std::shared_ptr<Junction>> junctions_;
    std::vector<NetNode> nets_;
};


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
        if (document_) for (const auto& component : document_->components()) {
            if (auto clock = std::dynamic_pointer_cast<ClockGenerator>(component)) clock->reset();
            if (auto scope = std::dynamic_pointer_cast<Oscilloscope>(component)) scope->clearSamples();
        }
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

    LogicState logicAtPin(const std::shared_ptr<Pin>& pin) const {
        if (!pin || !document_ || pin->net < 0) return LogicState::Undefined;
        return logicAtNet(pin->net);
    }

private:
    struct Driver {
        LogicState state{ LogicState::Undefined };
        std::string name;
    };

    std::shared_ptr<Component> ownerOf(const std::shared_ptr<Pin>& pin) const {
        return pin && document_ ? document_->componentById(pin->ownerId) : nullptr;
    }

    std::optional<Driver> explicitDriver(const std::shared_ptr<Pin>& pin) const {
        if (!pin) return std::nullopt;
        auto component = ownerOf(pin); if (!component) return std::nullopt;

        if (pin->type == PinType::Ground) return Driver{ LogicState::Low, component->label() + "." + pin->name };
        if (auto clock = std::dynamic_pointer_cast<ClockGenerator>(component)) {
            if (pin->name == "OUT") return Driver{ clock->output() ? LogicState::High : LogicState::Low, component->label() };
        }
        if (auto gate = std::dynamic_pointer_cast<LogicGate>(component)) {
            if (pin->name == "OUT") return Driver{ gate->outputState(), component->label() };
        }
        if (auto flipFlop = std::dynamic_pointer_cast<DFlipFlop>(component)) {
            if (pin->name == "Q") return Driver{ flipFlop->valid() ? (flipFlop->q() ? LogicState::High : LogicState::Low) : LogicState::Undefined, component->label() + ".Q" };
            if (pin->name == "QB") return Driver{ flipFlop->valid() ? (!flipFlop->q() ? LogicState::High : LogicState::Low) : LogicState::Undefined, component->label() + ".QB" };
        }
        if (auto adc = std::dynamic_pointer_cast<SimpleADC>(component)) {
            if (pin->name.size() > 1 && pin->name[0] == 'D') {
                const int bit = std::stoi(pin->name.substr(1)); return Driver{ ((adc->outputCode() >> bit) & 1u) ? LogicState::High : LogicState::Low, component->label() + "." + pin->name };
            }
        }
        if (auto mcu = std::dynamic_pointer_cast<Microcontroller>(component)) {
            char port = '\0'; int bit = -1; bool high = false;
            if (Microcontroller::decodePinName(pin->name, port, bit) && mcu->drivesPortBit(port, bit, high))
                return Driver{ high ? LogicState::High : LogicState::Low, component->label() + "." + pin->name };
        }
        if (auto keypad = std::dynamic_pointer_cast<Keypad>(component)) {
            if (pin->name.size() == 2 && pin->name[0] == 'C') {
                const int column = pin->name[1] - '1';
                std::array<bool, 4> rowLow{};
                for (int i = 0; i < 4; ++i) rowLow[i] = logicAtPin(component->pinByName("R" + std::to_string(i + 1))) == LogicState::Low;
                return Driver{ keypad->columnActive(column, rowLow) ? LogicState::Low : LogicState::High, component->label() + "." + pin->name };
            }
        }
        if (auto memory = std::dynamic_pointer_cast<ExternalMemory>(component)) {
            if (memory->readActive() && pin->name.size() > 1 && pin->name[0] == 'D') {
                const int bit = std::stoi(pin->name.substr(1)); return Driver{ ((memory->readData() >> bit) & 1u) ? LogicState::High : LogicState::Low, component->label() + "." + pin->name };
            }
        }
        return std::nullopt;
    }

    LogicState logicAtNet(int net) const {
        if (!document_ || net < 0 || net >= static_cast<int>(document_->nets().size())) return LogicState::Undefined;
        bool sawDriver = false; LogicState state = LogicState::Undefined;
        for (const auto& pin : document_->nets()[net].pins()) {
            auto driver = explicitDriver(pin); if (!driver) continue;
            if (driver->state == LogicState::Undefined) return LogicState::Undefined;
            if (!sawDriver) { state = driver->state; sawDriver = true; }
            else if (state != driver->state) return LogicState::Undefined;
        }
        if (sawDriver) return state;
        const double voltage = net < static_cast<int>(netVoltages_.size()) ? netVoltages_[net] : 0.0;
        if (voltage <= kLogicLowMax) return LogicState::Low;
        if (voltage >= kLogicHighMin) return LogicState::High;
        return LogicState::Undefined;
    }

    void tickDigital() {
        for (const auto& component : document_->components()) {
            if (auto clock = std::dynamic_pointer_cast<ClockGenerator>(component)) clock->tick(dt_);
            if (auto mcu = std::dynamic_pointer_cast<Microcontroller>(component)) {
                for (char port : {'A', 'B'}) for (int bit = 0; bit < 8; ++bit) {
                    const LogicState value = logicAtPin(component->pinByName(Microcontroller::pinName(port, bit)));
                    if (value != LogicState::Undefined) mcu->sampleInputPortBit(port, bit, value == LogicState::High);
                }
                mcu->tickCpu();
            }
        }

        for (const auto& component : document_->components()) {
            if (auto adc = std::dynamic_pointer_cast<SimpleADC>(component)) {
                adc->setAnalogInput(voltageAtPin(component->pinByName("VIN")), voltageAtPin(component->pinByName("VREF-")), voltageAtPin(component->pinByName("VREF+")));
                adc->tick(dt_);
            }
            if (auto dac = std::dynamic_pointer_cast<SimpleDAC>(component)) {
                std::uint32_t code = 0;
                for (int bit = 0; bit < dac->bits(); ++bit) if (logicAtPin(component->pinByName("D" + std::to_string(bit))) == LogicState::High) code |= 1u << bit;
                dac->setDigitalInput(code, voltageAtPin(component->pinByName("VREF-")), voltageAtPin(component->pinByName("VREF+")));
                dac->tick(dt_);
            }
        }

        for (int pass = 0; pass < 4; ++pass) {
            for (const auto& component : document_->components()) if (auto gate = std::dynamic_pointer_cast<LogicGate>(component)) {
                std::vector<LogicState> inputs;
                for (int i = 0; i < gate->inputCount(); ++i) inputs.push_back(logicAtPin(component->pinByName("IN" + std::to_string(i + 1))));
                if (std::any_of(inputs.begin(), inputs.end(), [](LogicState state) { return state == LogicState::Undefined; }))
                    addMessageUnique("Floating input detected at " + component->label() + ".");
                gate->evaluate(inputs, pass == 0 ? dt_ : 0.0);
            }
        }

        for (const auto& component : document_->components()) if (auto flipFlop = std::dynamic_pointer_cast<DFlipFlop>(component)) {
            const LogicState d = logicAtPin(component->pinByName("D")); const LogicState clk = logicAtPin(component->pinByName("CLK"));
            if (d == LogicState::Undefined || clk == LogicState::Undefined) addMessageUnique("Floating input detected at " + component->label() + ".");
            flipFlop->update(d, clk);
        }
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
            else if (std::dynamic_pointer_cast<Ammeter>(component)) {
                system.addConductance(netOf(component->pinByName("IN")), netOf(component->pinByName("OUT")), 1.0 / 1e-4);
            }
            else if (std::dynamic_pointer_cast<LED>(component)) {
                const double vDiff = voltageAtPin(component->pinByName("A")) - voltageAtPin(component->pinByName("K"));
                const double effectiveResistance = (vDiff > 1.8) ? 330.0 : 1e8;
                system.addConductance(netOf(component->pinByName("A")), netOf(component->pinByName("K")), 1.0 / effectiveResistance);
            }
        }

        for (const auto& component : document_->components()) {
            if (auto clock = std::dynamic_pointer_cast<ClockGenerator>(component)) {
                system.addDrivenVoltage(netOf(component->pinByName("OUT")), groundNet,
                    clock->output() ? 5.0 : 0.0);
            }
            else if (auto gate = std::dynamic_pointer_cast<LogicGate>(component); gate && gate->outputValid()) {
                system.addDrivenVoltage(netOf(component->pinByName("OUT")), groundNet,
                    gate->output() ? 5.0 : 0.0);
            }
            else if (auto flipFlop = std::dynamic_pointer_cast<DFlipFlop>(component); flipFlop && flipFlop->valid()) {
                system.addDrivenVoltage(netOf(component->pinByName("Q")), groundNet,
                    flipFlop->q() ? 5.0 : 0.0);
                system.addDrivenVoltage(netOf(component->pinByName("QB")), groundNet,
                    flipFlop->q() ? 0.0 : 5.0);
            }
            else if (auto adc = std::dynamic_pointer_cast<SimpleADC>(component)) {
                for (int bit = 0; bit < adc->bits(); ++bit) {
                    const bool isHigh = ((adc->outputCode() >> bit) & 1u) != 0;
                    system.addDrivenVoltage(netOf(component->pinByName("D" + std::to_string(bit))), groundNet,
                        isHigh ? 5.0 : 0.0);
                }
            }
            else if (auto dac = std::dynamic_pointer_cast<SimpleDAC>(component)) {
                system.addDrivenVoltage(netOf(component->pinByName("VOUT")), groundNet, dac->analogOutput());
            }
            else if (auto mcu = std::dynamic_pointer_cast<Microcontroller>(component)) {
                for (char port : {'A', 'B'}) {
                    for (int bit = 0; bit < 8; ++bit) {
                        bool outHigh = false;
                        if (!mcu->drivesPortBit(port, bit, outHigh)) continue;
                        system.addDrivenVoltage(netOf(component->pinByName(Microcontroller::pinName(port, bit))),
                            groundNet, outHigh ? 5.0 : 0.0);
                    }
                }
            }
            else if (auto keypad = std::dynamic_pointer_cast<Keypad>(component)) {
                std::array<bool, 4> rowsActive{};
                for (int i = 0; i < 4; ++i)
                    rowsActive[i] = logicAtPin(component->pinByName("R" + std::to_string(i + 1))) == LogicState::Low;

                for (int col = 0; col < 4; ++col) {
                    const bool colActive = keypad->columnActive(col, rowsActive);
                    system.addDrivenVoltage(netOf(component->pinByName("C" + std::to_string(col + 1))),
                        groundNet, colActive ? 0.0 : 5.0);
                }
            }
            else if (auto memory = std::dynamic_pointer_cast<ExternalMemory>(component); memory && memory->readActive()) {
                for (int bit = 0; bit < 8; ++bit) {
                    const bool dHigh = ((memory->readData() >> bit) & 1u) != 0;
                    system.addDrivenVoltage(netOf(component->pinByName("D" + std::to_string(bit))), groundNet,
                        dHigh ? 5.0 : 0.0);
                }
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
            else if (std::dynamic_pointer_cast<Ammeter>(component)) {
                ammeterCurrent_[component->id()] =
                    (voltageAtPin(component->pinByName("IN")) - voltageAtPin(component->pinByName("OUT"))) / 1e-4;
            }
        }
    }
    void updateConsumers() {
        for (const auto& component : document_->components()) {
            if (auto led = std::dynamic_pointer_cast<LED>(component)) led->setOn(voltageAtPin(component->pinByName("A")) - voltageAtPin(component->pinByName("K")) > kLedThreshold);
            else if (auto probe = std::dynamic_pointer_cast<VoltageProbe>(component)) probe->setVoltage(voltageAtPin(component->pinByName("IN")));
            else if (auto voltmeter = std::dynamic_pointer_cast<Voltmeter>(component)) voltmeter->setReading(voltageAtPin(component->pinByName("POS")) - voltageAtPin(component->pinByName("NEG")));
            else if (auto ammeter = std::dynamic_pointer_cast<Ammeter>(component)) ammeter->setReading(ammeterCurrent_[component->id()]);
            else if (auto scope = std::dynamic_pointer_cast<Oscilloscope>(component)) {
                const double reference = voltageAtPin(component->pinByName("GND")); scope->pushSample(voltageAtPin(component->pinByName("CH1")) - reference, voltageAtPin(component->pinByName("CH2")) - reference, time_);
            }
            else if (auto display = std::dynamic_pointer_cast<SevenSegment>(component)) {
                std::uint8_t mask = 0; for (int bit = 0; bit < 8; ++bit) if (logicAtPin(component->pinByName(bit == 7 ? "DP" : std::string(1, static_cast<char>('A' + bit)))) == LogicState::High) mask |= static_cast<std::uint8_t>(1u << bit); display->setMask(mask);
            }
            else if (auto lcd = std::dynamic_pointer_cast<LCD16x2>(component)) {
                std::uint8_t data = 0; for (int bit = 0; bit < 8; ++bit) if (logicAtPin(component->pinByName("D" + std::to_string(bit))) == LogicState::High) data |= static_cast<std::uint8_t>(1u << bit);
                lcd->tickBus(logicAtPin(component->pinByName("RS")) == LogicState::High, logicAtPin(component->pinByName("RW")) == LogicState::High,
                    logicAtPin(component->pinByName("E")) == LogicState::High, data);
            }
            else if (auto memory = std::dynamic_pointer_cast<ExternalMemory>(component)) {
                std::uint16_t address = 0; std::uint8_t data = 0;
                for (int bit = 0; bit < 8; ++bit) { if (logicAtPin(component->pinByName("A" + std::to_string(bit))) == LogicState::High) address |= static_cast<std::uint16_t>(1u << bit); if (logicAtPin(component->pinByName("D" + std::to_string(bit))) == LogicState::High) data |= static_cast<std::uint8_t>(1u << bit); }
                const bool readActive = logicAtPin(component->pinByName("RD")) == LogicState::Low; const bool writeActive = logicAtPin(component->pinByName("WR")) == LogicState::Low;
                memory->setReadBus(address, readActive); if (writeActive) memory->write(address, data);
            }
        }
    }

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

    void tick() {
        if (!document_) return;
        messages_.clear(); document_->buildNetlist(); tickDigital(); solveAnalog(); updateConsumers(); updateWireValues();
        for (const auto& warning : document_->runDRC()) addMessageUnique(warning);
        time_ += dt_;
    }

    CircuitDocument* document_{ nullptr };
    SimulationState state_{ SimulationState::Stopped };
    double dt_{ 0.01 };
    double time_{ 0.0 };
    double accumulatorWall_{ 0.0 };
    std::vector<double> netVoltages_;
    std::unordered_map<const Pin*, double> pinVoltages_;
    std::unordered_map<ComponentId, double> capacitorVoltageHistory_;
    std::unordered_map<ComponentId, double> inductorCurrentHistory_;
    std::unordered_map<ComponentId, double> ammeterCurrent_;
    std::unordered_map<WireId, int> wireLogicValues_;
    std::vector<std::string> messages_;
};
//Snapshot-based undo/redo (Command semantics over complete document state)
class HistoryManager {
public:
    explicit HistoryManager(CircuitDocument* document) : document_(document) {}

    void reset() { undo_.clear(); redo_.clear(); pendingBefore_.reset(); }
    void begin(const std::string& description) {
        if (!document_ || pendingBefore_) return;
        pendingBefore_ = document_->serializeToString();
        pendingDescription_ = description;
    }
    void commit() {
        if (!document_ || !pendingBefore_) return;
        const std::string after = document_->serializeToString();
        if (after != *pendingBefore_) { undo_.push_back({ pendingDescription_, *pendingBefore_, after }); if (undo_.size() > 100) undo_.erase(undo_.begin()); redo_.clear(); }
        pendingBefore_.reset(); pendingDescription_.clear();
    }
    void cancel() { pendingBefore_.reset(); pendingDescription_.clear(); }
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    std::string undoDescription() const { return canUndo() ? undo_.back().description : std::string(); }
    std::string redoDescription() const { return canRedo() ? redo_.back().description : std::string(); }
    bool undo(std::string& error) {
        if (!document_ || undo_.empty()) return false;
        Entry entry = undo_.back();
        undo_.pop_back();
        if (!document_->deserializeFromString(entry.before, error)) return false;
        redo_.push_back(std::move(entry));
        return true;
    }
    bool redo(std::string& error) {
        if (!document_ || redo_.empty()) return false;
        Entry entry = redo_.back();
        redo_.pop_back();
        if (!document_->deserializeFromString(entry.after, error)) return false;
        undo_.push_back(std::move(entry));
        return true;
    }
private:
    struct Entry { std::string description, before, after; };
    CircuitDocument* document_{ nullptr };
    std::vector<Entry> undo_, redo_;
    std::optional<std::string> pendingBefore_;
    std::string pendingDescription_;
};
//SDL2 user interface
static bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

static void fillRect(SDL_Renderer* renderer, SDL_Rect rect, Color color) {
    setRenderColor(renderer, color); SDL_RenderFillRect(renderer, &rect);
}

static void outlineRect(SDL_Renderer* renderer, SDL_Rect rect, Color color) {
    setRenderColor(renderer, color); SDL_RenderDrawRect(renderer, &rect);
}

struct UiButton {
    SDL_Rect rect{};
    std::string label;
    bool enabled{ true };
    bool active{ false };
};

static void drawButton(SDL_Renderer* renderer, TextRenderer& text, const UiButton& button,
    int mouseX, int mouseY, int fontSize = 13) {
    const bool hovered = button.enabled && pointInRect(mouseX, mouseY, button.rect);
    Color fill = button.active ? Color{ 36, 132, 172, 255 } : (hovered ? Color{ 68, 75, 88, 255 } : Palette::Panel2);
    if (!button.enabled) fill = Color{ 45, 48, 55, 255 };
    fillRect(renderer, button.rect, fill);
    outlineRect(renderer, button.rect, button.active ? Palette::Accent : Color{ 85, 92, 104, 255 });
    auto [tw, th] = text.measure(button.label, fontSize);
    text.draw(button.label, button.rect.x + (button.rect.w - tw) / 2,
        button.rect.y + (button.rect.h - th) / 2,
        button.enabled ? Palette::Text : Color{ 105, 110, 120, 255 }, fontSize);
}

struct RecentProject {
    std::string path;
    std::string timestamp;
};

enum class EditorMode { Select, Place, Wire, Junction, Probe };
enum class ScreenState { StartMenu, Editor };
enum class ModalKind { None, NewProject, FileDialog, Help, About, Scope };
enum class FileOperation { OpenProject, SaveProject, ExportImage, LoadFirmware };

struct FileDialogData {
    FileOperation operation{ FileOperation::OpenProject };
    fs::path directory;
    std::vector<fs::directory_entry> entries;
    int scroll{ 0 };
    int selected{ -1 };
    std::string pathText;
    bool editingPath{ false };
    Uint32 lastClickTime{ 0 };
    int lastClickIndex{ -1 };
};

struct NewProjectData {
    std::string name{ "Untitled" };
    std::string width{ "1600" };
    std::string height{ "1000" };
    int activeField{ 0 };
};

struct PropertyRow {
    PropertyDescriptor descriptor;
    SDL_Rect rect{};
};

class ProteusApp {
public:
    ProteusApp() : simulation_(&document_), history_(&document_) {}
    ~ProteusApp() { shutdown(); }

    bool initialize() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n"; return false;
        }
        if (TTF_Init() != 0) {
            std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n"; return false;
        }
        window_ = SDL_CreateWindow("ProteusClone SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1440, 900, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window_) { std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n"; return false; }
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
        if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
        if (!renderer_) { std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n"; return false; }
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        fonts_.initialize(); text_ = std::make_unique<TextRenderer>(renderer_, &fonts_);
        SDL_StartTextInput(); loadRecentProjects();
        categoryCollapsed_[ComponentCategory::Sources] = false;
        categoryCollapsed_[ComponentCategory::Passive] = false;
        categoryCollapsed_[ComponentCategory::Interactive] = false;
        categoryCollapsed_[ComponentCategory::Digital] = false;
        categoryCollapsed_[ComponentCategory::Advanced] = false;
        categoryCollapsed_[ComponentCategory::Peripheral] = false;
        categoryCollapsed_[ComponentCategory::Measurement] = false;
        return true;
    }

    int run() {
        if (!window_ || !renderer_) return 1;
        bool running = true;
        Uint64 previousCounter = SDL_GetPerformanceCounter();
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; break; }
                handleEvent(event, running);
            }
            const Uint64 now = SDL_GetPerformanceCounter();
            const double elapsed = static_cast<double>(now - previousCounter) / SDL_GetPerformanceFrequency();
            previousCounter = now;
            simulation_.update(elapsed);
            render();
        }
        return 0;
    }

private:
    // Lifecycle and layout
    void shutdown() {
        SDL_StopTextInput(); text_.reset(); fonts_.clear();
        if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
        if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
        if (TTF_WasInit()) TTF_Quit();
        SDL_Quit();
    }

    void updateLayout() {
        SDL_GetRendererOutputSize(renderer_, &windowWidth_, &windowHeight_);
        toolbarRect_ = { 0, 0, windowWidth_, 48 };
        statusRect_ = { 0, windowHeight_ - 25, windowWidth_, 25 };
        libraryRect_ = { 0, 48, 260, windowHeight_ - 48 - 25 };
        propertiesRect_ = { windowWidth_ - 300, 48, 300, windowHeight_ - 48 - 25 };
        logRect_ = { 260, windowHeight_ - 185, windowWidth_ - 260 - 300, 160 };
        canvasRect_ = { 260, 48, windowWidth_ - 260 - 300, windowHeight_ - 48 - 25 - 160 };
        if (canvasRect_.w < 200) canvasRect_.w = 200;
        if (canvasRect_.h < 150) canvasRect_.h = 150;
        camera_.viewport = canvasRect_;
    }

    // Recent projects and notifications
    fs::path recentConfigPath() const {
#ifdef _WIN32
        char* appData = nullptr;
        std::size_t length = 0;

        if (_dupenv_s(&appData, &length, "APPDATA") == 0 && appData != nullptr) {
            fs::path result =
                fs::path(appData) / ".proteusclone_sdl2_recent";

            std::free(appData);
            return result;
        }

        if (appData != nullptr) {
            std::free(appData);
        }

        return fs::path(".proteusclone_sdl2_recent");
#else
        const char* home = std::getenv("HOME");

        return home
            ? fs::path(home) / ".proteusclone_sdl2_recent"
            : fs::path(".proteusclone_sdl2_recent");
#endif
    }

    void loadRecentProjects() {
        recentProjects_.clear(); std::ifstream in(recentConfigPath()); std::string path, timestamp;
        while (in >> std::quoted(path) >> std::quoted(timestamp)) if (fs::exists(path)) recentProjects_.push_back({ path, timestamp });
        if (recentProjects_.size() > 5) recentProjects_.resize(5);
    }

    void saveRecentProjects() {
        std::ofstream out(recentConfigPath(), std::ios::trunc);
        for (const auto& recent : recentProjects_) out << std::quoted(recent.path) << ' ' << std::quoted(recent.timestamp) << "\n";
    }

    void addRecentProject(const std::string& path) {
        recentProjects_.erase(std::remove_if(recentProjects_.begin(), recentProjects_.end(), [&](const RecentProject& r) { return r.path == path; }), recentProjects_.end());
        recentProjects_.insert(recentProjects_.begin(), { path, nowTimestamp() }); if (recentProjects_.size() > 5) recentProjects_.resize(5); saveRecentProjects();
    }

    void notify(std::string message, Color color = Palette::Accent2) {
        notifications_.push_front({ std::move(message), color, SDL_GetTicks() }); if (notifications_.size() > 5) notifications_.pop_back();
    }

    void appendLog(const std::string& message) {
        if (!message.empty() && (logs_.empty() || logs_.back() != message)) logs_.push_back(message);
        while (logs_.size() > 200) logs_.pop_front();
    }
    // File/new project operations
    void newProject(int width, int height, const std::string& name) {
        simulation_.stop(); document_.clear(); document_.setProjectName(name.empty() ? "Untitled" : name); document_.setCanvasSize(width, height); document_.setModified(false);
        history_.reset(); currentFile_.clear(); clearSelection(); screen_ = ScreenState::Editor; mode_ = EditorMode::Select;
        camera_.zoom = 1.0; camera_.pan = { 40, 40 }; logs_.clear(); appendLog("New project created: " + document_.projectName());
        notify("New project created");
    }

    bool openProject(const std::string& path) {
        std::string error; simulation_.stop();
        if (!document_.loadFromFile(path, error)) { notify(error, Palette::Error); appendLog(error); return false; }
        currentFile_ = path; addRecentProject(path); history_.reset(); clearSelection(); screen_ = ScreenState::Editor; mode_ = EditorMode::Select;
        camera_.zoom = 1.0; camera_.pan = { 40, 40 }; logs_.clear(); appendLog("Loaded project: " + path); notify("Project loaded"); return true;
    }

    bool saveProject(const std::string& path) {
        std::string error;
        if (!document_.saveToFile(path, error)) { notify(error, Palette::Error); appendLog(error); return false; }
        currentFile_ = path; addRecentProject(path); notify("Project saved"); appendLog("Saved: " + path); return true;
    }

    void openFileDialog(FileOperation operation) {
        modal_ = ModalKind::FileDialog; fileDialog_ = {};
        fileDialog_.operation = operation;
        try { fileDialog_.directory = currentFile_.empty() ? fs::current_path() : fs::path(currentFile_).parent_path(); }
        catch (...) { fileDialog_.directory = fs::current_path(); }
        if (operation == FileOperation::SaveProject) fileDialog_.pathText = currentFile_.empty() ? (document_.projectName() + ".pcsdl") : currentFile_;
        else if (operation == FileOperation::ExportImage) fileDialog_.pathText = document_.projectName() + ".bmp";
        refreshFileEntries();
    }

    void refreshFileEntries() {
        fileDialog_.entries.clear(); fileDialog_.selected = -1; fileDialog_.scroll = 0;
        try {
            for (const auto& entry : fs::directory_iterator(fileDialog_.directory)) fileDialog_.entries.push_back(entry);
            std::sort(fileDialog_.entries.begin(), fileDialog_.entries.end(), [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) return a.is_directory() > b.is_directory();
                return toLower(a.path().filename().string()) < toLower(b.path().filename().string());
                });
        }
        catch (const std::exception& exception) { notify(exception.what(), Palette::Error); }
    }

    void confirmFileDialog() {
        fs::path selectedPath;
        if (!fileDialog_.pathText.empty()) {
            selectedPath = fs::path(fileDialog_.pathText); if (selectedPath.is_relative()) selectedPath = fileDialog_.directory / selectedPath;
        }
        else if (fileDialog_.selected >= 0 && fileDialog_.selected < static_cast<int>(fileDialog_.entries.size())) selectedPath = fileDialog_.entries[fileDialog_.selected].path();
        if (selectedPath.empty()) return;
        if (fs::is_directory(selectedPath)) { fileDialog_.directory = selectedPath; fileDialog_.pathText.clear(); refreshFileEntries(); return; }
        bool close = true;
        switch (fileDialog_.operation) {
        case FileOperation::OpenProject: close = openProject(selectedPath.string()); break;
        case FileOperation::SaveProject: close = saveProject(selectedPath.string()); break;
        case FileOperation::ExportImage: close = exportCanvasBmp(selectedPath.string()); break;
        case FileOperation::LoadFirmware: {
            auto selected = singleSelectedComponent(); auto mcu = std::dynamic_pointer_cast<Microcontroller>(selected);
            if (!mcu) { notify("Select a Microcontroller first.", Palette::Warning); close = false; break; }
            std::string error; history_.begin("Load MCU firmware");
            if (mcu->loadIntelHex(selectedPath.string(), error)) { history_.commit(); notify("Firmware loaded"); appendLog("Loaded firmware: " + selectedPath.string()); }
            else { history_.cancel(); notify(error, Palette::Error); appendLog(error); close = false; }
            break;
        }
        }
        if (close) modal_ = ModalKind::None;
    }

    bool exportCanvasBmp(std::string path) {
        if (toLower(fs::path(path).extension().string()) != ".bmp") path += ".bmp";
        const int width = clampValue(document_.canvasWidth(), 400, 4096);
        const int height = clampValue(document_.canvasHeight(), 300, 4096);
        SDL_Texture* target = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
        if (!target) { notify(std::string("Export texture failed: ") + SDL_GetError(), Palette::Error); return false; }
        SDL_Texture* previous = SDL_GetRenderTarget(renderer_); SDL_SetRenderTarget(renderer_, target);
        Camera exportCamera; exportCamera.viewport = { 0, 0, width, height };
        const double scaleX = width / static_cast<double>(document_.canvasWidth()); const double scaleY = height / static_cast<double>(document_.canvasHeight());
        exportCamera.zoom = std::min(scaleX, scaleY); exportCamera.pan = { 0, 0 };
        renderCanvasContent(exportCamera, true, false);
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        bool ok = surface && SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) == 0 && SDL_SaveBMP(surface, path.c_str()) == 0;
        if (surface) SDL_FreeSurface(surface);
        SDL_SetRenderTarget(renderer_, previous);
        SDL_DestroyTexture(target);
        if (ok) { notify("Canvas exported to BMP"); appendLog("Exported image: " + path); }
        else notify(std::string("Export failed: ") + SDL_GetError(), Palette::Error);
        return ok;
    }

    // Event dispatch
    void handleEvent(const SDL_Event& event, bool& running) {
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) updateLayout();
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            if (modal_ != ModalKind::None) { modal_ = ModalKind::None; return; }
            if (contextMenuOpen_) { contextMenuOpen_ = false; return; }
            if (wireStart_) { cancelWire(); return; }
            if (screen_ == ScreenState::Editor) { mode_ = EditorMode::Select; return; }
        }
        if (modal_ != ModalKind::None) { handleModalEvent(event); return; }
        if (screen_ == ScreenState::StartMenu) { handleStartMenuEvent(event, running); return; }
        handleEditorEvent(event, running);
    }

    void handleStartMenuEvent(const SDL_Event& event, bool& running) {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_n) { modal_ = ModalKind::NewProject; newProjectData_ = {}; }
            if (event.key.keysym.sym == SDLK_o) openFileDialog(FileOperation::OpenProject);
            if (event.key.keysym.sym == SDLK_q) running = false;
        }
        if (event.type != SDL_MOUSEBUTTONDOWN || event.button.button != SDL_BUTTON_LEFT) return;
        const int x = event.button.x, y = event.button.y;
        SDL_Rect newButton{ windowWidth_ / 2 - 210, 280, 200, 48 };
        SDL_Rect openButton{ windowWidth_ / 2 + 10, 280, 200, 48 };
        if (pointInRect(x, y, newButton)) { modal_ = ModalKind::NewProject; newProjectData_ = {}; return; }
        if (pointInRect(x, y, openButton)) { openFileDialog(FileOperation::OpenProject); return; }
        int ry = 390;
        for (const auto& recent : recentProjects_) {
            SDL_Rect row{ windowWidth_ / 2 - 360, ry, 720, 46 };
            if (pointInRect(x, y, row)) { openProject(recent.path); return; }
            ry += 52;
        }
    }

    void handleEditorEvent(const SDL_Event& event, bool& running) {
        const bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        const bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        if (event.type == SDL_KEYDOWN) {
            const SDL_Keycode key = event.key.keysym.sym;
            if (ctrl && key == SDLK_n) { modal_ = ModalKind::NewProject; newProjectData_ = {}; return; }
            if (ctrl && key == SDLK_o) { openFileDialog(FileOperation::OpenProject); return; }
            if (ctrl && key == SDLK_s) { if (currentFile_.empty()) openFileDialog(FileOperation::SaveProject); else saveProject(currentFile_); return; }
            if (ctrl && key == SDLK_e) { openFileDialog(FileOperation::ExportImage); return; }
            if (ctrl && key == SDLK_z) { std::string error; simulation_.pause(); if (!history_.undo(error) && !error.empty()) notify(error, Palette::Error); clearSelection(); return; }
            if (ctrl && (key == SDLK_y || (shift && key == SDLK_z))) { std::string error; simulation_.pause(); if (!history_.redo(error) && !error.empty()) notify(error, Palette::Error); clearSelection(); return; }
            if (ctrl && key == SDLK_q) { running = false; return; }
            if (key == SDLK_F1) { modal_ = ModalKind::Help; return; }
            if (key == SDLK_F5) { simulation_.start(); notify("Simulation running"); return; }
            if (key == SDLK_F6) { simulation_.pause(); notify("Simulation paused"); return; }
            if (key == SDLK_F7) { simulation_.stop(); notify("Simulation stopped"); return; }
            if (key == SDLK_F8) { simulation_.step(); notify("Simulation stepped"); return; }
            if (propertyEditing_) { handlePropertyKey(event.key); return; }
            if (searchFocused_) { handleSearchKey(event.key); return; }
            if (key == SDLK_DELETE || key == SDLK_BACKSPACE) { deleteSelection(); return; }
            if (key == SDLK_r) { transformSelection("Rotate", [](Component& c) { c.rotate90(); }); return; }
            if (key == SDLK_h) { transformSelection("Mirror horizontal", [](Component& c) { c.mirrorHorizontal(); }); return; }
            if (key == SDLK_v) { transformSelection("Mirror vertical", [](Component& c) { c.mirrorVertical(); }); return; }
            if (key == SDLK_w) { mode_ = EditorMode::Wire; cancelWire(); return; }
            if (key == SDLK_s && !ctrl) { mode_ = EditorMode::Select; return; }
            if (key == SDLK_j) { mode_ = EditorMode::Junction; return; }
            if (key == SDLK_p) { mode_ = EditorMode::Probe; return; }
            if (key == SDLK_g) { runDRC(); return; }
            if (key == SDLK_o && !ctrl) { if (std::dynamic_pointer_cast<Oscilloscope>(singleSelectedComponent())) modal_ = ModalKind::Scope; return; }
            if (key == SDLK_SPACE) spaceHeld_ = true;
        }
        if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_SPACE) spaceHeld_ = false;
        if (event.type == SDL_TEXTINPUT) {
            if (propertyEditing_) propertyEditBuffer_ += event.text.text;
            else if (searchFocused_) searchText_ += event.text.text;
        }
        if (event.type == SDL_MOUSEMOTION) handleEditorMouseMotion(event.motion);
        if (event.type == SDL_MOUSEWHEEL) handleEditorMouseWheel(event.wheel);
        if (event.type == SDL_MOUSEBUTTONDOWN) handleEditorMouseDown(event.button);
        if (event.type == SDL_MOUSEBUTTONUP) handleEditorMouseUp(event.button);
    }

    void handleModalEvent(const SDL_Event& event) {
        if (modal_ == ModalKind::Help || modal_ == ModalKind::About || modal_ == ModalKind::Scope) {
            if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_ESCAPE)) modal_ = ModalKind::None;
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_Rect close{ windowWidth_ / 2 + 370, 80, 30, 30 }; if (pointInRect(event.button.x, event.button.y, close)) modal_ = ModalKind::None;
            }
            return;
        }
        if (modal_ == ModalKind::NewProject) { handleNewProjectModal(event); return; }
        if (modal_ == ModalKind::FileDialog) { handleFileDialogModal(event); return; }
    }

    void handleNewProjectModal(const SDL_Event& event) {
        if (event.type == SDL_TEXTINPUT) {
            std::string* target = newProjectData_.activeField == 0 ? &newProjectData_.name : (newProjectData_.activeField == 1 ? &newProjectData_.width : &newProjectData_.height);
            *target += event.text.text;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_TAB) newProjectData_.activeField = (newProjectData_.activeField + 1) % 3;
            if (event.key.keysym.sym == SDLK_BACKSPACE) {
                std::string* target = newProjectData_.activeField == 0 ? &newProjectData_.name : (newProjectData_.activeField == 1 ? &newProjectData_.width : &newProjectData_.height);
                if (!target->empty()) target->pop_back();
            }
            if (event.key.keysym.sym == SDLK_RETURN) {
                try { newProject(std::stoi(newProjectData_.width), std::stoi(newProjectData_.height), newProjectData_.name); modal_ = ModalKind::None; }
                catch (...) { notify("Invalid canvas dimensions.", Palette::Error); }
            }
        }
        if (event.type != SDL_MOUSEBUTTONDOWN || event.button.button != SDL_BUTTON_LEFT) return;
        const int x = event.button.x, y = event.button.y;
        const int cx = windowWidth_ / 2, cy = windowHeight_ / 2;
        SDL_Rect name{ cx - 200, cy - 100, 400, 34 }, width{ cx - 200, cy - 52, 190, 34 }, height{ cx + 10, cy - 52, 190, 34 };
        if (pointInRect(x, y, name)) newProjectData_.activeField = 0;
        else if (pointInRect(x, y, width)) newProjectData_.activeField = 1;
        else if (pointInRect(x, y, height)) newProjectData_.activeField = 2;
        SDL_Rect a4{ cx - 200, cy + 4, 120, 34 }, a3{ cx - 65, cy + 4, 120, 34 }, hd{ cx + 70, cy + 4, 130, 34 };
        if (pointInRect(x, y, a4)) { newProjectData_.width = "1120"; newProjectData_.height = "800"; }
        if (pointInRect(x, y, a3)) { newProjectData_.width = "1600"; newProjectData_.height = "1120"; }
        if (pointInRect(x, y, hd)) { newProjectData_.width = "1920"; newProjectData_.height = "1080"; }
        SDL_Rect create{ cx + 20, cy + 65, 180, 42 }, cancel{ cx - 200, cy + 65, 180, 42 };
        if (pointInRect(x, y, cancel)) modal_ = ModalKind::None;
        if (pointInRect(x, y, create)) {
            try { newProject(std::stoi(newProjectData_.width), std::stoi(newProjectData_.height), newProjectData_.name); modal_ = ModalKind::None; }
            catch (...) { notify("Invalid canvas dimensions.", Palette::Error); }
        }
    }

    void handleFileDialogModal(const SDL_Event& event) {
        if (event.type == SDL_TEXTINPUT && fileDialog_.editingPath) fileDialog_.pathText += event.text.text;
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_BACKSPACE && fileDialog_.editingPath && !fileDialog_.pathText.empty()) fileDialog_.pathText.pop_back();
            if (event.key.keysym.sym == SDLK_RETURN) confirmFileDialog();
            if (event.key.keysym.sym == SDLK_UP && fileDialog_.selected > 0) --fileDialog_.selected;
            if (event.key.keysym.sym == SDLK_DOWN && fileDialog_.selected + 1 < static_cast<int>(fileDialog_.entries.size())) ++fileDialog_.selected;
        }
        if (event.type == SDL_MOUSEWHEEL) fileDialog_.scroll = clampValue(fileDialog_.scroll - event.wheel.y * 3, 0, std::max(0, static_cast<int>(fileDialog_.entries.size()) - 12));
        if (event.type != SDL_MOUSEBUTTONDOWN || event.button.button != SDL_BUTTON_LEFT) return;
        const int cx = windowWidth_ / 2, cy = windowHeight_ / 2;
        SDL_Rect pathField{ cx - 360, cy - 255, 720, 34 };
        if (pointInRect(event.button.x, event.button.y, pathField)) { fileDialog_.editingPath = true; return; }
        fileDialog_.editingPath = false;
        SDL_Rect up{ cx - 360, cy - 210, 80, 30 };
        if (pointInRect(event.button.x, event.button.y, up)) { if (fileDialog_.directory.has_parent_path()) { fileDialog_.directory = fileDialog_.directory.parent_path(); refreshFileEntries(); } return; }
        int y = cy - 170;
        for (int row = 0; row < 12; ++row, y += 32) {
            const int index = fileDialog_.scroll + row; if (index >= static_cast<int>(fileDialog_.entries.size())) break;
            SDL_Rect r{ cx - 360, y, 720, 30 };
            if (pointInRect(event.button.x, event.button.y, r)) {
                const Uint32 now = SDL_GetTicks(); const bool doubleClick = fileDialog_.lastClickIndex == index && now - fileDialog_.lastClickTime < 450;
                fileDialog_.selected = index; fileDialog_.pathText = fileDialog_.entries[index].path().filename().string();
                fileDialog_.lastClickIndex = index; fileDialog_.lastClickTime = now;
                if (doubleClick) { if (fileDialog_.entries[index].is_directory()) { fileDialog_.directory = fileDialog_.entries[index].path(); fileDialog_.pathText.clear(); refreshFileEntries(); } else confirmFileDialog(); }
                return;
            }
        }
        SDL_Rect confirm{ cx + 180, cy + 225, 180, 40 }, cancel{ cx - 360, cy + 225, 180, 40 };
        if (pointInRect(event.button.x, event.button.y, confirm)) confirmFileDialog();
        if (pointInRect(event.button.x, event.button.y, cancel)) modal_ = ModalKind::None;
    }
    // Editor mouse and keyboard details
    void handleSearchKey(const SDL_KeyboardEvent& key) {
        if (key.keysym.sym == SDLK_BACKSPACE && !searchText_.empty()) searchText_.pop_back();
        if (key.keysym.sym == SDLK_RETURN || key.keysym.sym == SDLK_ESCAPE) searchFocused_ = false;
    }

    void handlePropertyKey(const SDL_KeyboardEvent& key) {
        if (key.keysym.sym == SDLK_BACKSPACE && !propertyEditBuffer_.empty()) propertyEditBuffer_.pop_back();
        if (key.keysym.sym == SDLK_ESCAPE) { propertyEditing_ = false; return; }
        if (key.keysym.sym == SDLK_RETURN) applyEditedProperty();
    }

    void applyEditedProperty() {
        auto component = singleSelectedComponent();
        if (!component || propertyEditIndex_ < 0 || propertyEditIndex_ >= static_cast<int>(propertyRows_.size())) { propertyEditing_ = false; return; }
        const auto key = propertyRows_[propertyEditIndex_].descriptor.key;
        try {
            history_.begin("Edit property " + key);
            if (component->setProperty(key, propertyEditBuffer_)) { history_.commit(); document_.setModified(true); document_.rerouteConnectedWires(); notify("Property updated"); }
            else history_.cancel();
        }
        catch (const std::exception& exception) { history_.cancel(); notify(std::string("Invalid property: ") + exception.what(), Palette::Error); }
        propertyEditing_ = false;
    }

    void handleEditorMouseDown(const SDL_MouseButtonEvent& button) {
        mouseX_ = button.x; mouseY_ = button.y;
        if (contextMenuOpen_ && button.button == SDL_BUTTON_LEFT) { handleContextMenuClick(button.x, button.y); return; }
        if (button.button == SDL_BUTTON_LEFT) {
            buildToolbarButtons();
            for (const auto& pair : toolbarButtons_) if (pointInRect(button.x, button.y, pair.first.rect)) { executeToolbarAction(pair.second); return; }
            if (pointInRect(button.x, button.y, libraryRect_)) { handleLibraryClick(button.x, button.y, button.clicks); return; }
            if (pointInRect(button.x, button.y, propertiesRect_)) { handlePropertiesClick(button.x, button.y); return; }
        }
        if (button.button == SDL_BUTTON_RIGHT && pointInRect(button.x, button.y, libraryRect_)) {
            handleLibraryRightClick(button.x, button.y);
            return;
        }
        if (!pointInRect(button.x, button.y, canvasRect_)) return;
        const Vec2 world = camera_.screenToWorld({ static_cast<double>(button.x), static_cast<double>(button.y) });

        if (button.button == SDL_BUTTON_MIDDLE || (button.button == SDL_BUTTON_LEFT && spaceHeld_)) {
            panning_ = true; panStartScreen_ = { static_cast<double>(button.x), static_cast<double>(button.y) }; panStartOffset_ = camera_.pan; return;
        }
        if (button.button == SDL_BUTTON_RIGHT) {
            if (mode_ == EditorMode::Wire && wireStart_) { cancelWire(); return; }
            auto component = document_.componentAt(world); auto wire = document_.wireAt(world, 7.0); auto junction = document_.junctionAt(world);
            if (component) { if (!selectedComponents_.count(component->id())) { clearSelection(); selectedComponents_.insert(component->id()); } }
            else if (wire) { clearSelection(); selectedWire_ = wire->id(); wire->setSelected(true); }
            else if (junction) { clearSelection(); selectedJunction_ = junction->id(); junction->setSelected(true); }
            contextMenuOpen_ = component || wire || junction; contextMenuPosition_ = { button.x, button.y }; return;
        }
        if (button.button != SDL_BUTTON_LEFT) return;

        if (mode_ == EditorMode::Place) { placeComponent(world); return; }
        if (mode_ == EditorMode::Wire) { handleWireClick(world); return; }
        if (mode_ == EditorMode::Junction) { history_.begin("Add junction"); const std::size_t before = document_.junctions().size(); document_.addJunction(world); if (document_.junctions().size() != before) { history_.commit(); notify("Junction added"); } else { history_.cancel(); notify("A junction requires at least two crossing wires.", Palette::Warning); } return; }
        if (mode_ == EditorMode::Probe) { const double voltage = simulation_.voltageAtWorld(world); appendLog("Probe at (" + std::to_string(static_cast<int>(world.x)) + ", " + std::to_string(static_cast<int>(world.y)) + ") = " + formatDouble(voltage) + " V"); notify("Voltage: " + formatDouble(voltage) + " V"); return; }

        const bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
        auto component = document_.componentAt(world);
        if (component) {
            // Live interactions remain available while the simulator is running or paused.
            if (auto sw = std::dynamic_pointer_cast<Switch>(component)) { history_.begin("Toggle switch"); sw->toggle(); history_.commit(); document_.setModified(true); }
            if (auto buttonComponent = std::dynamic_pointer_cast<PushButton>(component)) { buttonComponent->setPressed(true); heldPushButton_ = buttonComponent; }
            if (!ctrl && !selectedComponents_.count(component->id())) clearSelection();
            if (ctrl && selectedComponents_.count(component->id())) selectedComponents_.erase(component->id()); else selectedComponents_.insert(component->id());
            selectedWire_.reset(); selectedJunction_.reset();
            if (selectedComponents_.count(component->id())) beginComponentDrag(world);
            if (button.clicks >= 2) { propertyEditing_ = false; notify("Properties are shown in the right panel."); }
            return;
        }
        if (auto wire = document_.wireAt(world, 7.0)) { clearSelection(); selectedWire_ = wire->id(); wire->setSelected(true); return; }
        if (auto junction = document_.junctionAt(world)) { clearSelection(); selectedJunction_ = junction->id(); junction->setSelected(true); return; }
        if (!ctrl) clearSelection();
        selectingRectangle_ = true;
        selectionStartWorld_ = world;
        selectionEndWorld_ = world;
    }

    void handleEditorMouseMotion(const SDL_MouseMotionEvent& motion) {
        mouseX_ = motion.x; mouseY_ = motion.y;
        const Vec2 screen{ static_cast<double>(motion.x), static_cast<double>(motion.y) };
        const Vec2 world = camera_.screenToWorld(screen); lastMouseWorld_ = world;
        for (const auto& component : document_.components()) if (component) for (const auto& pin : component->pins()) if (pin) pin->highlighted = distance(world, pin->worldPosition) <= kPinHoverRadius / camera_.zoom + 3.0;
        if (panning_) { camera_.pan = panStartOffset_ + (screen - panStartScreen_); return; }
        if (draggingComponents_) {
            Vec2 delta = snapToGrid(world) - snapToGrid(dragStartWorld_);
            for (const auto& [id, original] : dragOriginalPositions_) if (auto component = document_.componentById(id)) component->moveTo(original + delta);
            document_.rerouteConnectedWires(); document_.setModified(true); return;
        }
        if (selectingRectangle_) selectionEndWorld_ = world;
        if (wireStart_) wireCurrentWorld_ = snapToGrid(world);
    }

    void handleEditorMouseUp(const SDL_MouseButtonEvent& button) {
        if (button.button == SDL_BUTTON_MIDDLE || button.button == SDL_BUTTON_LEFT) panning_ = false;
        if (button.button == SDL_BUTTON_LEFT && draggingComponents_) { draggingComponents_ = false; history_.commit(); dragOriginalPositions_.clear(); }
        if (button.button == SDL_BUTTON_LEFT && selectingRectangle_) {
            selectingRectangle_ = false; const RectD selection = normalizedRect(selectionStartWorld_, selectionEndWorld_);
            for (const auto& component : document_.components()) if (component && selection.intersects(component->worldBounds())) selectedComponents_.insert(component->id());
        }
        if (button.button == SDL_BUTTON_LEFT && heldPushButton_) { heldPushButton_->setPressed(false); heldPushButton_.reset(); }
    }
