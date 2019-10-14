#pragma once

#include "widget/bindobb.h"
#include "widget/setkeyboardinputs.h"
#include "widget/bindkeyboardinputs.h"
#include "widget/onclick.h"
#include "widget/onhover.h"
#include "widget/onpointermove.h"
#include "widget/onpointerup.h"
#include "widget/onpointerdown.h"
#include "widget/drawbackground.h"
#include "widget/addwidgets.h"
#include "widget/adddrawings.h"
#include "widget/ondraw.h"

#include "clickevent.h"
#include "widgetmap.h"

#include <reactive/stream/collect.h>
#include <reactive/stream/stream.h>

#include <reactive/signal/map.h>
#include <reactive/signal/tee.h>
#include <reactive/signal.h>

#include <btl/sequence.h>
#include <btl/bundle.h>

namespace reactive
{
    template <typename T>
    inline auto transform(Signal<avg::Transform, T> t)
    {
        auto tt = btl::cloneOnCopy(std::move(t));

        auto f = [t=std::move(tt)](auto widget)
        {
            auto w = std::move(widget)
                .transform(t->clone())
                ;

            return widget::makeWidgetTransformResult(std::move(w));
        };

        //static_assert(IsWidgetMap<decltype(f)>::value, "");

        return widget::makeWidgetTransform(std::move(f));
    }

    template <typename TSignalWidget, typename = typename
        std::enable_if
        <
            IsSignalType<TSignalWidget, Widget>::value
        >::type>
    auto reduce(TSignalWidget w2)
    {
        auto w = signal::share(std::move(w2));

        auto drawContext = signal::mbind([](auto&& w)
                {
                    return w.getDrawContext();
                }, w);

        auto drawing = signal::mbind([](auto&& w) {
                return w.getDrawing().clone();
                }, w);

        auto areas = signal::mbind([](auto&& w) { return w.getInputAreas().clone(); }, w);
        auto obb = signal::mbind([](auto&& w) { return w.getObb().clone(); }, w);

        auto keyboardInputs = signal::mbind([](auto&& w) {
                return w.getKeyboardInputs().clone();
                }, w);

        auto theme = signal::mbind([](auto&& w) { return w.getTheme().clone(); }, w);

        return makeWidget(
                std::move(drawContext),
                std::move(drawing),
                std::move(areas),
                std::move(obb),
                std::move(keyboardInputs),
                std::move(theme)
                );
    }

    template <typename T>
    auto setFocusable(Signal<bool, T> focusable)
    {
        return widget::makeWidgetTransform()
            .provide(widget::grabKeyboardInputs())
            .values(std::move(focusable))
            .bind([](auto keyboardInputs, auto focusable)
            {
                auto newInputs = signal::map(
                    [](std::vector<KeyboardInput> inputs, bool focusable)
                    -> std::vector<KeyboardInput>
                    {
                        inputs[0] = std::move(inputs[0]).setFocusable(focusable);
                        return inputs;
                    },
                    std::move(keyboardInputs),
                    std::move(focusable)
                    );

                return widget::setKeyboardInputs(std::move(newInputs));
            });
    }

    template <typename T>
    auto requestFocus(Signal<bool, T> requestFocus)
    {
        return widget::makeWidgetTransform()
            .provide(widget::grabKeyboardInputs())
            .values(std::move(requestFocus))
            .bind([](auto keyboardInputs, auto requestFocus)
            {
                auto newInputs = signal::map(
                    [](std::vector<KeyboardInput> inputs, bool requestFocus)
                    -> std::vector<KeyboardInput>
                    {
                        if (!inputs.empty() && (!inputs[0].hasFocus() || requestFocus))
                            inputs[0] = std::move(inputs[0]).requestFocus(requestFocus);

                        return inputs;
                    },
                    std::move(keyboardInputs),
                    std::move(requestFocus)
                    );

                return widget::setKeyboardInputs(std::move(newInputs));
            });
    }

