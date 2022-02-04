#pragma once

#include "curve.h"
#include "vector.h"
#include "transform.h"
#include "obb.h"
#include "pen.h"
#include "color.h"
#include "brush.h"
#include "rect.h"
#include "avgvisibility.h"

#include <btl/option.h>

#include <tuple>

namespace avg
{
    AVG_EXPORT float lerp(float a, float b, float t);
    AVG_EXPORT Vector2f lerp(Vector2f a, Vector2f b, float t);
    AVG_EXPORT Rect lerp(Rect a, Rect b, float t);
    AVG_EXPORT Transform lerp(Transform const a, Transform const& b, float t);
    AVG_EXPORT Obb lerp(Obb const& a, Obb const& b, float t);
    AVG_EXPORT Color lerp(Color const& a, Color const& b, float t);
    AVG_EXPORT Brush lerp(Brush const& a, Brush const& b, float t);
    AVG_EXPORT Pen lerp(Pen const& a, Pen const& b, float t);
    AVG_EXPORT Curve lerp(Curve a, Curve b, float t);

    template <typename T>
    btl::option<T> lerp(
            btl::option<T> const& a,
            btl::option<T> const& b,
            float t)
    {
        if (!a.valid())
            return b;
        if (!b.valid())
            return a;

        return btl::just(lerp(*a, *b, t));
    }

    template <typename... Ts, size_t... S>
    std::tuple<Ts...> tupleLerp(
            std::tuple<Ts...> const& a,
            std::tuple<Ts...> const& b,
            float t,
            std::index_sequence<S...>)
    {
        return std::make_tuple(
                lerp(std::get<S>(a), std::get<S>(b), t)...
                );
    }

    template <typename... Ts>
    std::tuple<Ts...> lerp(std::tuple<Ts...> const& a,
            std::tuple<Ts...> const& b, float t)
    {
        return tuple_lerp(a, b, t, std::make_index_sequence<sizeof...(Ts)>());
    }

    template <typename T>
    using LerpType = decltype(
            lerp(
                std::declval<std::decay_t<T>>(),
                std::declval<std::decay_t<T>>(),
                0.0f
                )
            );

    template <typename T, typename = void>
    struct HasLerp : std::false_type {};

    template <typename T>
    struct HasLerp<T, btl::void_t<
        LerpType<T>
        >> : std::true_type {};
} // namespace avg

