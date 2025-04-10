/**
 * @file math_types.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-15
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <type_traits>
#include <tuple>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "mksync/config.h"

MKS_BEGIN
template <typename T>
    requires std::is_arithmetic_v<T>
struct Point {
    T x;
    T y;
    Point() : x(0), y(0) {}
    template <typename T1, typename T2>
    Point(T1 px, T2 py) : x(px), y(py)
    {
    }
    Point(const Point &rhs) : x(rhs.x), y(rhs.y) {}
    template <typename G>
    explicit Point(const Point<G> &rhs)
    {
        x = static_cast<T>(rhs.x);
        y = static_cast<T>(rhs.y);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto cast() const
    {
        if constexpr (std::numeric_limits<T>::min() <= std::numeric_limits<G>::min() &&
                      std::numeric_limits<T>::max() <= std::numeric_limits<G>::max()) {
            return Point<G>(static_cast<G>(x), static_cast<G>(y));
        }
        else {
            if (x < std::numeric_limits<G>::min() || x > std::numeric_limits<G>::max() ||
                y < std::numeric_limits<G>::min() || y > std::numeric_limits<G>::max()) {
                throw std::overflow_error("Cannot cast to target type");
            }
            return Point<G>(static_cast<G>(x), static_cast<G>(y));
        }
    }
    template <typename G>
    auto operator+(const Point<G> &rhs) const
    {
        return Point<std::common_type_t<T, G>>(x + rhs.x, y + rhs.y);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator+(const G &rhs) const
    {
        return Point<std::common_type_t<T, G>>(x + rhs, y + rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator-(const Point &rhs) const
    {
        return Point<std::common_type_t<T, G>>(x - rhs.x, y - rhs.y);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator-(const G &rhs) const
    {
        return Point<std::common_type_t<T, G>>(x - rhs, y - rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator*(const G &rhs) const
    {
        return Point<std::common_type_t<T, G>>(x * rhs, y * rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator/(const G &rhs) const
    {
        return Point<std::common_type_t<T, G>>(x / rhs, y / rhs);
    }
    template <typename G>
    auto operator+=(const Point<G> &rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    template <typename G>
    auto operator-=(const Point<G> &rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator*=(const G &rhs)
    {
        x *= rhs;
        y *= rhs;
        return *this;
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator/=(const G &rhs)
    {
        x /= rhs;
        y /= rhs;
        return *this;
    }
    template <typename G>
    auto operator<=>(const Point<G> &rhs) const
    {
        if constexpr (std::is_floating_point_v<G> || std::is_floating_point_v<T>) {
            if (std::abs(x - rhs.x) < 1e-6 && std::abs(y - rhs.y) < 1e-6) {
                return std::partial_ordering::equivalent;
            }
            return std::tie(x, y) <=> std::tie(rhs.x, rhs.y);
        }
        else {
            return std::tie(x, y) <=> std::tie(rhs.x, rhs.y);
        }
    }
    template <typename G>
    auto operator==(const Point<G> &rhs) const
    {
        if constexpr (std::is_floating_point_v<G> || std::is_floating_point_v<T>) {
            return std::abs(x - rhs.x) < 1e-6 && std::abs(y - rhs.y) < 1e-6;
        }
        else {
            return x == rhs.x && y == rhs.y;
        }
    }

    template <typename G>
    auto operator!=(const Point<G> &rhs) const
    {
        return !(*this == rhs);
    }
};

template <typename T>
    requires std::is_arithmetic_v<T>
struct Size {
    T width;
    T height;

    Size() : width(0), height(0) {}
    template <typename T1, typename T2>
    Size(T1 sw, T2 sh) : width(sw), height(sh)
    {
    }
    Size(const Size &rhs)
    {
        width  = rhs.width;
        height = rhs.height;
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    explicit Size(const Size<G> &rhs)
    {
        width  = static_cast<T>(rhs.width);
        height = static_cast<T>(rhs.height);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto cast() const
    {
        if constexpr (std::numeric_limits<T>::min() <= std::numeric_limits<G>::min() &&
                      std::numeric_limits<T>::max() <= std::numeric_limits<G>::max()) {
            return Size<G>(static_cast<G>(width), static_cast<G>(height));
        }
        else {
            if (width < std::numeric_limits<G>::min() || width > std::numeric_limits<G>::max() ||
                height < std::numeric_limits<G>::min() || height > std::numeric_limits<G>::max()) {
                throw std::overflow_error("Cannot cast to target type");
            }
            return Size<G>(static_cast<G>(width), static_cast<G>(height));
        }
    }
    template <typename G>
    auto operator+(const Size<G> &rhs) const
    {
        return Size<std::common_type_t<T, G>>(width + rhs.width, height + rhs.height);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator+(const G &rhs) const
    {
        return Size<std::common_type_t<T, G>>(width + rhs, height + rhs);
    }
    template <typename G>
    auto operator-(const Size<G> &rhs) const
    {
        return Size<std::common_type_t<T, G>>(width - rhs.width, height - rhs.height);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator-(const G &rhs) const
    {
        return Size<std::common_type_t<T, G>>(width - rhs, height - rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator*(const G &rhs) const
    {
        return Size<std::common_type_t<T, G>>(width * rhs, height * rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator/(const G &rhs) const
    {
        return Size<std::common_type_t<T, G>>(width / rhs, height / rhs);
    }
    template <typename G>
    auto operator+=(const Size<G> &rhs)
    {
        width += rhs.width;
        height += rhs.height;
        return *this;
    }
    template <typename G>
    auto operator-=(const Size<G> &rhs)
    {
        width -= rhs.width;
        height -= rhs.height;
        return *this;
    }
    template <typename G>
    auto operator*=(const G &rhs)
    {
        width *= rhs;
        height *= rhs;
    }
    template <typename G>
    auto operator/=(const G &rhs)
    {
        width /= rhs;
        height /= rhs;
    }
    template <typename G>
    auto operator==(const Size<G> &rhs) const
    {
        if constexpr (std::is_floating_point_v<G> || std::is_floating_point_v<T>) {
            return std::abs(width - rhs.width) < 1e-6 && std::abs(height - rhs.height) < 1e-6;
        }
        else {
            return std::tie(width, height) == std::tie(rhs.width, rhs.height);
        }
    }
    template <typename G>
    auto operator!=(const Size<G> &rhs) const
    {
        return !(*this == rhs);
    }
    template <typename G>
    auto scale(const G &sw, const G &sy) const
    {
        return Size<std::common_type_t<T, G>>(width * sw, height * sy);
    }
    template <typename G>
    auto scale(const G &scale) const
    {
        return scale(scale, scale);
    }
};

template <typename T = void>
struct Rect {
    template <typename... Ts>
    static auto minimum_bounding_rectangle(const Rect<Ts> &...rhs)
    {
        using RT = std::common_type_t<Ts...>;
        return Rect<RT>(std::min({static_cast<RT>(rhs.pos.x)...}),
                        std::min({static_cast<RT>(rhs.pos.y)...}),
                        std::max({(rhs.pos.x + static_cast<RT>(rhs.size.width))...}) -
                            std::min({static_cast<RT>(rhs.pos.x)...}),
                        std::max({(rhs.pos.y + static_cast<RT>(rhs.size.height))...}) -
                            std::min({static_cast<RT>(rhs.pos.y)...}));
    }
};

template <typename T>
    requires std::is_arithmetic_v<T>
struct Rect<T> {
    Point<T> pos;
    Size<T>  size;
    Rect() : pos(), size() {}
    template <typename T1, typename T2>
    Rect(const Point<T1> &rp, const Size<T2> &rs)
        : pos(static_cast<Point<T>>(rp)), size(static_cast<Size<T>>(rs))
    {
        assert(size.width >= 0 && size.height >= 0);
        size.width  = std::max(size.width, static_cast<T>(0));
        size.height = std::max(size.height, static_cast<T>(0));
    }
    template <typename T1, typename T2, typename T3, typename T4>
    Rect(T1 px, T2 py, T3 sw, T4 sh) : pos(px, py), size(sw, sh)
    {
        assert(size.width >= 0 && size.height >= 0);
        size.width  = std::max(size.width, static_cast<T>(0));
        size.height = std::max(size.height, static_cast<T>(0));
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    operator Rect<G>() const
    {
        return Rect<G>(static_cast<Point<G>>(pos), static_cast<Size<G>>(size));
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto cast() const
    {
        return Rect<G>(pos.template cast<G>(), size.template cast<G>);
    }
    template <typename G>
    auto operator==(const Rect<G> &rhs) const
    {
        return pos == rhs.pos && size == rhs.size;
    }
    template <typename G>
    auto operator!=(const Rect<G> &rhs) const
    {
        return std::tie(pos, size) != std::tie(rhs.pos, rhs.size);
    }
    template <typename G>
    auto scale(const G &scale) const
    {
        assert(scale > 0);
        return Rect(pos, size.scale(scale));
    }
    template <typename G>
    auto scale(const G &sw, const G &sy) const
    {
        assert(sw > 0 && sy > 0);
        return Rect(pos, size.scale(sw, sy));
    }
    template <typename G>
    auto move(const Point<G> &rp) const
    {
        return Rect(rp, size);
    }

    auto center() const { return (pos + Point<T>(pos.x + size.width, pos.y + size.height)) / 2; }
    template <typename G>
    auto move_center(const Point<G> &rp) const
    {
        return Rect(pos + rp - center(), size);
    }
    template <typename G>
    auto contains(const Point<G> &rp) const
    {
        return rp >= top_left() && rp <= bottom_right();
    }
    template <typename G>
    auto contains(const Rect<G> &rect) const
    {
        return rect.top_left() >= top_left() && rect.bottom_right() <= bottom_right();
    }
    template <typename G>
    auto intersects(const Rect<G> &rect) const
    {
        return !(rect.top() >= bottom() || rect.left() >= right() || rect.bottom() <= top() ||
                 rect.right() <= left());
    }
    template <typename G>
    auto intersection(const Rect<G> &rect) const
    {
        using RT = std::common_type_t<T, G>;
        if (!intersects(rect)) {
            return Rect<RT>();
        }
        return Rect<RT>(
            Point<RT>(std::max(static_cast<RT>(left()), static_cast<RT>(rect.left())),
                      std::max(static_cast<RT>(top()), static_cast<RT>(rect.top()))),
            Size<RT>(std::min(static_cast<RT>(right()), static_cast<RT>(rect.right())) -
                         std::max(static_cast<RT>(left()), static_cast<RT>(rect.left())),
                     std::min(static_cast<RT>(bottom()), static_cast<RT>(rect.bottom())) -
                         std::max(static_cast<RT>(top()), static_cast<RT>(rect.top()))));
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto adjust(G dx1, G dy1, G dx2, G dy2) const
    {
        return Rect(pos + Point<T>(dx1, dy1), size + Size<T>(-dx1 - dx2, -dy1 - dy2));
    }
    auto top_left() const { return pos; }
    auto top_right() const { return pos + Point<T>(size.width, 0); }
    auto bottom_left() const { return pos + Point<T>(0, size.height); }
    auto bottom_right() const { return pos + Point<T>(size.width, size.height); }
    auto top() const { return pos.y; }
    auto bottom() const { return pos.y + size.height; }
    auto left() const { return pos.x; }
    auto right() const { return pos.x + size.width; }
};

template <typename T>
    requires std::is_arithmetic_v<T>
struct Line {
    Point<T> p1;
    Point<T> p2;

    template <typename T1, typename T2>
    Line(const Point<T1> &rp1, const Point<T2> &rp2) : p1(rp1), p2(rp2)
    {
    }
    template <typename T1, typename T2, typename T3, typename T4>
    Line(T1 x1, T2 y1, T3 x2, T4 y2) : p1(x1, y1), p2(x2, y2)
    {
    }
    template <typename G>
    operator Line<G>() const
    {
        return Line<G>(static_cast<Point<G>>(p1), static_cast<Point<G>>(p2));
    }
    template <typename G>
    auto cast() const
    {
        return Line<G>(p1.template cast<G>(), p2.template cast<G>());
    }
    auto k() const { return (p2.y - p1.y) / (p2.x - p1.x); }
    auto b() const { return p1.y - (k() * p1.x); }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator()(const G &px) const
    {
        return (k() * px) + b();
    }
    template <typename G>
    auto contain(const Point<G> &rp) const
    {
        return std::abs(rp.y - (*this)(rp.x)) < 1e-6;
    }
    template <typename G>
    auto intersect(const Line<G> &rhs) const
    {
        using RT = std::common_type_t<T, G>;
        if (k() == rhs.k()) {
            return Point<RT>{0, 0};
        }
        return Point<RT>((RT)(rhs.b() - b()) / (k() - rhs.k()),
                         (k() * ((rhs.b() - b()) / (k() - rhs.k()))) + b());
    }
    auto length() const { return std::sqrt(std::pow(p2.x - p1.x, 2) + std::pow(p2.y - p1.y, 2)); }
};

template <typename T>
    requires std::is_arithmetic_v<T>
struct Vec2 : public Point<T> {
    using Point<T>::Point;
    Vec2(const Point<T> &rp) : Point<T>(rp) {}

    template <typename G>
    auto cross(const Vec2<G> &rhs) const
    {
        if constexpr (std::is_signed_v<std::common_type_t<T, G>>) {
            return (std::common_type_t<T, G>)(this->x * rhs.y) - (this->y * rhs.x);
        }
        else {
            return (std::make_signed_t<std::common_type_t<T, G>>)((this->x * rhs.y) -
                                                                  (this->y * rhs.x));
        }
    }
    template <typename G>
    auto dot(const Vec2<G> &rhs)
    {
        return (this->x * rhs.x) + (this->y * rhs.y);
    }
    template <typename G>
    auto operator*(const Vec2<G> &rhs) const
    {
        return (this->x * rhs.x) + (this->y * rhs.y);
    }
    template <typename G>
    auto operator+(const Vec2<G> &rhs) const
    {
        return Vec2<G>(this->x + rhs.x, this->y + rhs.y);
    }
    template <typename G>
    auto operator-(const Vec2<G> &rhs) const
    {
        return Vec2<G>(this->x - rhs.x, this->y - rhs.y);
    }

    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator+(const G &rhs) const
    {
        return Vec2<std::common_type_t<T, G>>(this->x + rhs, this->y + rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator-(const G &rhs) const
    {
        return Vec2<std::common_type_t<T, G>>(this->x - rhs, this->y - rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator*(const G &rhs) const
    {
        return Vec2<std::common_type_t<T, G>>(this->x * rhs, this->y * rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator/(const G &rhs) const
    {
        return Vec2<std::common_type_t<T, G>>(this->x / rhs, this->y / rhs);
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator+=(const G &rhs)
    {
        this->x += rhs;
        this->y += rhs;
        return *this;
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator-=(const G &rhs)
    {
        this->x -= rhs;
        this->y -= rhs;
        return *this;
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator*=(const G &rhs)
    {
        this->x *= rhs;
        this->y *= rhs;
        return *this;
    }
    template <typename G>
        requires std::is_arithmetic_v<G>
    auto operator/=(const G &rhs)
    {
        this->x /= rhs;
        this->y /= rhs;
        return *this;
    }
};

template <typename... Ts>
    requires(std::is_arithmetic_v<Ts> && ...)
Point(Ts...) -> Point<std::common_type_t<Ts...>>;

template <typename... Ts>
    requires(std::is_arithmetic_v<Ts> && ...)
Size(Ts...) -> Size<std::common_type_t<Ts...>>;

template <typename... Ts>
    requires(std::is_arithmetic_v<Ts> && ...)
Rect(Ts...) -> Rect<std::common_type_t<Ts...>>;
template <typename T>
    requires std::is_arithmetic_v<T>
Rect(const Point<T> &, const Size<T> &) -> Rect<T>;
template <typename T1, typename T2>
    requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
Rect(const Point<T1> &, const Size<T2> &) -> Rect<std::common_type_t<T1, T2>>;

template <typename... Ts>
    requires(std::is_arithmetic_v<Ts> && ...)
Line(Ts...) -> Line<std::common_type_t<Ts...>>;
template <typename T1, typename T2>
    requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
Line(const Point<T1> &, const Point<T2> &) -> Line<std::common_type_t<T1, T2>>;

template <typename T>
    requires std::is_arithmetic_v<T>
Vec2(const Point<T> &) -> Vec2<T>;
template <typename... Ts>
    requires(std::is_arithmetic_v<Ts> && ...)
Vec2(Ts...) -> Vec2<std::common_type_t<Ts...>>;
MKS_END