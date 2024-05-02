#include "widget/focuson.h"

#include "widget/setfocusable.h"
#include "widget/requestfocus.h"
#include "widget/instancemodifier.h"
#include "widget/widget.h"

#include <reactive/signal2/signal.h>

#include <reactive/stream/collect.h>
#include <reactive/stream/stream.h>

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
    auto focusRequest = stream::collect2(std::move(stream)).map(hasValues);

    return makeWidgetModifier(makeInstanceSignalModifier(
        [](auto instance, auto focusRequest)
        {
            return std::move(instance)
                | setFocusable(signal2::constant(true))
                | requestFocus(std::move(focusRequest))
                ;
        },
        std::move(focusRequest)
        ));
}
} // namespace reactive::widget
