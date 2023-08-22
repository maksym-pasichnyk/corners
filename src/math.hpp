#pragma once

template<size_t, typename>
struct vector_type;

template<typename T>
struct vector_type<2, T> {
    T x;
    T y;

    friend constexpr auto operator<=>(vector_type const&, vector_type const&) noexcept = default;
};

template<typename T>
struct vector_type<3, T> {
    T x;
    T y;
    T z;

    friend constexpr auto operator<=>(vector_type const&, vector_type const&) noexcept = default;
};

template<typename T>
struct vector_type<4, T> {
    T x;
    T y;
    T z;
    T w;

    friend constexpr auto operator<=>(vector_type const&, vector_type const&) noexcept = default;
};

template<typename T>
vector_type(T, T) -> vector_type<2, T>;

template<typename T>
vector_type(T, T, T) -> vector_type<3, T>;

template<typename T>
vector_type(T, T, T, T) -> vector_type<4, T>;

using i32vec2 = vector_type<2, i32>;
using i32vec3 = vector_type<3, i32>;
using i32vec4 = vector_type<4, i32>;

using f32vec2 = vector_type<2, f32>;
using f32vec3 = vector_type<3, f32>;
using f32vec4 = vector_type<4, f32>;

template<typename T, size_t N1, size_t N2>
struct matrix_type;

template<typename T>
struct matrix_type<T, 2, 2> {
    T m00, m01;
    T m10, m11;
};

template<typename T>
struct matrix_type<T, 2, 3> {
    T m00, m01, m02;
    T m10, m11, m12;
};

template<typename T>
struct matrix_type<T, 2, 4> {
    T m00, m01, m02, m03;
    T m10, m11, m12, m13;
};

template<typename T>
struct matrix_type<T, 3, 2> {
    T m00, m01;
    T m10, m11;
    T m20, m21;
};

template<typename T>
struct matrix_type<T, 3, 3> {
    T m00, m01, m02;
    T m10, m11, m12;
    T m20, m21, m22;
};

template<typename T>
struct matrix_type<T, 3, 4> {
    T m00, m01, m02, m03;
    T m10, m11, m12, m13;
    T m20, m21, m22, m23;
};

template<typename T>
struct matrix_type<T, 4, 4> {
    T m00, m01, m02, m03;
    T m10, m11, m12, m13;
    T m20, m21, m22, m23;
    T m30, m31, m32, m33;
};

using i32mat2x2 = matrix_type<i32, 2, 2>;
using i32mat2x3 = matrix_type<i32, 2, 3>;
using i32mat2x4 = matrix_type<i32, 2, 4>;

using i32mat3x2 = matrix_type<i32, 3, 2>;
using i32mat3x3 = matrix_type<i32, 3, 3>;
using i32mat3x4 = matrix_type<i32, 3, 4>;

using i32mat4x2 = matrix_type<i32, 4, 2>;
using i32mat4x3 = matrix_type<i32, 4, 3>;
using i32mat4x4 = matrix_type<i32, 4, 4>;

using f32mat2x2 = matrix_type<f32, 2, 2>;
using f32mat2x3 = matrix_type<f32, 2, 3>;
using f32mat2x4 = matrix_type<f32, 2, 4>;

using f32mat3x2 = matrix_type<f32, 3, 2>;
using f32mat3x3 = matrix_type<f32, 3, 3>;
using f32mat3x4 = matrix_type<f32, 3, 4>;

using f32mat4x2 = matrix_type<f32, 4, 2>;
using f32mat4x3 = matrix_type<f32, 4, 3>;
using f32mat4x4 = matrix_type<f32, 4, 4>;

#define impl_vector_type_operators(op)                                                                         \
template<size_t N, typename T>                                                                                 \
constexpr auto operator op(vector_type<N, T> const& lhs, vector_type<N, T> const& rhs) -> vector_type<N, T> {  \
    if constexpr(N == 2) {                                                                                     \
        return vector_type(lhs.x op rhs.x, lhs.y op rhs.y);                                                    \
    } else if constexpr(N == 3) {                                                                              \
        return vector_type(lhs.x op rhs.x, lhs.y op rhs.y, lhs.z op rhs.z);                                    \
    } else if constexpr(N == 4) {                                                                              \
        return vector_type(lhs.x op rhs.x, lhs.y op rhs.y, lhs.z op rhs.z, lhs.w op rhs.w);                    \
    } else {                                                                                                   \
        static_assert(false);                                                                                  \
    }                                                                                                          \
}                                                                                                              \
template<size_t N, typename T>                                                                                 \
constexpr auto operator op(vector_type<N, T> const& lhs, T const& rhs) -> vector_type<N, T> {                  \
    if constexpr(N == 2) {                                                                                     \
        return vector_type(lhs.x op rhs, lhs.y op rhs);                                                        \
    } else if constexpr(N == 3) {                                                                              \
        return vector_type(lhs.x op rhs, lhs.y op rhs, lhs.z op rhs);                                          \
    } else if constexpr(N == 4) {                                                                              \
        return vector_type(lhs.x op rhs, lhs.y op rhs, lhs.z op rhs, lhs.w op rhs);                            \
    } else {                                                                                                   \
        static_assert(false);                                                                                  \
    }                                                                                                          \
}                                                                                                              \
template<size_t N, typename T>                                                                                 \
constexpr auto operator op(T const& lhs, vector_type<N, T> const& rhs) -> vector_type<N, T> {                  \
    if constexpr(N == 2) {                                                                                     \
        return vector_type(lhs op rhs.x, lhs op rhs.y);                                                        \
    } else if constexpr(N == 3) {                                                                              \
        return vector_type(lhs op rhs.x, lhs op rhs.y, lhs op rhs.z);                                          \
    } else if constexpr(N == 4) {                                                                              \
        return vector_type(lhs op rhs.x, lhs op rhs.y, lhs op rhs.z, lhs op rhs.w);                            \
    } else {                                                                                                   \
        static_assert(false);                                                                                  \
    }                                                                                                          \
}

impl_vector_type_operators(+)
impl_vector_type_operators(-)
impl_vector_type_operators(*)
impl_vector_type_operators(/)

constexpr auto orthographic(f32 r, f32 l, f32 t, f32 b) -> f32mat4x4 {
    f32 x = 2.0F / (r - l);
    f32 y = 2.0F / (t - b);

    return f32mat4x4(
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );
}

constexpr auto orthographic_from_size(f32 width, f32 height) {
    auto r = +f32(width) * 0.5F;
    auto l = -f32(width) * 0.5F;
    auto t = +f32(height) * 0.5F;
    auto b = -f32(height) * 0.5F;
    return orthographic(r, l, t, b);
}