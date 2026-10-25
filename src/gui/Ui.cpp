// Ui.cpp
#include "Ui.h"

#include <cmath>

namespace gui {

const std::vector<BoardTheme>& boardThemes() {
    static const std::vector<BoardTheme> themes = {
        // name        light               dark                last move (light / dark squares)         selected
        {"Green",  {235, 236, 208}, {115, 149, 82},  {245, 246, 130}, {185, 202, 67},  {255, 255, 80, 110}},
        {"Brown",  {240, 217, 181}, {181, 136, 99},  {205, 210, 106}, {170, 162, 58},  {20, 85, 30, 110}},
        {"Blue",   {222, 227, 230}, {140, 162, 173}, {195, 216, 135}, {151, 174, 106}, {20, 85, 160, 90}},
        {"Purple", {232, 225, 240}, {136, 118, 168}, {222, 214, 132}, {176, 160, 110}, {255, 255, 120, 90}},
        {"Graphite", {200, 200, 196}, {110, 112, 115}, {214, 212, 140}, {150, 150, 96}, {255, 255, 120, 90}},
    };
    return themes;
}

bool Fonts::load(const std::string& assetDir) {
    bool ok = regular.loadFromFile(assetDir + "/fonts/Inter-Regular.ttf");
    if (!semibold.loadFromFile(assetDir + "/fonts/Inter-SemiBold.ttf")) semibold = regular;
    if (!bold.loadFromFile(assetDir + "/fonts/Inter-Bold.ttf")) bold = semibold;
    return ok;
}

// ---------------------------------------------------------------------------
// RoundedRect
// ---------------------------------------------------------------------------
RoundedRect::RoundedRect(sf::Vector2f size, float radius, unsigned cornerPoints)
    : size_(size), radius_(radius), cornerPoints_(cornerPoints < 2 ? 2 : cornerPoints) {
    update();
}

void RoundedRect::setSize(sf::Vector2f size) {
    size_ = size;
    update();
}

void RoundedRect::setRadius(float radius) {
    radius_ = radius;
    update();
}

std::size_t RoundedRect::getPointCount() const { return cornerPoints_ * 4; }

sf::Vector2f RoundedRect::getPoint(std::size_t index) const {
    // Each corner is a quarter circle made of `cornerPoints_` points.
    const float r = std::min(radius_, std::min(size_.x, size_.y) / 2.f);
    const std::size_t corner = index / cornerPoints_;
    const float t = static_cast<float>(index % cornerPoints_) / static_cast<float>(cornerPoints_ - 1);
    const float pi = 3.14159265f;
    const float angle = (static_cast<float>(corner) + t) * pi / 2.f + pi;  // start at top-left
    sf::Vector2f centre;
    switch (corner) {
        case 0: centre = {r, r}; break;                       // top-left
        case 1: centre = {size_.x - r, r}; break;             // top-right
        case 2: centre = {size_.x - r, size_.y - r}; break;   // bottom-right
        default: centre = {r, size_.y - r}; break;            // bottom-left
    }
    return {centre.x + r * std::cos(angle), centre.y + r * std::sin(angle)};
}

void drawRoundedRect(sf::RenderTarget& target, sf::FloatRect rect, float radius, sf::Color fill, sf::Color outline,
                     float outlineThickness) {
    RoundedRect shape({rect.width, rect.height}, radius);
    shape.setPosition(rect.left, rect.top);
    shape.setFillColor(fill);
    if (outlineThickness > 0.f) {
        shape.setOutlineColor(outline);
        shape.setOutlineThickness(outlineThickness);
    }
    target.draw(shape);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
static const sf::Font& fontFor(const Fonts& fonts, Weight w) {
    switch (w) {
        case Weight::Bold:     return fonts.bold;
        case Weight::SemiBold: return fonts.semibold;
        default:               return fonts.regular;
    }
}

float textWidth(const Fonts& fonts, const std::string& str, unsigned size, Weight weight) {
    sf::Text t(sf::String::fromUtf8(str.begin(), str.end()), fontFor(fonts, weight), size);
    return t.getLocalBounds().width;
}

float drawText(sf::RenderTarget& target, const Fonts& fonts, const std::string& str, sf::Vector2f pos, unsigned size,
               sf::Color color, Weight weight, Align align) {
    sf::Text t(sf::String::fromUtf8(str.begin(), str.end()), fontFor(fonts, weight), size);
    t.setFillColor(color);
    const float w = t.getLocalBounds().width;
    float x = pos.x;
    if (align == Align::Center) x -= w / 2.f;
    if (align == Align::Right) x -= w;
    t.setPosition(std::round(x), std::round(pos.y));  // whole pixels = sharper text
    target.draw(t);
    return w;
}

void drawTextCentered(sf::RenderTarget& target, const Fonts& fonts, const std::string& str, sf::FloatRect rect,
                      unsigned size, sf::Color color, Weight weight) {
    sf::Text t(sf::String::fromUtf8(str.begin(), str.end()), fontFor(fonts, weight), size);
    t.setFillColor(color);
    // Centre on the font's cap height rather than the glyph bounds, so that
    // labels with and without descenders line up.
    const float w = t.getLocalBounds().width;
    const float capHeight = static_cast<float>(size) * 0.72f;
    const float top = static_cast<float>(size) * 0.25f;  // empty space above capitals in SFML's layout
    t.setPosition(std::round(rect.left + (rect.width - w) / 2.f),
                  std::round(rect.top + (rect.height - capHeight) / 2.f - top));
    target.draw(t);
}

// ---------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------
static void triangle(sf::RenderTarget& target, sf::Vector2f c, float s, bool pointLeft, sf::Color color) {
    sf::ConvexShape tri(3);
    float d = pointLeft ? -1.f : 1.f;
    tri.setPoint(0, {c.x + d * s * 0.45f, c.y});
    tri.setPoint(1, {c.x - d * s * 0.35f, c.y - s * 0.45f});
    tri.setPoint(2, {c.x - d * s * 0.35f, c.y + s * 0.45f});
    tri.setFillColor(color);
    target.draw(tri);
}

// A thick arc (part of a ring) from -50 to +50 degrees, opening to the right.
static void arc(sf::RenderTarget& target, sf::Vector2f c, float radius, float thickness, sf::Color color) {
    sf::VertexArray strip(sf::TriangleStrip);
    const float pi = 3.14159265f;
    for (int i = 0; i <= 16; ++i) {
        float a = (-50.f + 100.f * static_cast<float>(i) / 16.f) * pi / 180.f;
        strip.append(sf::Vertex({c.x + (radius - thickness / 2) * std::cos(a), c.y + (radius - thickness / 2) * std::sin(a)}, color));
        strip.append(sf::Vertex({c.x + (radius + thickness / 2) * std::cos(a), c.y + (radius + thickness / 2) * std::sin(a)}, color));
    }
    target.draw(strip);
}

static void bar(sf::RenderTarget& target, sf::Vector2f c, float s, sf::Color color) {
    sf::RectangleShape r({s * 0.14f, s * 0.9f});
    r.setPosition(c.x - s * 0.07f, c.y - s * 0.45f);
    r.setFillColor(color);
    target.draw(r);
}

void drawIcon(sf::RenderTarget& target, Icon icon, sf::Vector2f c, float s, sf::Color color) {
    switch (icon) {
        case Icon::Prev: triangle(target, c, s, true, color); break;
        case Icon::Next: triangle(target, c, s, false, color); break;
        case Icon::First:
            triangle(target, {c.x + s * 0.12f, c.y}, s, true, color);
            bar(target, {c.x - s * 0.42f, c.y}, s, color);
            break;
        case Icon::Last:
            triangle(target, {c.x - s * 0.12f, c.y}, s, false, color);
            bar(target, {c.x + s * 0.42f, c.y}, s, color);
            break;
        case Icon::Flip: {
            // two arrows: up and down
            sf::ConvexShape up(3), down(3);
            up.setPoint(0, {c.x - s * 0.25f, c.y - s * 0.5f});
            up.setPoint(1, {c.x - s * 0.5f, c.y - s * 0.1f});
            up.setPoint(2, {c.x, c.y - s * 0.1f});
            down.setPoint(0, {c.x + s * 0.25f, c.y + s * 0.5f});
            down.setPoint(1, {c.x, c.y + s * 0.1f});
            down.setPoint(2, {c.x + s * 0.5f, c.y + s * 0.1f});
            up.setFillColor(color);
            down.setFillColor(color);
            target.draw(up);
            target.draw(down);
            sf::RectangleShape a({s * 0.14f, s * 0.45f}), b({s * 0.14f, s * 0.45f});
            a.setPosition(c.x - s * 0.32f, c.y - s * 0.12f);
            b.setPosition(c.x + s * 0.18f, c.y - s * 0.33f);
            a.setFillColor(color);
            b.setFillColor(color);
            target.draw(a);
            target.draw(b);
            break;
        }
        case Icon::Sound:
        case Icon::Mute: {
            // speaker body
            sf::RectangleShape box({s * 0.22f, s * 0.36f});
            box.setPosition(c.x - s * 0.45f, c.y - s * 0.18f);
            box.setFillColor(color);
            target.draw(box);
            sf::ConvexShape cone(4);
            cone.setPoint(0, {c.x - s * 0.25f, c.y - s * 0.18f});
            cone.setPoint(1, {c.x + s * 0.02f, c.y - s * 0.45f});
            cone.setPoint(2, {c.x + s * 0.02f, c.y + s * 0.45f});
            cone.setPoint(3, {c.x - s * 0.25f, c.y + s * 0.18f});
            cone.setFillColor(color);
            target.draw(cone);
            if (icon == Icon::Sound) {
                arc(target, {c.x + s * 0.02f, c.y}, s * 0.22f, s * 0.08f, color);
                arc(target, {c.x + s * 0.02f, c.y}, s * 0.42f, s * 0.08f, color);
            } else {
                sf::RectangleShape l1({s * 0.5f, s * 0.09f}), l2({s * 0.5f, s * 0.09f});
                l1.setOrigin(s * 0.25f, s * 0.045f);
                l2.setOrigin(s * 0.25f, s * 0.045f);
                l1.setPosition(c.x + s * 0.3f, c.y);
                l2.setPosition(c.x + s * 0.3f, c.y);
                l1.setRotation(45.f);
                l2.setRotation(-45.f);
                l1.setFillColor(color);
                l2.setFillColor(color);
                target.draw(l1);
                target.draw(l2);
            }
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
void Button::draw(sf::RenderTarget& target, const Fonts& fonts, sf::Vector2f mouse) const {
    const bool hover = enabled && rect.contains(mouse);
    sf::Color fill = palette::Button;
    if (!enabled) fill = palette::ButtonDisabled;
    else if (primary) fill = hover ? palette::AccentHover : palette::Accent;
    else if (selected) fill = hover ? palette::AccentHover : palette::AccentDark;
    else if (hover) fill = palette::ButtonHover;

    // Subtle "3D" bottom edge like modern chess sites.
    sf::FloatRect shadowRect = rect;
    shadowRect.top += 3.f;
    drawRoundedRect(target, shadowRect, 7.f, primary ? sf::Color(69, 116, 36) : sf::Color(0, 0, 0, 70));
    drawRoundedRect(target, rect, 7.f, fill);

    const sf::Color textColor = enabled ? palette::Text : palette::TextFaint;
    if (icon != Icon::None && label.empty()) {
        drawIcon(target, icon, {rect.left + rect.width / 2.f, rect.top + rect.height / 2.f}, rect.height * 0.42f, textColor);
    } else {
        drawTextCentered(target, fonts, label, rect, fontSize, textColor, primary ? Weight::Bold : Weight::SemiBold);
    }
}

int hitTest(const std::vector<Button>& buttons, sf::Vector2f p) {
    for (const Button& b : buttons)
        if (b.contains(p)) return b.id;
    return -1;
}

}  // namespace gui
