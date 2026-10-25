// Renderer.h - draws the chess board: squares, coordinates, highlights,
// pieces, legal-move hints, arrows, animations and the promotion picker.
//
// The Renderer never changes the game. Everything it needs is passed in a
// BoardState, which the GameScreen fills every frame.
#pragma once

#include "Ui.h"
#include "engine/Board.h"

#include <SFML/Graphics.hpp>

#include <array>
#include <string>
#include <vector>

namespace gui {

// A piece sliding from one square to another (plus the rook when castling).
struct MoveAnimation {
    bool active = false;
    chess::Piece piece;
    chess::Square from = chess::NO_SQUARE, to = chess::NO_SQUARE;
    chess::Piece rook;
    chess::Square rookFrom = chess::NO_SQUARE, rookTo = chess::NO_SQUARE;
    float progress = 1.f;  // 0 = start, 1 = finished
};

struct Arrow {
    chess::Square from, to;
    sf::Color color;
};

struct BoardState {
    const chess::Board* board = nullptr;
    bool flipped = false;
    chess::Square lastFrom = chess::NO_SQUARE, lastTo = chess::NO_SQUARE;
    chess::Square selected = chess::NO_SQUARE;
    std::vector<chess::Square> targets;  // legal destinations of the selected piece
    bool showTargets = true;
    bool showCoordinates = true;
    bool dragging = false;
    sf::Vector2f dragPos;
    chess::Square hover = chess::NO_SQUARE;
    chess::Square checkSquare = chess::NO_SQUARE;  // king in check (red glow)
    MoveAnimation animation;
    std::vector<Arrow> arrows;
    const BoardTheme* theme = nullptr;
};

class Renderer {
public:
    bool loadPieces(const std::string& assetDir);

    void drawBoard(sf::RenderTarget& target, const Fonts& fonts, const BoardState& state) const;

    // Draws a piece image scaled to `size` x `size` at `topLeft`.
    void drawPiece(sf::RenderTarget& target, chess::Piece piece, sf::Vector2f topLeft, float size,
                   sf::Uint8 alpha = 255) const;

    // Promotion picker: a column of 4 pieces on the promotion square's file.
    void drawPromotionPicker(sf::RenderTarget& target, chess::Square promoSquare, chess::Color color, bool flipped,
                             sf::Vector2f mouse) const;
    // Piece chosen at `pos`, or PieceType::None if the click missed the picker.
    static chess::PieceType promotionChoiceAt(sf::Vector2f pos, chess::Square promoSquare, bool flipped);

    // Coordinate conversion between the virtual screen and board squares.
    static chess::Square squareAt(sf::Vector2f pos, bool flipped);
    static sf::Vector2f squareTopLeft(chess::Square sq, bool flipped);
    static sf::Vector2f squareCentre(chess::Square sq, bool flipped);

private:
    void drawArrow(sf::RenderTarget& target, const Arrow& arrow, bool flipped) const;
    static sf::FloatRect promotionRect(int index, chess::Square promoSquare, bool flipped);

    std::array<std::array<sf::Texture, 7>, 2> textures_;  // [colour][PieceType]
    std::array<std::array<bool, 7>, 2> loaded_{};
};

}  // namespace gui
