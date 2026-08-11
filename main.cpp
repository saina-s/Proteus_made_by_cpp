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

void Component::draw(CanvasPainter& painter, bool selected) const {
    const Color outline = selected ? Palette::Selection : Palette::DarkText;
    painter.rect(worldBounds(), outline, false);
    drawPins(painter);
}

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


    std::shared_ptr<Component> componentById(ComponentId id) const {
        for (const auto& c : components_) if (c && c->id() == id) return c;
        return nullptr;
    }

    std::shared_ptr<Component> componentAt(Vec2 world) const {
        for (auto it = components_.rbegin(); it != components_.rend(); ++it) if (*it && (*it)->hitTest(world)) return *it;
        return nullptr;
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

