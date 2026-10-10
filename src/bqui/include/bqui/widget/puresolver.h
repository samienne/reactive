#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

namespace bqui::widget
{
    /**
     * @brief Wraps a subtree in one pure-solver region: the containers inside
     * emit band-free constraints plus the universal weak defaults into a single
     * shared solve, ignoring their SizeHint bands.
     *
     * A hbox or vbox that joins the region contributes a pure fragment rather
     * than a banded one. The container states only structure -- children tiled
     * edge to edge with the trailing slack on a signed gap variable, and a
     * leading-edge pin plus weak default on the cross axis -- while each leaf
     * owns its own weak width==100 / height==100 default and each filler()
     * couples to the container's shared flex variable, so the fillers split the
     * leftover space and reflow live as the window resizes. The pure size
     * vocabulary in bqui/modifier/constraintsize.h (fixedWidth, minWidth,
     * maxWidth and their height/size forms) firms up individual children.
     *
     * A container that finds no region around it runs the shipped per-container
     * banded path unchanged, so this is a strict opt-in.
     */
    BQUI_EXPORT AnyWidget pureSolverRoot(AnyWidget content);
} // namespace bqui::widget
