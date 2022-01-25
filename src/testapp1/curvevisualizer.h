#pragma once

#include <reactive/widget/widget.h>

reactive::widget::AnyWidget curveVisualizer(
        reactive::AnySignal<avg::Curve> curve
        );
