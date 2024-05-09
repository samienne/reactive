#pragma once

#include <reactive/widget/widget.h>

reactive::widget::AnyWidget curveVisualizer(
        reactive::signal::AnySignal<avg::Curve> curve
        );
