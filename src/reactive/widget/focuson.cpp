#include "widget/focuson.h"

#include "widget/setfocusable.h"
#include "widget/requestfocus.h"
#include "widget/instancemodifier.h"
#include "widget/widget.h"

#include <bq/signal/signal.h>

#include <bq/stream/collect.h>
#include <bq/stream/stream.h>

namespace reactive::widget
{

namespace
{
    bool hasValues(std::vector<bool> const& v)
    {
        return !v.empty();
    }
} // anonymous namespace

AnyWidgetModifier focusOn(stream::Stream<bool> stream)
{
    auto focusRequest = stream::collect(std::move(stream)).map(hasValues);

    return makeWidgetModifier(makeInstanceSignalModifier(
        [](auto instance, auto focusRequest)
        {
            return std::move(instance)
                | setFocusable(signal::constant(true))
                | requestFocus(std::move(focusRequest))
                ;
        },
        std::move(focusRequest)
        ));
}
} // namespace reactive::widget
