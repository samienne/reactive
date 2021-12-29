#pragma once

#include "reduce.h"

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
    inline auto addWidgets(Widget w, std::vector<Widget> const& widgets)
    {
        auto container = std::make_shared<avg::ContainerNode>(
                w.getObb()
                //avg::Obb(w.getObb().getSize())
                );

        container->addChild(w.getRenderTree().getRoot());

        for (auto const& widget : widgets)
        {
            container->addChild(widget.getRenderTree().getRoot());
        }

        std::vector<InputArea> areas = w.getInputAreas();
        for (auto const& widget : widgets)
            for (auto const& area : widget.getInputAreas())
                areas.push_back(area);

        std::vector<KeyboardInput> inputs = w.getKeyboardInputs();
        for (auto const& widget : widgets)
            for (auto const& input : widget.getKeyboardInputs())
                if (input.isFocusable())
                    inputs.push_back(input);

        return std::move(w)
            .setRenderTree(avg::RenderTree(std::move(container)))
            .setInputAreas(std::move(areas))
            .setKeyboardInputs(std::move(inputs))
            ;
    }

    /*template <typename TWidgets, std::enable_if_t<
        btl::All<
            btl::IsSequence<TWidgets>
            >::value
        , int> = 0
    >*/
    inline auto addWidgets(std::vector<AnySignal<Widget>> widgets)
    {
        return makeWidgetTransformer([widgets=std::move(widgets)](auto w) mutable
        {
            auto widget = group(std::move(w), combine(std::move(widgets)))
                .map([](Widget w, std::vector<Widget> widgets)
                {
                    return addWidgets(std::move(w), std::move(widgets));
                });

            return makeWidgetTransformerResult(
                    std::move(widget)
                    );
        });

        /*
        auto f = [widgets=btl::cloneOnCopy(std::move(widgets))]
            (auto widget)
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

            auto renderTreeSignals = btl::fmap(*widgets, [](auto&& w)
            {
                return w.getRenderTree();
            });

            auto renderTrees = signal::combine(std::move(renderTreeSignals));

            auto renderTree = signal::map(
                    [](avg::RenderTree const& root,
                        std::vector<avg::RenderTree> const& trees,
                        avg::Obb const& obb)
                    {
                        auto container = std::make_shared<avg::ContainerNode>(
                                obb
                                );

                        container->addChild(root.getRoot());

                        for (auto const& tree : trees)
                            container->addChild(tree.getRoot());

                        return avg::RenderTree(std::move(container));
                    },
                    widget.getRenderTree(),
                    std::move(renderTrees),
                    widget.getObb()
                    );

            return makeWidgetTransformerResult(makeWidget(
                    std::move(renderTree),
                    std::move(areasSignal),
                    std::move(widget.getObb()),
                    std::move(keyboardInputsSignal),
                    std::move(widget.getTheme())
                    )
                );
        };

        return makeWidgetTransformer(std::move(f));
        */
        //return makeWidgetTransformer();
    }


    template <typename T>
    auto addWidgets(Signal<T, std::vector<Widget>> widgets)
    {
        auto f = [widgets=btl::cloneOnCopy(std::move(widgets))](auto widget)
        {
            auto w1 = signal::map([]
                    (Widget w, std::vector<Widget> widgets)
                    {
                        /*
                        return btl::clone(widget)
                            | addWidgets(std::move(widgets))
                            ;
                            */

                        return addWidgets(std::move(w), std::move(widgets));

                    },
                    std::move(widget),
                    btl::clone(*widgets)
                    );

            return makeWidgetTransformerResult(std::move(w1));
        };

        return makeWidgetTransformer(std::move(f));
    }

    inline auto addWidget(AnySignal<Widget> widget)
    {
        std::vector<AnySignal<Widget>> widgets;
        widgets.push_back(std::move(widget));

        return addWidgets(std::move(widgets));
    }
} // namespace reactive::widget

