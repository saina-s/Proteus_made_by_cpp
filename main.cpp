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
        std::vector<Vec2> points{ P(r.left(), r.top()), P(r.right(), r.top()), P(r.right(), r.bottom()), P(r.left(), r.bottom()), P(r.left(), r.top()) };
        if (filled) {
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

    else {
        localRect(localBounds(), stroke, true, bodyFill); label(type_, 0, -5, 10); label(label_, 0, 10, 8, Palette::Muted);
    }

    if (selected) {
        const RectD bounds = worldBounds();
        painter.rect({ bounds.x - 4, bounds.y - 4, bounds.w + 8, bounds.h + 8 }, Palette::Selection, false);
    }
    drawPins(painter);
}

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
        registerComponent<Resistor>("Resistor");
        registerComponent<Capacitor>("Capacitor");
        registerComponent<Inductor>("Inductor");
        registerComponent<Potentiometer>("Potentiometer");
        registerComponent<Switch>("Switch");
        registerComponent<PushButton>("PushButton");
        registerComponent<LED>("LED");
        registerComponent<SevenSegment>("SevenSegment");
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
        {"Resistor", ComponentCategory::Passive, "Ohmic two-terminal resistor."},
        {"Capacitor", ComponentCategory::Passive, "Backward-Euler dynamic capacitor."},
        {"Inductor", ComponentCategory::Passive, "Backward-Euler dynamic inductor."},
        {"Potentiometer", ComponentCategory::Passive, "Three-terminal adjustable resistor."},
        {"Switch", ComponentCategory::Interactive, "Persistent open/closed interactive switch."},
        {"PushButton", ComponentCategory::Interactive, "Momentary switch: active only while held."},
        {"LED", ComponentCategory::Interactive, "Colored LED with threshold and visual state."},
        {"SevenSegment", ComponentCategory::Interactive, "Seven-segment display with A-G, DP and COM pins."},
    };
    return entries;
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


