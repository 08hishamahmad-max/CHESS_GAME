// Renderer.cpp
#include "Renderer.h"

#include <cmath>

using namespace chess;

namespace gui {

namespace {

const char* const PIECE_LETTERS = " PNBRQK";  // index = PieceType
const PieceType PROMOTION_ORDER[4] = {PieceType::Queen, PieceType::Knight, PieceType::Rook, PieceType::Bishop};

void fillRect(sf::RenderTarget& target, sf::Vector2f pos, sf::Vector2f size, sf::Color color) {
    sf::RectangleShape r(size);
    r.setPosition(pos);
    r.setFillColor(color);
    target.draw(r);
}

// A circle that fades from `inner` in the centre to transparent at the edge.
void radialGlow(sf::RenderTarget& target, sf::Vector2f centre, float radius, sf::Color inner) {
    sf::VertexArray fan(sf::TriangleFan);
    fan.append(sf::Vertex(centre, inner));
    sf::Color outer = inner;
    outer.a = 0;
    for (int i = 0; i <= 40; ++i) {
        float a = static_cast<float>(i) / 40.f * 2.f * 3.14159265f;
        fan.append(sf::Vertex({centre.x + radius * std::cos(a), centre.y + radius * std::sin(a)}, outer));
    }
    target.draw(fan);
}

}  // namespace

bool Renderer::loadPieces(const std::string& assetDir) {
    bool all = true;
    for (int c = 0; c < 2; ++c) {
        for (int t = 1; t <= 6; ++t) {
            std::string file = assetDir + "/pieces/" + (c == 0 ? "w" : "b") + PIECE_LETTERS[t] + ".png";
            loaded_[c][t] = textures_[c][t].loadFromFile(file);
            if (loaded_[c][t]) {
                textures_[c][t].setSmooth(true);
                textures_[c][t].generateMipmap();  // crisp when scaled down
            }
            all = all && loaded_[c][t];
        }
    }
    return all;
}

// ---------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------
sf::Vector2f Renderer::squareTopLeft(Square sq, bool flipped) {
    int col = flipped ? 7 - fileOf(sq) : fileOf(sq);
    int row = flipped ? rankOf(sq) : 7 - rankOf(sq);
    return {layout::BOARD_X + col * layout::SQUARE, layout::BOARD_Y + row * layout::SQUARE};
}

sf::Vector2f Renderer::squareCentre(Square sq, bool flipped) {
    sf::Vector2f p = squareTopLeft(sq, flipped);
    return {p.x + layout::SQUARE / 2.f, p.y + layout::SQUARE / 2.f};
}

Square Renderer::squareAt(sf::Vector2f pos, bool flipped) {
    float x = pos.x - layout::BOARD_X, y = pos.y - layout::BOARD_Y;
    if (x < 0 || y < 0 || x >= layout::BOARD_SIZE || y >= layout::BOARD_SIZE) return NO_SQUARE;
    int col = static_cast<int>(x / layout::SQUARE), row = static_cast<int>(y / layout::SQUARE);
    return makeSquare(flipped ? 7 - col : col, flipped ? row : 7 - row);
}

// ---------------------------------------------------------------------------
// Pieces
// ---------------------------------------------------------------------------
void Renderer::drawPiece(sf::RenderTarget& target, Piece piece, sf::Vector2f topLeft, float size, sf::Uint8 alpha) const {
    if (piece.isEmpty()) return;
    const int c = static_cast<int>(piece.color), t = static_cast<int>(piece.type);
    if (loaded_[c][t]) {
        sf::Sprite sprite(textures_[c][t]);
        float scale = size / static_cast<float>(textures_[c][t].getSize().x);
        sprite.setScale(scale, scale);
        sprite.setPosition(topLeft);
        sprite.setColor(sf::Color(255, 255, 255, alpha));
        target.draw(sprite);
        return;
    }
    // Fallback when the image files are missing: a simple disc.
    sf::CircleShape disc(size * 0.36f);
    disc.setPosition(topLeft.x + size * 0.14f, topLeft.y + size * 0.14f);
    disc.setFillColor(piece.color == Color::White ? sf::Color(250, 250, 245, alpha) : sf::Color(30, 30, 34, alpha));
    disc.setOutlineThickness(2.f);
    disc.setOutlineColor(sf::Color(100, 100, 100, alpha));
    target.draw(disc);
}

// ---------------------------------------------------------------------------
// Board
// ---------------------------------------------------------------------------
void Renderer::drawBoard(sf::RenderTarget& target, const Fonts& fonts, const BoardState& s) const {
    const Board& board = *s.board;
    const BoardTheme& theme = s.theme ? *s.theme : boardThemes().front();
    const float S = layout::SQUARE;

    // Soft shadow under the board.
    drawRoundedRect(target, {layout::BOARD_X - 2.f, layout::BOARD_Y + 2.f, layout::BOARD_SIZE + 4.f, layout::BOARD_SIZE + 6.f},
                    6.f, sf::Color(0, 0, 0, 80));

    // 1. Squares, with last-move and selection tints.
    for (Square sq = 0; sq < 64; ++sq) {
        const bool dark = (fileOf(sq) + rankOf(sq)) % 2 == 0;  // a1 is dark
        sf::Color color = dark ? theme.dark : theme.light;
        if (sq == s.lastFrom || sq == s.lastTo) color = dark ? theme.lastMoveDark : theme.lastMoveLight;
        fillRect(target, squareTopLeft(sq, s.flipped), {S, S}, color);
        if (sq == s.selected) fillRect(target, squareTopLeft(sq, s.flipped), {S, S}, theme.selected);
    }

    // 2. Red glow behind a king in check.
    if (s.checkSquare != NO_SQUARE)
        radialGlow(target, squareCentre(s.checkSquare, s.flipped), S * 0.62f, sf::Color(255, 30, 30, 235));

    // 3. Coordinates drawn inside the edge squares (like lichess / chess.com).
    if (s.showCoordinates) {
        for (int i = 0; i < 8; ++i) {
            const int file = s.flipped ? 7 - i : i;
            const int rank = s.flipped ? i : 7 - i;
            // file letters along the bottom row
            Square bottom = makeSquare(file, s.flipped ? 7 : 0);
            bool darkB = (fileOf(bottom) + rankOf(bottom)) % 2 == 0;
            sf::Vector2f pb = squareTopLeft(bottom, s.flipped);
            drawText(target, fonts, std::string(1, char('a' + file)), {pb.x + S - 5.f, pb.y + S - 21.f}, 14,
                     darkB ? theme.light : theme.dark, Weight::SemiBold, Align::Right);
            // rank numbers along the left column
            Square left = makeSquare(s.flipped ? 7 : 0, rank);
            bool darkL = (fileOf(left) + rankOf(left)) % 2 == 0;
            sf::Vector2f pl = squareTopLeft(left, s.flipped);
            drawText(target, fonts, std::string(1, char('1' + rank)), {pl.x + 5.f, pl.y + 3.f}, 14,
                     darkL ? theme.light : theme.dark, Weight::SemiBold);
        }
    }

    // 4. Pieces (skipping the dragged piece and the animated ones).
    const MoveAnimation& anim = s.animation;
    for (Square sq = 0; sq < 64; ++sq) {
        if (s.dragging && sq == s.selected) continue;
        if (anim.active && (sq == anim.to || sq == anim.rookTo)) continue;
        drawPiece(target, board.at(sq), squareTopLeft(sq, s.flipped), S);
    }

    // 5. Legal destinations: small dot on empty squares, ring on captures.
    if (s.showTargets) {
        for (Square sq : s.targets) {
            sf::Vector2f c = squareCentre(sq, s.flipped);
            if (board.at(sq).isEmpty()) {
                sf::CircleShape dot(S * 0.165f, 32);
                dot.setOrigin(dot.getRadius(), dot.getRadius());
                dot.setPosition(c);
                dot.setFillColor(sf::Color(0, 0, 0, 36));
                target.draw(dot);
            } else {
                float r = S * 0.46f, thickness = S * 0.085f;
                sf::CircleShape ring(r - thickness, 48);
                ring.setOrigin(ring.getRadius(), ring.getRadius());
                ring.setPosition(c);
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineThickness(thickness);
                ring.setOutlineColor(sf::Color(0, 0, 0, 36));
                target.draw(ring);
            }
        }
    }

    // 6. Square under the dragged piece.
    if (s.dragging && s.hover != NO_SQUARE) {
        sf::Vector2f p = squareTopLeft(s.hover, s.flipped);
        sf::RectangleShape outline({S - 6.f, S - 6.f});
        outline.setPosition(p.x + 3.f, p.y + 3.f);
        outline.setFillColor(sf::Color(255, 255, 255, 30));
        outline.setOutlineThickness(3.f);
        outline.setOutlineColor(sf::Color(255, 255, 255, 170));
        target.draw(outline);
    }

    // 7. Arrows (engine hint).
    for (const Arrow& a : s.arrows) drawArrow(target, a, s.flipped);

    // 8. Animated pieces, eased so they slow down at the end.
    if (anim.active) {
        float t = anim.progress;
        float eased = 1.f - (1.f - t) * (1.f - t) * (1.f - t);  // ease-out cubic
        auto lerp = [&](Square from, Square to) {
            sf::Vector2f a = squareTopLeft(from, s.flipped), b = squareTopLeft(to, s.flipped);
            return sf::Vector2f(a.x + (b.x - a.x) * eased, a.y + (b.y - a.y) * eased);
        };
        if (anim.rookTo != NO_SQUARE) drawPiece(target, anim.rook, lerp(anim.rookFrom, anim.rookTo), S);
        drawPiece(target, anim.piece, lerp(anim.from, anim.to), S);
    }

    // 9. Dragged piece: a bit larger, with a shadow, centred on the mouse.
    if (s.dragging && s.selected != NO_SQUARE) {
        const float big = S * 1.12f;
        sf::Vector2f tl(s.dragPos.x - big / 2.f, s.dragPos.y - big / 2.f);
        const Piece p = board.at(s.selected);
        const int c = static_cast<int>(p.color), t = static_cast<int>(p.type);
        if (loaded_[c][t]) {  // shadow = the same image in translucent black
            sf::Sprite shadow(textures_[c][t]);
            float scale = big / static_cast<float>(textures_[c][t].getSize().x);
            shadow.setScale(scale, scale);
            shadow.setPosition(tl.x + 5.f, tl.y + 8.f);
            shadow.setColor(sf::Color(0, 0, 0, 70));
            target.draw(shadow);
        }
        drawPiece(target, p, tl, big);
    }
}

void Renderer::drawArrow(sf::RenderTarget& target, const Arrow& arrow, bool flipped) const {
    sf::Vector2f a = squareCentre(arrow.from, flipped), b = squareCentre(arrow.to, flipped);
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1.f) return;
    sf::Vector2f u = d / len, n(-u.y, u.x);
    const float S = layout::SQUARE;
    const float width = S * 0.2f, headLen = S * 0.45f, headWidth = S * 0.5f;
    sf::Vector2f start = a + u * (S * 0.2f);
    sf::Vector2f headBase = b - u * headLen;

