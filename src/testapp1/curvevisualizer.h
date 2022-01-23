#pragma once

#include <reactive/widget/widget.h>

reactive::widget::AnyWidget curveVisualizer(
        reactive::AnySignal<std::function<float(float)>> curve
        );
