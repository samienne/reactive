#pragma once

#include <reactive/widget/widget.h>

reactive::widget::AnyWidget curveVisualizer(
        bq::signal::AnySignal<avg::Curve> curve
        );
