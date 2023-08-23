#pragma once

#include "SDL.h"
#include "stb_image.h"

struct LogicalSize {
    u32 width;
    u32 height;
};
struct PhysicalSize {
    u32 width;
    u32 height;
};

struct WindowBuilder;

struct Window {
    friend WindowBuilder;

    auto native_handle() const -> SDL_Window* {
        return handle.get();
    }

    auto get_physical_size() const -> PhysicalSize {
        i32 width, height;
        SDL_GL_GetDrawableSize(handle.get(), &width, &height);
        return PhysicalSize(width, height);
    }

private:
    struct Drop {
        void operator()(SDL_Window* ptr) {
            SDL_DestroyWindow(ptr);
        }
    };

    explicit Window() : handle(nullptr) {}
    explicit Window(SDL_Window* ptr) : handle(ptr) {}

    std::unique_ptr<SDL_Window, Drop> handle;
};

struct WindowBuilder {
    std::string title = {};
    i32         x     = {};
    i32         y     = {};
    LogicalSize size  = {};
    i32         flags = {};

    static auto new_() -> WindowBuilder {
        return {
            .title = {},
            .x = SDL_WINDOWPOS_CENTERED,
            .y = SDL_WINDOWPOS_CENTERED,
            .size = LogicalSize(1, 1),
            .flags = 0,
        };
    }

    auto set_title(std::string title) -> WindowBuilder& {
        this->title = std::move(title);
        return *this;
    }

    auto set_size(LogicalSize const& size) -> WindowBuilder& {
        this->size = size;
        return *this;
    }

    auto set_fullscreen(bool flag) -> WindowBuilder& {
        if (flag) {
            this->flags |= SDL_WINDOW_FULLSCREEN;
        } else {
            this->flags &= ~SDL_WINDOW_FULLSCREEN;
        }
        return *this;
    }

    auto set_resizable(bool flag) -> WindowBuilder& {
        if (flag) {
            this->flags |= SDL_WINDOW_RESIZABLE;
        } else {
            this->flags &= ~SDL_WINDOW_RESIZABLE;
        }
        return *this;
    }

    auto set_alow_high_dpi(bool flag) -> WindowBuilder& {
        if (flag) {
            this->flags |= SDL_WINDOW_ALLOW_HIGHDPI;
        } else {
            this->flags &= ~SDL_WINDOW_ALLOW_HIGHDPI;
        }
        return *this;
    }

    auto set_x(i32 x) -> WindowBuilder& {
        this->x = x;
        return *this;
    }

    auto set_y(i32 y) -> WindowBuilder& {
        this->y = y;
        return *this;
    }

    auto create() -> Window {
        return Window(SDL_CreateWindow(
            this->title.c_str(),
            this->x,
            this->y,
            this->size.width,
            this->size.height,
            this->flags
        ));
    }
};

struct Renderer {
    static auto new_(Window& window) -> Renderer {
        return Renderer(SDL_CreateRenderer(window.native_handle(), -1, 0));
    }

    auto native_handle() const -> SDL_Renderer* {
        return handle.get();
    }

    void present() {
        SDL_RenderPresent(handle.get());
    }

private:
    struct Drop {
        void operator()(SDL_Renderer* ptr) {
            SDL_DestroyRenderer(ptr);
        }
    };

    explicit Renderer() : handle(nullptr) {}
    explicit Renderer(SDL_Renderer* ptr) : handle(ptr) {}

    std::unique_ptr<SDL_Renderer, Drop> handle;
};

struct Event_Quit {};
struct Event_RequestRedraw {};
struct Event_MouseButtonUp {
    u32 timestamp;
    u32 windowID;
    u32 which;
    u8 button;
    u8 state;
    u8 clicks;
    i32 x;
    i32 y;
};
struct Event_MouseButtonDown {
    u32 timestamp;
    u32 windowID;
    u32 which;
    u8 button;
    u8 state;
    u8 clicks;
    i32 x;
    i32 y;
};
struct Event_EventsCleared {};
struct Event_LoopExiting {};

struct Event_MouseMotion {
    u32 timestamp;
    u32 windowID;
    u32 which;
    u32 state;
    i32 x;
    i32 y;
    i32 xrel;
    i32 yrel;
};

struct Event : Enum<
    Event_Quit,
    Event_RequestRedraw,
    Event_EventsCleared,
    Event_LoopExiting,
    Event_MouseMotion,
    Event_MouseButtonUp,
    Event_MouseButtonDown
> {
    using Quit = Event_Quit;
    using RequestRedraw = Event_RequestRedraw;
    using EventsCleared = Event_EventsCleared;
    using MouseMotion = Event_MouseMotion;
    using MouseButtonUp = Event_MouseButtonUp;
    using MouseButtonDown = Event_MouseButtonDown;
    using LoopExiting = Event_LoopExiting;

    using Enum::Enum;
};

