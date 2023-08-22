#include "loop.hpp"
#include "math.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <set>
#include <deque>

struct Texture {
    std::string path;
};

struct GpuTexture {
    u32 width;
    u32 height;
    u32 native_handle;
};

template<typename T>
struct AssetLoader;

template<>
struct AssetLoader<Texture> {
    using Result = GpuTexture;

    struct Drop {
        static auto operator()(u8* ptr) {
            stbi_image_free(ptr);
        }
    };

    using Image = std::unique_ptr<u8[], Drop>;

    static auto open(Texture const& texture) -> Option<Result> {
        i32 width;
        i32 height;
        stbi_set_flip_vertically_on_load_thread(true);
        auto image = Image(stbi_load(texture.path.c_str(), &width, &height, nullptr, 4));
        if (image == nullptr) {
            return None;
        }

        u32 native_handle = {};
        glGenTextures(1, &native_handle);
        glBindTexture(GL_TEXTURE_2D, native_handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.get());
        glBindTexture(GL_TEXTURE_2D, 0);
        return GpuTexture(width, height, native_handle);
    }
};

template<std::invocable Fn>
struct Lazy : Fn {
    explicit operator auto() {
        return Fn::operator()();
    }
};

template<std::invocable Fn>
Lazy(Fn) -> Lazy<Fn>;

template<typename T>
struct Handle {
    u64 resource;

    friend auto operator<=>(Handle const&, Handle const&) noexcept = default;
};

template<typename T>
struct std::hash<Handle<T>> {
    static auto operator()(Handle<T> const& self) -> size_t {
        return std::hash<u64>{}(self.resource);
    }
};

template<typename T>
struct Assets {
    using Loader = AssetLoader<T>;
    using Resource = Loader::Result;

    auto add(T resource) -> Handle<T> {
        auto handle = Handle<T>(next_resource++);
        resources.emplace(handle, Loader::open(resource).unwrap());
        return handle;
    }

    auto get(Handle<T> const& handle) & -> Resource& {
        return resources.at(handle);
    }

private:
    u64 next_resource = 0;
    std::unordered_map<Handle<T>, Resource> resources;
};

auto create_shader(u32 type, std::string const& src) -> Option<u32> {
    auto sources = std::array{
        src.c_str()
    };

    auto lengths = std::array{
        i32(src.size())
    };

    u32 handle = glCreateShader(type);
    glShaderSource(handle, 1, &sources[0], &lengths[0]);
    glCompileShader(handle);

    i32 length = {};
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        auto msg = std::string(size_t(length), '\0');
        glGetShaderInfoLog(handle, length, nullptr, &msg[0]);
        fmt::println(stdout, "{}", msg);
    }

    i32 status = {};
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        glDeleteShader(handle);
        return None;
    }
    return Some(auto(handle));
}

struct Primitive_Arrays {
    i32 first;
    i32 count;
};
struct Primitive_Elements {
    i32 count;
    i32 type;
};
struct Primitive : Enum<Primitive_Arrays, Primitive_Elements> {
    using Enum::Enum;

    using Arrays = Primitive_Arrays;
    using Elements = Primitive_Elements;
};

struct Indices_U32 {
    std::vector<u32> elements;
};

struct Indices : Enum<Indices_U32> {
    using Enum::Enum;
    using U32 = Indices_U32;
};

struct DefaultVertex {
    f32 position[3];
    f32 texcoord[2];
};

struct Mesh {
    i32                         topology    = {};
    std::vector<DefaultVertex>  vertices    = {};
    Option<Indices>             indices     = {};
};

struct GpuMesh {
    i32 topology        = {};
    u32 vertex_array    = {};
    u32 vertex_buffer   = {};
    u32 element_buffer  = {};
    Primitive primitive = {};
};

template<>
struct AssetLoader<Mesh> {
    using Result = GpuMesh;

    static auto open(Mesh const& mesh) -> Option<Result> {
        u32 vertex_array = {};
        glGenVertexArrays(1, &vertex_array);
        glBindVertexArray(vertex_array);

        u32 vertex_buffer = {};
        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultVertex) * mesh.vertices.size(), mesh.vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultVertex), (void*)offsetof(DefaultVertex, position));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultVertex), (void*)offsetof(DefaultVertex, texcoord));
        glEnableVertexAttribArray(1);

        u32 element_buffer = {};
        auto primitive = mesh.indices.map_or(
            Primitive(Primitive::Arrays(0, i32(mesh.vertices.size()))),
            [&](auto const& self) {
                return match_(self) {
                    case_(Indices::U32 const& indices) {
                        auto count = i32(indices.elements.size());
                        auto size = i32(sizeof(u32) * indices.elements.size());
                        glGenBuffers(1, &element_buffer);
                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
                        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices.elements.data(), GL_STATIC_DRAW);
                        return Primitive(Primitive::Elements(count, GL_UNSIGNED_INT));
                    },
                    case_(auto const&) { std::abort(); }
                };
            }
        );
        glBindVertexArray(0);
        return GpuMesh(mesh.topology, vertex_array, vertex_buffer, element_buffer, primitive);
    }
};

