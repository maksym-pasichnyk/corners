#pragma once

#include <set>
#include <deque>
#include <array>
#include <mutex>
#include <vector>
#include <memory>
#include <numbers>
#include <variant>
#include <unordered_map>

#include "fmt/format.h"

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

template<typename... Fn>
struct overloads : Fn ... {
    using Fn::operator()...;
};

template<typename... Fn>
overloads(Fn...) -> overloads<Fn...>;

template<typename T, typename... Fn>
constexpr auto operator|(T&& obj, overloads<Fn...> const& cases) -> decltype(std::visit(cases, std::forward<T>(obj))) {
    return std::visit(cases, std::forward<T>(obj));
}

#define match_(...) (__VA_ARGS__) | overloads
#define case_ [&]

template<typename... Variant>
struct Enum : std::variant<Variant...> {
    using variant = std::variant<Variant...>;
    using variant::variant;

private:
    template<typename T, typename Self>
    [[nodiscard]] constexpr auto as_impl(Self&& self) -> decltype(auto) {
        return std::get<T>(std::forward<Self>(self));
    }

public:
    template<typename T>
    [[nodiscard]] constexpr auto is() const -> bool {
        return std::holds_alternative<T>(*this);
    }

    template<typename T>
    [[nodiscard]] constexpr auto as()& -> T& {
        return as_impl<T>(*this);
    }

    template<typename T>
    [[nodiscard]] constexpr auto as() const& -> T const& {
        return as_impl<T>(*this);
    }

    template<typename T>
    [[nodiscard]] constexpr auto as()&& -> T&& {
        return as_impl<T>(std::move(*this));
    }

    template<typename T>
    [[nodiscard]] constexpr auto as() const&& -> T const&& {
        return as_impl<T>(std::move(*this));
    }

    template<typename T>
    [[nodiscard]] constexpr auto get_if()& -> T* {
        return std::get_if<T>(this);
    }

    template<typename T>
    [[nodiscard]] constexpr auto get_if() const& -> T const* {
        return std::get_if<T>(this);
    }

    template<typename T>
    [[nodiscard]] constexpr auto get_if() && -> T* = delete;

    template<typename T>
    [[nodiscard]] constexpr auto get_if() const && -> T const* = delete;
};

constexpr struct {} None;

template<typename T>
struct Option : public Enum<std::decay_t<decltype(None)>, T> {
private:
    using Enum = Enum<std::decay_t<decltype(None)>, T>;
    using Enum::Enum;

    template<typename U>
    friend constexpr auto Some(U&& value) -> Option<U>;

private:
    template<typename Self>
    static constexpr auto unwrap_impl(Self&& self) -> decltype(auto) {
        return std::forward<Self>(self).Enum::template as<T>();
    }

public:
    [[nodiscard]] constexpr auto unwrap() & -> T& {
        return Option::unwrap_impl(*this);
    }

    [[nodiscard]] constexpr auto unwrap() const& -> T const& {
        return Option::unwrap_impl(*this);
    }

    [[nodiscard]] constexpr auto unwrap() && -> T&& {
        return Option::unwrap_impl(std::move(*this));
    }

    [[nodiscard]] constexpr auto unwrap() const&& -> T const&& {
        return Option::unwrap_impl(std::move(*this));
    }

    template<typename U>
    [[nodiscard]] constexpr auto unwrap_or(U&& other) & -> U {
        return match_(*this) {
            case_(T& self) {
                return static_cast<U>(self);
            },
            case_(auto) {
                return std::forward<U>(other);
            }
        };
    }

    template<typename U>
    [[nodiscard]] constexpr auto unwrap_or(U&& other) const& -> U {
        return match_(*this) {
            case_(T const& self) {
                return static_cast<U>(self);
            },
            case_(auto) {
                return std::forward<U>(other);
            }
        };
    }

//    template<typename U>
//    [[nodiscard]] auto unwrap_or(U&& other) && -> T&& = delete;
//
//    template<typename U>
//    [[nodiscard]] auto unwrap_or(U&& other) const&& -> T const&& = delete;

    [[nodiscard]] constexpr auto is_some() const noexcept -> bool {
        return this->Enum::template is<T>();
    }

    [[nodiscard]] constexpr auto is_none() const noexcept -> bool {
        return !is_some();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_some();
    }

