#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

#include <btl/function.h>

#include <vector>

namespace bqui::widget
{
    using ObbMap = btl::Function<
        std::vector<avg::Obb>(ase::Vector2f size,
                std::vector<SizeHint> const&)>;

    template <typename T>
    struct IsObbMap :
        btl::All<
            std::is_assignable<ObbMap, T>,
            std::is_copy_constructible<T>
        > {};

    using SizeHintMap = btl::Function<
        SizeHint(std::vector<SizeHint> const&)
        >;

    template <typename T>
    struct IsSizeHintMap :
        btl::All<
            std::is_assignable<SizeHintMap, T>,
            std::is_copy_constructible<T>
        > {};

    /** @brief Places a list of children, whose membership may change.
     *
     * The one layout engine. Children enter as an array, so a fixed list and
     * one driven by forEach() are the same case: every child is built once per
     * identity, and an insertion or a removal leaves its siblings — and
     * whatever they have accumulated — untouched.
     *
     * `sizeHintMap` computes the container's own hint from the children's, and
     * `obbMap` places them; both see every child's hint at once. `obbMap` must
     * return one obb per hint, which is checked. Every child is put through
     * modifier::handleGravity() before it is built.
     */
    BQUI_EXPORT AnyWidget layout(SizeHintMap sizeHintMap,
            ObbMap obbMap, bq::signal::ArraySignal<AnyWidget> widgets);

    /** @overload */
    BQUI_EXPORT AnyWidget layout(SizeHintMap sizeHintMap,
            ObbMap obbMap, std::vector<AnyWidget> widgets);
}

