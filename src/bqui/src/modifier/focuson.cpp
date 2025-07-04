#include "bqui/modifier/focuson.h"

#include "bqui/modifier/setfocusable.h"
#include "bqui/modifier/requestfocus.h"
#include "bqui/modifier/instancemodifier.h"

#include <bq/signal/signal.h>

#include <bq/stream/collect.h>
#include <bq/stream/stream.h>

namespace bqui::modifier
{

namespace
{
    bool hasValues(std::vector<bool> const& v)
    {
        return !v.empty();
    }
} // anonymous namespace

AnyWidgetModifier focusOn(bq::stream::Stream<bool> stream)
{
    auto focusRequest = bq::stream::collect(std::move(stream)).map(hasValues);

    return makeWidgetModifier(makeInstanceSignalModifier(
        [](auto instance, auto focusRequest)
        {
            return std::move(instance)
                | setFocusable(bq::signal::constant(true))
                | requestFocus(std::move(focusRequest))
                ;
        },
        std::move(focusRequest)
        ));
}
} // namespace reactive::widget
