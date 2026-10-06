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

    /**
     * @brief Lays widgets out with your own sizing and placement.
     *
     * Takes the same widgets as hbox(). `sizeHintMap` turns the children's
     * size hints into the container's, and `obbMap` returns one obb per child
     * for a given container size. Children go through
     * modifier::handleGravity() first, and a child keeps its state when others
     * are added or removed.
     */
    BQUI_EXPORT AnyWidget layout(SizeHintMap sizeHintMap,
            ObbMap obbMap, bq::signal::ArraySignal<AnyWidget> widgets);
}