    [[nodiscard]] constexpr auto operator*() & -> T& {
        return *this->Enum::template get_if<T>();
    }

    [[nodiscard]] constexpr auto operator*() const& -> T const& {
        return *this->Enum::template get_if<T>();
    }

    [[nodiscard]] constexpr auto operator*() && -> T&& {
        return std::move(*this->Enum::template get_if<T>());
    }

    [[nodiscard]] constexpr auto operator*() const&& -> T const&& {
        return std::move(*this->Enum::template get_if<T>());
    }

    [[nodiscard]] constexpr auto operator->() & -> T* {
        return std::addressof(this->Enum::template as<T>());
    }

    [[nodiscard]] constexpr auto operator->() const& -> T const* {
        return std::addressof(this->Enum::template as<T>());
    }

    [[nodiscard]] constexpr auto operator->() && -> T* = delete;
    [[nodiscard]] constexpr auto operator->() const && -> T* = delete;

    template<typename Fn, typename R = std::invoke_result_t<Fn, T const&>>
    [[nodiscard]] constexpr auto map(Fn&& fn) const& -> Option<R> {
        return match_(*this) {
            case_(T const& self) -> Option<R> {
                return Some(fn(self));
            },
            case_(auto) -> Option<R> {
                return None;
            }
        };
    }

    template<typename Fn, typename R = std::invoke_result_t<Fn, T&>>
    [[nodiscard]] constexpr auto map_or(R&& other, Fn&& fn)& -> R {
        return match_(*this) {
            case_(T& self) -> R {
                return fn(self);
            },
            case_(auto) -> R {
                return std::forward<R>(other);
            }
        };
    }

    template<typename Fn, typename R = std::invoke_result_t<Fn, T const&>>
    [[nodiscard]] constexpr auto map_or(R&& other, Fn&& fn) const& -> R {
        return match_(*this) {
            case_(T const& self) -> R {
                return fn(self);
            },
            case_(auto) -> R {
                return std::forward<R>(other);
            }
        };
    }
};

template<typename U>
constexpr auto Some(U&& value) -> Option<U> {
    return Option<U>{std::forward<U>(value)};
}

template<typename T>
struct ManagedPtr;

template<typename T>
struct Managed {
    explicit Managed() : strong_references(1) {}
    virtual ~Managed() = default;

    auto retain() -> T* {
        strong_references.fetch_add(1);
        return static_cast<T*>(this);
    }

    void release() {
        if (strong_references.fetch_sub(1) == 1) {
            delete this;
        }
    }

    auto references() -> size_t {
        return strong_references;
    }

    auto shared_from_this() -> ManagedPtr<T> {
        return ManagedPtr<T>(retain());
    }

private:
    std::atomic_size_t strong_references;
};

template<typename T>
struct ManagedPtr final {
    explicit ManagedPtr() : handle(nullptr) {}
    explicit ManagedPtr(T* pointer) : handle(pointer) {}

    ManagedPtr(ManagedPtr const& other) : handle(other.get()) {
        if (handle) { handle->Managed::retain(); }
    }
    ManagedPtr(ManagedPtr&& other) noexcept = default;

    auto operator=(ManagedPtr const& other) -> ManagedPtr& {
        *this = ManagedPtr(other);
        return *this;
    }

    auto operator=(ManagedPtr&& other) noexcept -> ManagedPtr& = default;

    auto operator*() const& -> T& {
        return handle.get();
    }
    auto operator->() const& -> T* {
        return handle.get();
    }

    auto get() const noexcept -> T* {
        return handle.get();
    }

    auto release() noexcept -> T* {
        return handle.release();
    }

protected:
    struct Dtor {
        void operator()(T* ptr) {
            ptr->Managed::release();
        }
    };

    std::unique_ptr<T, Dtor> handle;
};

template<std::invocable Fn>
struct Lazy : Fn {
    explicit operator auto() {
        return Fn::operator()();
    }
};

template<std::invocable Fn>
Lazy(Fn) -> Lazy<Fn>;

#define label_(name)                    \
    if constexpr (false) {              \
        __break__ ## name: break;       \
        __continue__ ## name: continue; \
    } else

#define break_(name) goto __break__ ## name
#define continue_(name) goto __continue__ ## name