struct ControlFlow {
    void request_exit() {
        _flag.store(true);
    }

    auto exit_requested() -> bool {
        return _flag.load();
    }

private:
    std::atomic_bool _flag;
};

#ifdef EMSCRIPTEN
typedef void (*em_callback_func)();
typedef void (*em_arg_callback_func)(void*);

extern "C" void emscripten_set_main_loop(void(*)(), i32, i32);
extern "C" void emscripten_set_main_loop_arg(void(*)(void*), void*, i32, i32);
extern "C" void emscripten_cancel_main_loop();
#endif

struct EventLoop {
    static auto new_() -> EventLoop {
        return {};
    }

    template<typename Fn> requires std::invocable<Fn, Event const&, ControlFlow&>
    [[noreturn]] void run(Fn&& fn) {
#ifdef EMSCRIPTEN
        struct EventHandler {
            Fn fn;
        };

        auto* handler = new EventHandler(std::forward<Fn>(fn));
        emscripten_set_main_loop_arg([](void* arg) {
            ControlFlow control_flow = {};

            auto* handler = static_cast<EventHandler*>(arg);
            while (auto event = poll_event()) {
                handler->fn(*event, control_flow);
            }
            handler->fn(Event(Event::EventsCleared()), control_flow);
            handler->fn(Event(Event::RequestRedraw()), control_flow);

            if (control_flow.exit_requested()) {
                handler->fn(Event(Event::LoopExiting()), control_flow);
                emscripten_cancel_main_loop();
            }
        }, handler, -1, 1);
        std::unreachable();
#else
        ControlFlow control_flow = {};
        while (!control_flow.exit_requested()) {
            while (auto event = poll_event()) {
                fn(*event, control_flow);
            }
            fn(Event(Event::EventsCleared()), control_flow);
            fn(Event(Event::RequestRedraw()), control_flow);
        }
        fn(Event(Event::LoopExiting()), control_flow);
        std::exit(0);
#endif
    }

    static auto poll_event() -> Option<Event> {
        SDL_Event event;
        if (SDL_PollEvent(&event) == 1) {
            return transform(event);
        }
        return None;
    }

    static auto wait_event() -> Option<Event> {
        SDL_Event event;
        if (SDL_WaitEvent(&event) == 1) {
            return transform(event);
        }
        return None;
    }

    static auto transform(SDL_Event const& event) -> Option<Event> {
        switch (event.type) {
            case SDL_QUIT: {
                return Event::Quit();
            }
            case SDL_MOUSEBUTTONDOWN: {
                return Event::MouseButtonDown{
                    event.button.timestamp,
                    event.button.windowID,
                    event.button.which,
                    event.button.button,
                    event.button.state,
                    event.button.clicks,
                    event.button.x,
                    event.button.y,
                };
            }
            case SDL_MOUSEBUTTONUP: {
                return Event::MouseButtonUp{
                    event.button.timestamp,
                    event.button.windowID,
                    event.button.which,
                    event.button.button,
                    event.button.state,
                    event.button.clicks,
                    event.button.x,
                    event.button.y,
                };
            }
            case SDL_MOUSEMOTION: {
                return Event::MouseMotion{
                    event.motion.timestamp,
                    event.motion.windowID,
                    event.motion.which,
                    event.motion.state,
                    event.motion.x,
                    event.motion.y,
                    event.motion.xrel,
                    event.motion.yrel,
                };
            }
            default: {
                return None;
            }
        }
    }
};

struct Texture {
    std::string path;
};

struct GpuTexture {
    SDL_Texture* native_handle;
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

    static auto open(Texture const& texture, Renderer& renderer) -> Option<Result> {
        i32 width;
        i32 height;
        stbi_set_flip_vertically_on_load_thread(true);
        auto image = Image(stbi_load(texture.path.c_str(), &width, &height, nullptr, 4));
        if (image == nullptr) {
            return None;
        }

        auto* surface = SDL_CreateRGBSurfaceWithFormatFrom(image.get(), width, height, 0, width * 4, SDL_PIXELFORMAT_ABGR8888);
        auto* native_handle = SDL_CreateTextureFromSurface(renderer.native_handle(), surface);
        SDL_FreeSurface(surface);
        return GpuTexture(native_handle);
    }
};

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

    auto add(T resource, Renderer& renderer) -> Handle<T> {
        auto handle = Handle<T>(next_resource++);
        resources.emplace(handle, Loader::open(resource, renderer).unwrap());
        return handle;
    }

    auto get(Handle<T> const& handle) & -> Resource& {
        return resources.at(handle);
    }

private:
    u64 next_resource = 0;
    std::unordered_map<Handle<T>, Resource> resources;
};

struct AssetManager {
    Assets<Texture> textures;

    static auto new_() -> AssetManager {
        return {};
    }
};