struct AssetManager {
    Assets<Mesh> meshes;
    Assets<Texture> textures;

    static auto new_() -> AssetManager {
        return {};
    }
};

auto main(i32 argc, const char* argv[]) -> i32 {
    auto event_loop = EventLoop::new_();
    auto window = WindowBuilder::new_()
        .set_title("Demo")
        .set_size(LogicalSize(450, 450))
        .create();

    auto renderer = Renderer::new_(window);

    SDL_GL_MakeCurrent(window.native_handle(), renderer.native_handle());
    glewExperimental = GL_TRUE;
    glewInit();

    auto asset_manager = AssetManager::new_();
    auto board_texture = asset_manager.textures.add(Texture("assets/board.png"));
    auto black_texture = asset_manager.textures.add(Texture("assets/black.png"));
    auto white_texture = asset_manager.textures.add(Texture("assets/white.png"));
    auto select_texture = asset_manager.textures.add(Texture("assets/select.png"));

    auto vs = create_shader(GL_VERTEX_SHADER, R"(
        #version 410

        uniform mat4x4 u_ClipMatrix;
        uniform mat4x4 u_SpriteMatrix;

        layout(location = 0) in vec3 in_VertexPosition;
        layout(location = 1) in vec2 in_VertexTexCoord;

        layout(location = 1) out vec2 fs_VertexTexCoord;

        void main() {
            gl_Position = u_ClipMatrix * u_SpriteMatrix * vec4(in_VertexPosition, 1);

            fs_VertexTexCoord = in_VertexTexCoord;
        }
    )").unwrap();
    auto fs = create_shader(GL_FRAGMENT_SHADER, R"(
        #version 410

        uniform sampler2D u_MainTexture;

        layout(location = 0) in vec2 fs_VertexTexCoord;

        layout(location = 0) out vec4 out_FragColor;

        void main() {
            out_FragColor = texture(u_MainTexture, fs_VertexTexCoord);
        }
    )").unwrap();

    auto program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    i32 status = {};
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    i32 length = {};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        auto msg = std::string(size_t(length), '\0');
        glGetProgramInfoLog(program, length, nullptr, &msg[0]);
        fmt::println(stdout, "{}", msg);
    }

    glDetachShader(program, vs);
    glDetachShader(program, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    auto quad = asset_manager.meshes.add(Mesh{
        .topology = GL_TRIANGLES,
        .vertices = std::vector<DefaultVertex>{
            {-0.5F, -0.5F, +0.0f, 0.0F, 0.0F},
            {-0.5F, +0.5F, +0.0f, 0.0F, 1.0F},
            {+0.5F, +0.5F, +0.0f, 1.0F, 1.0F},
            {+0.5F, -0.5F, +0.0f, 1.0F, 0.0F},
        },
        .indices = Indices::U32({
            0, 1, 2,
            0, 2, 3
        })
    });

    auto draw_sprite = [&](Handle<Texture> texture, f32mat4x4 const& m, f32 x, f32 y, f32 w, f32 h) {
        x = x + w * 0.5F;
        y = y + h * 0.5F;

        auto sprite_matrix = f32mat4x4(
            w, 0, 0, 0,
            0, h, 0, 0,
            0, 0, 1, 0,
            x, y, 0, 1
        );
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_ClipMatrix"), 1, GL_FALSE, &m.m00);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_SpriteMatrix"), 1, GL_FALSE, &sprite_matrix.m00);

        auto& gpu_texture = asset_manager.textures.get(texture);
        glBindTexture(GL_TEXTURE_2D, gpu_texture.native_handle);
        glActiveTexture(GL_TEXTURE0 + 0);
        glUniform1i(glGetUniformLocation(program, "u_MainTexture"), 0);

        auto& gpu_mesh = asset_manager.meshes.get(quad);
        glBindVertexArray(gpu_mesh.vertex_array);
        match_(gpu_mesh.primitive) {
            case_(Primitive::Arrays const& impl) {
                glDrawArrays(gpu_mesh.topology, impl.first, impl.count);
            },
            case_(Primitive::Elements const& impl) {
                glDrawElements(gpu_mesh.topology, impl.count, impl.type, nullptr);
            }
        };
        glBindVertexArray(0);
        glUseProgram(0);
    };

    enum class State {
        None,
        White,
        Black,
    };

    enum class Mode {
        White,
        Black,
    };

    static constexpr auto id = [](i32 x, i32 y) -> int {
        return x + y * 8;
    };

    std::array<State, 8 * 8> board = {};
    board[id(0, 5)] = State::Black;
    board[id(0, 6)] = State::Black;
    board[id(0, 7)] = State::Black;
    board[id(1, 5)] = State::Black;
    board[id(1, 6)] = State::Black;
    board[id(1, 7)] = State::Black;
    board[id(2, 5)] = State::Black;
    board[id(2, 6)] = State::Black;
    board[id(2, 7)] = State::Black;

    board[id(5, 0)] = State::White;
    board[id(5, 1)] = State::White;
    board[id(5, 2)] = State::White;
    board[id(6, 0)] = State::White;
    board[id(6, 1)] = State::White;
    board[id(6, 2)] = State::White;
    board[id(7, 0)] = State::White;
    board[id(7, 1)] = State::White;
    board[id(7, 2)] = State::White;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Option<i32vec2> selected = None;

    auto mode = Mode::White;
    auto next = Mode::Black;

    std::atomic_bool request_exit = {};
    while (!request_exit.load()) {
        bool mouse_pressed = false;
        while (auto event = event_loop.poll_event()) {
            match_(*event) {
                case_(Event::Quit&) {
                    request_exit.store(true);
                },
                case_(Event::MouseButtonUp& button) {},
                case_(Event::MouseButtonDown& button) {
                    mouse_pressed = true;
                },
                case_(auto&) {}
            };
        }

        auto physical_size = window.get_physical_size();
        auto clip_matrix = orthographic_from_size(
            f32(physical_size.width),
            f32(physical_size.height)
        );
        clip_matrix.m30 = -1.0F;
        clip_matrix.m31 = -1.0F;

        glViewport(0, 0, i32(physical_size.width), i32(physical_size.height));
        glClearColor(1.0F, 1.0F, 1.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);

        i32 mouse_x;
        i32 mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        mouse_y = 450 - mouse_y;

        struct Rect {
            f32 x0;
            f32 y0;
            f32 x1;
            f32 y1;

            [[nodiscard]] auto contains(f32 x, f32 y) const -> bool {
                return x0 <= x && x < x1 && y0 <= y && y < y1;
            }
        };

        auto cell_size = 450.0F / 8.0F;
        auto get_available_cells = [](std::array<State, 8 * 8>& board, i32vec2 where) -> std::set<i32vec2> {
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
        };

        draw_sprite(board_texture, clip_matrix, 0, 0, 450.0F, 450.0F);

        for (i32 x = 0; x < 8; ++x) {
            for (i32 y = 0; y < 8; ++y) {
                auto px = cell_size * f32(x);
                auto py = cell_size * f32(y);
                auto rect = Rect(px, py, px + cell_size, py + cell_size);
                auto press = mouse_pressed && rect.contains(f32(mouse_x), f32(mouse_y));

                switch (board[id(x, y)]) {
                    case State::None: {
                        if (selected && press) {
                            auto cells = selected.map([&](auto& it) {
                                return get_available_cells(board, it);
                            });

                            if (cells->contains(i32vec2(x, y))) {
                                std::swap(board[id(x, y)], board[id(selected->x, selected->y)]);
                                selected = None;
                                mode = next;
                            }
                        }
                        break;
                    }
                    case State::Black: {
                        if (press && (mode == Mode::Black)) {
                            selected = i32vec2(x, y);
                            next = Mode::White;
                        }
                        if (selected && (*selected == i32vec2(x, y))) {
                            draw_sprite(select_texture, clip_matrix, px, py, cell_size, cell_size);
                        }
                        draw_sprite(black_texture, clip_matrix, px, py, cell_size, cell_size);
                        break;
                    }
                    case State::White: {
                        if (press && (mode == Mode::White)) {
                            selected = i32vec2(x, y);
                            next = Mode::Black;
                        }
                        if (selected && (*selected == i32vec2(x, y))) {
                            draw_sprite(select_texture, clip_matrix, px, py, cell_size, cell_size);
                        }
                        draw_sprite(white_texture, clip_matrix, px, py, cell_size, cell_size);
                        break;
                    }
                }
            }
        }
        SDL_GL_SwapWindow(window.native_handle());
    }

    return 0;
}