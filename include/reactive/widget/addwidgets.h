#pragma once

#include "reduce.h"
#include "adddrawings.h"

#include "widgettransformer.h"

#include "reactive/inputarea.h"
#include "reactive/widget.h"

#include "reactive/signal/combine.h"
#include "reactive/signal/mbind.h"

#include <btl/all.h>
#include <btl/typetraits.h>
#include <btl/sequence.h>
#include <btl/cloneoncopy.h>
#include <btl/bundle.h>

#include <type_traits>

namespace reactive::widget
{
    template <typename TWidgets, std::enable_if_t<
        btl::All<
            btl::IsSequence<TWidgets>
            >::value
        , int> = 0
    >
    auto addWidgets(TWidgets widgets)
    {
        auto f = [widgets=btl::cloneOnCopy(std::move(widgets))](auto widget)
        {
            auto addAreas = [](std::vector<InputArea> own,
                    //std::vector<std::vector<InputArea>> areas
                    auto areas
                    )
                -> std::vector<InputArea>
                {
                    return reactive::detail::concat(
                            own,
                            reactive::detail::join(std::move(areas))
                            );
                };

            //std::vector<Signal<std::vector<InputArea>>> areas;
            //for (auto&& w : *widgets)
            auto areas = btl::fmap(*widgets, [](auto&& w)
            {
                return w.getInputAreas();
            });

            auto areasSignal = signal::map(
                    addAreas,
                    widget.getInputAreas(),
                    signal::combine(std::move(areas))
                    );

            //std::vector<Signal<std::vector<KeyboardInput>>> vectorOfInputs;
            //vectorOfInputs.reserve(widgets->size());
            //for (auto&& w : *widgets)
            auto tupleOfInputs = btl::fmap(*widgets, [](auto&& w)
            {
                return w.getKeyboardInputs();
            });

            auto flatten = [](auto inputs)
            {
                return reactive::detail::join(std::move(inputs));
            };

            auto inputs = signal::map(flatten,
                    signal::combine(std::move(tupleOfInputs)));

            auto addInputs = [](std::vector<KeyboardInput> lhs,
                    std::vector<KeyboardInput> rhs)
                -> std::vector<KeyboardInput>
                {
                    std::vector<KeyboardInput> result;

                    for (auto&& input : lhs)
                    {
                        if (input.isFocusable())
                            result.push_back(std::move(input));
                    }

                    for (auto&& input : rhs)
                    {
                        if (input.isFocusable())
                            result.push_back(std::move(input));
                    }

                    return result;
                };

            auto keyboardInputsSignal = signal::map(addInputs,
                    std::move(widget.getKeyboardInputs()),
                    std::move(inputs));

            //std::vector<Signal<avg::Drawing>> drawings;
            //for (auto&& w : *widgets)
                //drawings.push_back(std::move(w.getDrawing()));
            auto drawings = btl::fmap(*widgets, [](auto&& w)
            {
                return w.getDrawing();
            });

            return makeWidgetTransformerResult(makeWidget(
                    std::move(widget.getDrawContext()),
                    std::move(widget.getDrawing()),
                    std::move(areasSignal),
                    std::move(widget.getObb()),
                    std::move(keyboardInputsSignal),
                    std::move(widget.getTheme())
                    )
                | addDrawings(signal::combine(std::move(drawings)))
                );

        };

        return makeWidgetTransformer(std::move(f));
    }

    template <typename T>
    auto addWidgets(Signal<T, std::vector<Widget>> widgets)
    {
        auto f = [widgets=btl::cloneOnCopy(std::move(widgets))](auto widget)
        {
            auto w1 = signal::map([widget=std::move(widget)]
                    (std::vector<Widget> widgets)
                    {
                        return btl::clone(widget)
                            | addWidgets(std::move(widgets))
                            ;
                    },
                    btl::clone(*widgets)
                    );

            return makeWidgetTransformerResult(reduce(std::move(w1)));
        };

        return makeWidgetTransformer(std::move(f));
    }

    inline auto addWidget(Widget widget)
    {
        std::vector<Widget> widgets;
        widgets.push_back(std::move(widget));

        return addWidgets(std::move(widgets));
    }
} // namespace reactive::widget