    inline auto focusOn(stream::Stream<bool> stream)
    {
        auto hasValues = [](std::vector<bool> const& v) -> bool
        {
            return !v.empty();
        };

        auto focusRequest = signal::map(
                hasValues,
                stream::collect(std::move(stream))
                );

        auto f = [focusRequest=btl::cloneOnCopy(std::move(focusRequest))]
            (auto widget)
        {
            return makeWidgetTransformResult(
                    std::move(widget)
                    | setFocusable(signal::constant(true))
                    //| requestFocus(std::move(*focusRequest))
                    | requestFocus(focusRequest->clone())
                    );
        };

        return widget::makeWidgetTransform(std::move(f));
    }

    inline auto trackSize(signal::InputHandle<ase::Vector2f> handle)
        //-> FactoryMap
    {
        auto f = [handle = std::move(handle)](auto widget) mutable
            // -> Widget
        {
            auto obb = signal::tee(
                    widget.getObb(),
                    std::mem_fn(&avg::Obb::getSize),
                    std::move(handle)
                    );

            return widget::makeWidgetTransformResult(std::move(widget)
                .setObb(std::move(obb))
                | widget::makeWidgetTransform()
                .provide(widget::bindObb(), widget::grabKeyboardInputs())
                .bind([](auto obb, auto inputs)
                {
                    auto newInputs = signal::map(
                        [](avg::Obb, std::vector<KeyboardInput> inputs)
                        {
                            // TODO: Remove this hack. This is needed to keep
                            // the obb signal in around.
                            return inputs;
                        },
                        std::move(obb),
                        std::move(inputs)
                        );

                    return widget::setKeyboardInputs(std::move(newInputs));
                }));
        };

        //static_assert(std::is_convertible<decltype(f), WidgetMap>::value, "");
        //static_assert(IsWidgetTransform<decltype(f)>::value, "");

        return widget::makeWidgetTransform(std::move(f));
    }

    inline auto trackObb(signal::InputHandle<avg::Obb> handle)
        //-> FactoryMap
    {
        auto f = [handle = std::move(handle)](auto widget)
            // -> Widget
        {
            auto obb = signal::tee(widget.getObb(), handle);

            return widget::makeWidgetTransformResult(std::move(widget)
                .setObb(std::move(obb))
                | widget::makeWidgetTransform()
                .provide(widget::bindObb(), widget::grabKeyboardInputs())
                .bind([](auto obb, auto inputs)
                {
                    auto newInputs = signal::map(
                        [](avg::Obb, std::vector<KeyboardInput> inputs)
                        {
                            // TODO: Remove this hack. This is needed to keep
                            // the obb signal in around.
                            return inputs;
                        },
                        std::move(obb),
                        std::move(inputs)
                        );

                    return widget::setKeyboardInputs(std::move(newInputs));
                }));
        };

        //static_assert(std::is_convertible<decltype(f), WidgetMap>::value, "");
        //static_assert(IsWidgetTransform<decltype(f)>::value, "");

        return widget::makeWidgetTransform(std::move(f));
    }

    inline auto trackFocus(signal::InputHandle<bool> const& handle)
        // -> FactoryMap
    {
        auto f = [handle](auto widget)
        {
            auto anyHasFocus = [](std::vector<KeyboardInput> const& inputs)
                -> bool
            {
                for (auto&& input : inputs)
                    if (input.hasFocus())
                        return true;

                return false;
            };

            auto input = signal::tee(
                    signal::share(widget.getKeyboardInputs()),
                    anyHasFocus, handle);

            return widget::makeWidgetTransformResult(
                    std::move(widget).setKeyboardInputs(std::move(input))
                    );
        };

        return widget::makeWidgetTransform(std::move(f));
    }

    inline auto trackTheme(signal::InputHandle<widget::Theme> const& handle)
        //-> FactoryMap
    {
        auto f = [handle](auto widget)
            // -> Widget
        {
            auto theme = widget.getTheme();
            return std::move(widget)
                .setTheme(signal::tee(std::move(theme), handle));
        };

        return widgetMap(f);
    }

} // namespace reactive

