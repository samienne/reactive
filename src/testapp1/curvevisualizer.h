#pragma once

#include <bqui/widget/widget.h>

bqui::widget::AnyWidget curveVisualizer(
        bq::signal::AnySignal<avg::Curve> curve
        );
