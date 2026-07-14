#pragma once

#include "widgetmodifier.h"

#include "bqui/widget/introspection.h"
#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <string>

namespace bqui::modifier
{
    // Replace the widget's introspection with the given (reactive) value. This
    // is the primitive; the more specific modifiers below are built on it.
    BQUI_EXPORT AnyWidgetModifier setWidgetIntrospection(
            bq::signal::AnySignal<widget::Introspection> introspection);

    BQUI_EXPORT AnyWidgetModifier setWidgetIntrospection(
            widget::Introspection introspection);

    // Give the widget a name, carried along the build. Transforms the current
    // introspection rather than replacing it, so an existing role survives.
    BQUI_EXPORT AnyWidgetModifier setName(std::string name);

    // Set the widget's role (its "what kind" tag), e.g. "ScrollBar".
    BQUI_EXPORT AnyWidgetModifier setRole(std::string role);
} // namespace bqui::modifier
