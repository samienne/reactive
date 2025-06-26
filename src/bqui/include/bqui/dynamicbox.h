#pragma once

#include "bqui/provider/providebuildparams.h"

#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/instancemodifier.h"
#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/setid.h"

#include "bqui/widget/builder.h"
#include "bqui/widget/widget.h"
#include "bqui/widget/box.h"

#include <bq/signal/combine.h>
#include <bq/signal/signal.h>

#include <avg/rendertree.h>

namespace bqui
{
    template <Axis dir>
    widget::AnyWidget dynamicBox(
            bq::signal::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>> widgets)
    {
        return widget::makeWidget([](BuildParams const& params, auto widgets)
            {
                auto builders = widgets.map([params]
                        (std::vector<std::pair<size_t, widget::AnyWidget>> widgets)
                        {
                            std::vector<std::pair<size_t, widget::AnyBuilder>> result;

                            for (auto&& widget : widgets)
                            {
                                auto builder = std::move(widget.second)(params);
                                result.push_back({widget.first, std::move(builder)});
                            }

                            return result;
                        }).share();

                auto hintsWithoutId = builders.map([]
                        (std::vector<std::pair<size_t, widget::AnyBuilder>> const& builders)
                    {
                            std::vector<bq::signal::AnySignal<SizeHint>>
                            hintSignals;
                            for (auto const& builder : builders)
                            {
                                hintSignals.push_back(builder.second.getSizeHint());
                            }

                            return combine(std::move(hintSignals));
                    }).join().share();

                auto hints = builders.map([]
                        (std::vector<std::pair<size_t, widget::AnyBuilder>> const& builders)
                    {
                            std::vector<bq::signal::AnySignal<std::pair<size_t, SizeHint>>>
                            hintSignals;
                            for (auto const& builder : builders)
                            {
                                hintSignals.push_back(
                                        builder.second.getSizeHint()
                                            .map([id=builder.first](SizeHint hint)
                                                {
                                                    return std::make_pair(id, hint);
                                                })
                                        );
                            }

                            return combine(std::move(hintSignals));
                    }).join().share();

                auto resultHint = hintsWithoutId.map(
                        [](std::vector<SizeHint> const& hints) -> SizeHint
                        {
                            return widget::accumulateSizeHints<dir>(hints);
                        });

                return makeWidgetWithSize([](auto size, auto hintsWithoutId, auto builders)
                    {
                        auto obbs = merge(std::move(size), hintsWithoutId)
                            .map(&widget::mapObbs<dir>)
                            .merge(builders)
                            .map([](std::vector<avg::Obb> const& obbs,
                                std::vector<std::pair<size_t,
                                    widget::AnyBuilder>> const& builders)
                                {
                                    assert(obbs.size() == builders.size());

                                    std::vector<std::pair<size_t, avg::Obb>> result;
                                    for (size_t i = 0; i < builders.size(); ++i)
                                    {
                                        size_t id = builders[i].first;
                                        auto const& obb = obbs[i];

                                        result.push_back({ id, obb });
                                    }

                                    return result;
                                })
                            .share();

                        auto elements = builders.withPrevious(
                            [obbs](auto previous, auto builders)
                            {
                                std::vector<std::pair<size_t, widget::AnyElement>> result;

                                for (auto&& builder : builders)
                                {
                                    size_t id = builder.first;

                                    bool found = false;
                                    for (auto&& prev : previous)
                                    {
                                        if (prev.first == id)
                                        {
                                            result.push_back(std::move(prev));
                                            found = true;
                                        }
                                    }

                                    if (found)
                                        continue;

                                    auto obb = obbs.map(
                                        [id](std::vector<std::pair<size_t,
                                            avg::Obb>> const& obbs)
                                        {
                                            for (auto const& obb : obbs)
                                            {
                                                if (obb.first == id)
                                                    return obb.second;
                                            }

                                            assert(false);
                                            return avg::Obb();
                                        })
                                        .check()
                                        .share();

                                    auto size = obb.map(&avg::Obb::getSize);
                                    auto transform = obb.map(&avg::Obb::getTransform);

                                    auto element = std::move(builder.second)
                                            (std::move(size))
                                            | modifier::setElementId(
                                                    bq::signal::constant(avg::UniqueId())
                                                    )
                                            | modifier::transformBuilder(transform)
                                            ;

                                    result.push_back({ id, std::move(element) });
                                }

                                return result;
                            },
                            std::vector<std::pair<size_t, widget::AnyElement>>()
                            );

                        auto instances = std::move(elements).map(
                            [](std::vector<std::pair<size_t, widget::AnyElement>>
                                elements)
                            {
                                std::vector<bq::signal::AnySignal<widget::Instance>> result;

                                for (auto&& element : elements)
                                {
                                    result.push_back(
                                            std::move(element.second).getInstance());
                                }

                                return combine(result);
                            }).join();

                        return widget::makeWidget()
                            | modifier::addWidgets(std::move(instances))
                            ;
                    },
                    hintsWithoutId,
                    builders
                    )
                    | modifier::setSizeHint(std::move(resultHint))
                    ;
            },
                provider::provideBuildParams(),
                std::move(widgets).share()
                )
            ;
    }
}
