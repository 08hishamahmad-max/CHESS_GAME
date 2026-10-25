// Ui.h - small UI toolkit shared by all screens: colours, fonts, layout
// constants, rounded rectangles, text helpers and a Button widget.
#pragma once

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

namespace gui {

// ---------------------------------------------------------------------------
// Layout of the (virtual) window. Everything is drawn in these coordinates;
// App scales the picture to the real window size (letter-boxing if needed).
// ---------------------------------------------------------------------------
namespace layout {
constexpr float WIDTH = 1220.f;
constexpr float HEIGHT = 780.f;

constexpr float SQUARE = 84.f;
constexpr float BOARD_X = 36.f;
constexpr float BOARD_Y = 56.f;
constexpr float BOARD_SIZE = SQUARE * 8.f;  // 672

constexpr float PANEL_X = BOARD_X + BOARD_SIZE + 30.f;  // right-hand panel
constexpr float PANEL_Y = 12.f;
constexpr float PANEL_W = WIDTH - PANEL_X - 18.f;
constexpr float PANEL_H = HEIGHT - 2 * PANEL_Y;
}  // namespace layout

// ---------------------------------------------------------------------------
// Colours of the dark interface.
// ---------------------------------------------------------------------------
namespace palette {
const sf::Color Background(36, 34, 31);
const sf::Color Panel(48, 46, 43);
const sf::Color PanelDark(39, 37, 34);
const sf::Color PanelLight(62, 59, 55);
const sf::Color Button(66, 63, 59);
const sf::Color ButtonHover(84, 80, 75);
const sf::Color ButtonDisabled(52, 50, 47);
const sf::Color Accent(129, 182, 76);       // green, like chess.com
const sf::Color AccentHover(149, 199, 96);
const sf::Color AccentDark(96, 145, 54);
const sf::Color Danger(201, 72, 64);
const sf::Color Text(240, 238, 234);
const sf::Color TextDim(160, 156, 150);
const sf::Color TextFaint(115, 111, 106);
const sf::Color Shadow(0, 0, 0, 90);
}  // namespace palette

// ---------------------------------------------------------------------------
// Board colour themes.
// ---------------------------------------------------------------------------
struct BoardTheme {
    const char* name;
    sf::Color light, dark;
    sf::Color lastMoveLight, lastMoveDark;
    sf::Color selected;
};
const std::vector<BoardTheme>& boardThemes();

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------
struct Fonts {
    sf::Font regular, semibold, bold;
    bool load(const std::string& assetDir);
};
enum class Weight { Regular, SemiBold, Bold };
enum class Align { Left, Center, Right };

// ---------------------------------------------------------------------------
// Shapes and drawing helpers
// ---------------------------------------------------------------------------
// A rectangle with rounded corners (SFML only has sharp ones).
class RoundedRect : public sf::Shape {
public:
    RoundedRect(sf::Vector2f size = {0, 0}, float radius = 0.f, unsigned cornerPoints = 8);
    void setSize(sf::Vector2f size);
    void setRadius(float radius);
    std::size_t getPointCount() const override;
    sf::Vector2f getPoint(std::size_t index) const override;

private:
    sf::Vector2f size_;
    float radius_;
    unsigned cornerPoints_;
};

void drawRoundedRect(sf::RenderTarget& target, sf::FloatRect rect, float radius, sf::Color fill,
                     sf::Color outline = sf::Color::Transparent, float outlineThickness = 0.f);

// Draws text; `pos` is the top-left, top-centre or top-right depending on `align`.
// Returns the width of the text.
float drawText(sf::RenderTarget& target, const Fonts& fonts, const std::string& str, sf::Vector2f pos,
               unsigned size, sf::Color color, Weight weight = Weight::Regular, Align align = Align::Left);

// Draws text centred (both ways) inside a rectangle.
void drawTextCentered(sf::RenderTarget& target, const Fonts& fonts, const std::string& str, sf::FloatRect rect,
                      unsigned size, sf::Color color, Weight weight = Weight::SemiBold);

float textWidth(const Fonts& fonts, const std::string& str, unsigned size, Weight weight = Weight::Regular);

// Simple vector icons used on buttons (drawn with shapes, no image files).
enum class Icon { None, First, Prev, Next, Last, Flip, Sound, Mute };
void drawIcon(sf::RenderTarget& target, Icon icon, sf::Vector2f centre, float size, sf::Color color);

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
struct Button {
    int id = 0;
    sf::FloatRect rect;
    std::string label;
    Icon icon = Icon::None;
    bool enabled = true;
    bool primary = false;   // green call-to-action style
    bool selected = false;  // toggled on (e.g. chosen option in the menu)
    unsigned fontSize = 15;

    bool contains(sf::Vector2f p) const { return enabled && rect.contains(p); }
    void draw(sf::RenderTarget& target, const Fonts& fonts, sf::Vector2f mouse) const;
};

// Returns the id of the button under `p`, or -1.
int hitTest(const std::vector<Button>& buttons, sf::Vector2f p);

}  // namespace gui