    sf::ConvexShape shaft(4);
    shaft.setPoint(0, start + n * (width / 2));
    shaft.setPoint(1, headBase + n * (width / 2));
    shaft.setPoint(2, headBase - n * (width / 2));
    shaft.setPoint(3, start - n * (width / 2));
    shaft.setFillColor(arrow.color);
    target.draw(shaft);

    sf::ConvexShape head(3);
    head.setPoint(0, b - u * (S * 0.05f));
    head.setPoint(1, headBase + n * (headWidth / 2));
    head.setPoint(2, headBase - n * (headWidth / 2));
    head.setFillColor(arrow.color);
    target.draw(head);
}

// ---------------------------------------------------------------------------
// Promotion picker
// ---------------------------------------------------------------------------
sf::FloatRect Renderer::promotionRect(int index, Square promoSquare, bool flipped) {
    // The picker starts on the promotion square and extends towards the centre.
    sf::Vector2f p = squareTopLeft(promoSquare, flipped);
    bool atTop = p.y < layout::BOARD_Y + layout::SQUARE / 2.f;
    float y = atTop ? p.y + index * layout::SQUARE : p.y - index * layout::SQUARE;
    return {p.x, y, layout::SQUARE, layout::SQUARE};
}

void Renderer::drawPromotionPicker(sf::RenderTarget& target, Square promoSquare, Color color, bool flipped,
                                   sf::Vector2f mouse) const {
    fillRect(target, {layout::BOARD_X, layout::BOARD_Y}, {layout::BOARD_SIZE, layout::BOARD_SIZE}, sf::Color(0, 0, 0, 140));
    for (int i = 0; i < 4; ++i) {
        sf::FloatRect r = promotionRect(i, promoSquare, flipped);
        bool hover = r.contains(mouse);
        sf::CircleShape bg(r.width / 2.f, 48);
        bg.setPosition(r.left, r.top);
        bg.setFillColor(hover ? sf::Color(232, 140, 60) : sf::Color(178, 178, 178));
        if (hover) {
            fillRect(target, {r.left, r.top}, {r.width, r.height}, sf::Color(232, 140, 60));
        } else {
            fillRect(target, {r.left, r.top}, {r.width, r.height}, sf::Color(240, 240, 240));
            target.draw(bg);
        }
        float shrink = hover ? 0.f : r.width * 0.08f;
        drawPiece(target, Piece(PROMOTION_ORDER[i], color), {r.left + shrink, r.top + shrink}, r.width - 2 * shrink);
    }
}

PieceType Renderer::promotionChoiceAt(sf::Vector2f pos, Square promoSquare, bool flipped) {
    for (int i = 0; i < 4; ++i)
        if (promotionRect(i, promoSquare, flipped).contains(pos)) return PROMOTION_ORDER[i];
    return PieceType::None;
}

}  // namespace gui
