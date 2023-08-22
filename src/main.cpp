#include "loop.hpp"
#include "math.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

enum class State {
    None,
    White,
    Black,
};

enum class Mode {
    White,
    Black,
};

struct Rect {
    f32 x0;
    f32 y0;
    f32 x1;
    f32 y1;

    [[nodiscard]] auto contains(f32 x, f32 y) const -> bool {
        return x0 <= x && x < x1 && y0 <= y && y < y1;
    }
};

struct GameState {
    AssetManager                asset_manager   = {};
    Handle<Texture>             board_texture   = {};
    Handle<Texture>             black_texture   = {};
    Handle<Texture>             white_texture   = {};
    Handle<Texture>             select_texture  = {};
    Option<i32vec2>             cell            = {};
    Mode                        mode            = {};
    Mode                        next            = {};
    std::array<State, 8 * 8>    board           = {};
};

static auto id(i32 x, i32 y) -> i32 {
    return x + y * 8;
}

static auto get_available_cells(std::array<State, 8 * 8>& board, i32vec2 where) -> std::set<i32vec2> {
    std::set<i32vec2> cells = {};

    auto offsets = std::array{
            i32vec2(-1, 0),
            i32vec2(+1, 0),
            i32vec2(0, +1),
            i32vec2(0, -1),
    };

    auto is_available = [&board](i32vec2 const& cell) -> bool {
        if (cell.x < 0 || cell.x >= 8) {
            return false;
        }
        if (cell.y < 0 || cell.y >= 8) {
            return false;
        }
        return board[id(cell.x, cell.y)] == State::None;
    };

    for (auto& offset : offsets) {
        if (is_available(where + offset)) {
            cells.emplace(where + offset);
        }
    }

    std::set<i32vec2> view = {};
    std::deque<i32vec2> queue = {};
    queue.emplace_back(where);
    while (!queue.empty()) {
        auto node = queue.front();
        queue.pop_front();

        if (view.contains(node)) {
            continue;
        }

        for (auto& offset : offsets) {
            if (!is_available(node + offset) && is_available(node + offset * 2)) {
                cells.emplace(node + offset * 2);
                queue.emplace_back(node + offset * 2);
            }
        }

        view.emplace(node);
    }
    return cells;
}

static auto draw_sprite(Renderer& renderer, GpuTexture const& texture, f32 x, f32 y, f32 w, f32 h) {
    auto rect = SDL_Rect(i32(x), i32(y), i32(w), i32(h));
    SDL_RenderCopy(renderer.native_handle(), texture.native_handle, nullptr, &rect);
}

static void init_board(GameState& gs) {
    gs.board[id(0, 0)] = State::Black;
    gs.board[id(0, 1)] = State::Black;
    gs.board[id(0, 2)] = State::Black;
    gs.board[id(1, 0)] = State::Black;
    gs.board[id(1, 1)] = State::Black;
    gs.board[id(1, 2)] = State::Black;
    gs.board[id(2, 0)] = State::Black;
    gs.board[id(2, 1)] = State::Black;
    gs.board[id(2, 2)] = State::Black;

    gs.board[id(5, 5)] = State::White;
    gs.board[id(5, 6)] = State::White;
    gs.board[id(5, 7)] = State::White;
    gs.board[id(6, 5)] = State::White;
    gs.board[id(6, 6)] = State::White;
    gs.board[id(6, 7)] = State::White;
    gs.board[id(7, 5)] = State::White;
    gs.board[id(7, 6)] = State::White;
    gs.board[id(7, 7)] = State::White;
}

auto main(i32 argc, const char* argv[]) -> i32 {
    SDL_Init(SDL_INIT_VIDEO);

    auto event_loop = EventLoop::new_();
    auto window = WindowBuilder::new_()
        .set_title("Corners")
        .set_size(LogicalSize(450, 450))
        .create();
    auto renderer = Renderer::new_(window);

    auto gs = GameState();

    gs.mode = Mode::White;
    gs.next = Mode::Black;

    gs.asset_manager = AssetManager::new_();
    gs.board_texture = gs.asset_manager.textures.add(Texture("assets/board.png"), renderer);
    gs.black_texture = gs.asset_manager.textures.add(Texture("assets/black.png"), renderer);
    gs.white_texture = gs.asset_manager.textures.add(Texture("assets/white.png"), renderer);
    gs.select_texture = gs.asset_manager.textures.add(Texture("assets/select.png"), renderer);

    init_board(gs);

    event_loop.run([
        mouse_pressed = false,
        gs = std::move(gs),
        window = std::move(window),
        renderer = std::move(renderer)
    ](auto const& event, auto& control_flow) mutable {
//        static bool mouse_pressed = false;
        match_(event) {
            case_(Event::Quit const&) {
                control_flow.request_exit();
            },
            case_(Event::MouseButtonUp const&) {},
            case_(Event::MouseButtonDown const&) {
                mouse_pressed = true;
            },
            case_(Event::RequestRedraw const&) {
                static constexpr auto cell_size = 450.0F / 8.0F;

                SDL_SetRenderDrawColor(renderer.native_handle(), 0xFF, 0xFF, 0xFF, 0xFF);
                SDL_RenderClear(renderer.native_handle());

                i32 mouse_x;
                i32 mouse_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);

                draw_sprite(renderer, gs.asset_manager.textures.get(gs.board_texture), 0, 0, 450.0F, 450.0F);

                for (i32 x = 0; x < 8; ++x) {
                    for (i32 y = 0; y < 8; ++y) {
                        auto px = cell_size * f32(x);
                        auto py = cell_size * f32(y);
                        auto rect = Rect(px, py, px + cell_size, py + cell_size);
                        auto press = mouse_pressed && rect.contains(f32(mouse_x), f32(mouse_y));

                        switch (gs.board[id(x, y)]) {
                            case State::None: {
                                if (gs.cell && press) {
                                    auto cells = gs.cell.map([&](auto& it) {
                                        return get_available_cells(gs.board, it);
                                    });

                                    if (cells->contains(i32vec2(x, y))) {
                                        std::swap(gs.board[id(x, y)], gs.board[id(gs.cell->x, gs.cell->y)]);
                                        gs.cell = None;
                                        gs.mode = gs.next;
                                    }
                                }
                                break;
                            }
                            case State::Black: {
                                if (press && (gs.mode == Mode::Black)) {
                                    gs.cell = i32vec2(x, y);
                                    gs.next = Mode::White;
                                }
                                if (gs.cell && (*gs.cell == i32vec2(x, y))) {
                                    draw_sprite(renderer, gs.asset_manager.textures.get(gs.select_texture), px, py, cell_size, cell_size);
                                }
                                draw_sprite(renderer, gs.asset_manager.textures.get(gs.black_texture), px, py, cell_size, cell_size);
                                break;
                            }
                            case State::White: {
                                if (press && (gs.mode == Mode::White)) {
                                    gs.cell = i32vec2(x, y);
                                    gs.next = Mode::Black;
                                }
                                if (gs.cell && (*gs.cell == i32vec2(x, y))) {
                                    draw_sprite(renderer, gs.asset_manager.textures.get(gs.select_texture), px, py, cell_size, cell_size);
                                }
                                draw_sprite(renderer, gs.asset_manager.textures.get(gs.white_texture), px, py, cell_size, cell_size);
                                break;
                            }
                        }
                    }
                }

                SDL_RenderPresent(renderer.native_handle());

                mouse_pressed = false;
            },
            case_(auto&) {}
        };
    });
    return 0;
}