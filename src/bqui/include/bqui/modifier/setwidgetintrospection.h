#pragma once

#include "widgetmodifier.h"

#include "bqui/widget/introspection.h"
#include "bqui/widget/datavalue.h"
#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <string>

namespace bqui::modifier
{
    /// Replace the widget's introspection with the given (reactive) value. The
    /// full escape hatch; the more specific modifiers below build on it.
    BQUI_EXPORT AnyWidgetModifier setWidgetIntrospection(
            bq::signal::AnySignal<widget::Introspection> introspection);

    /// \overload
    BQUI_EXPORT AnyWidgetModifier setWidgetIntrospection(
            widget::Introspection introspection);

    /// Give the widget a name. Transforms the current introspection rather than
    /// replacing it, so an existing role, data, and children survive.
    BQUI_EXPORT AnyWidgetModifier setName(
            bq::signal::AnySignal<std::string> name);

    /// \overload
    BQUI_EXPORT AnyWidgetModifier setName(std::string name);

    /// Set the widget's role (its "what kind" tag), e.g. "Button". Transforms
    /// the current introspection rather than replacing it.
    BQUI_EXPORT AnyWidgetModifier setRole(
            bq::signal::AnySignal<std::string> role);

    /// \overload
    BQUI_EXPORT AnyWidgetModifier setRole(std::string role);

    /// Set (or override) a single key in the widget's introspection data bag,
    /// leaving other keys untouched.
    BQUI_EXPORT AnyWidgetModifier setData(std::string key,
            bq::signal::AnySignal<widget::DataValue> value);

    /// \overload
    BQUI_EXPORT AnyWidgetModifier setData(std::string key,
            widget::DataValue value);

    /// Accumulate a capability onto the widget's own introspection node. The
    /// capability applies to that node's obb.
    BQUI_EXPORT AnyWidgetModifier addCapability(widget::Capability capability);

    /// Set the widget's introspection obb from its own realised size, in the
    /// widget's local coordinate space. A container lifts these into window
    /// space as it aggregates (see widget/layout.cpp). Self-describing widgets
    /// apply this so their node reports real geometry.
    BQUI_EXPORT AnyWidgetModifier setIntrospectionObb();
} // namespace bqui::modifier
