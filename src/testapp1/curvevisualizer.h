#pragma once

#include <reactive/widget/widget.h>

reactive::widget::AnyWidget curveVisualizer(
        reactive::signal2::AnySignal<avg::Curve> curve
        );
