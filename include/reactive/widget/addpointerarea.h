#pragma once

#include "share.h"

#include "reactive/signal/cast.h"

#include "reactive/eventresult.h"
#include "reactive/widgetfactory.h"

#include <btl/compose.h>

#include <tuple>

namespace reactive::widget
{
    namespace detail
    {
        template <typename... TTags, typename TWidget, typename TFunc,
                 typename... TSigs>
        auto mapFunctionFromWidget(TWidget w, std::tuple<TSigs...> sigs,
                TFunc&& f)
        {
            auto w2 = share<TTags...>(std::move(w));

            return btl::apply(
                    [](auto f, auto... sigs)
                    {
                        static_assert(btl::All<IsSignal<decltype(sigs)>...>::value, "");
                        return signal::mapFunction(
                                std::move(f),
                                std::move(sigs)...
                                );
                    },
                    std::tuple_cat(
                        std::forward_as_tuple(
                            std::move(f),
                            get<TTags>(w2)...
                            ),
                        std::move(sigs)
                        )
                    );
        }

    } // namespace detail

    template <typename... TTags, typename TFunc, typename... TSigs>
    auto mapFunctionWithTags(TFunc&& f, TSigs... sigs)
    {
        return [sigs=std::make_tuple(std::move(sigs)...),
                f=std::forward<TFunc>(f)](auto w)
        {
            auto w2 = share<TTags...>(std::move(w));

            auto cb = detail::mapFunctionFromWidget<TTags...>(
                    w2, std::move(sigs), std::move(f));

            return make_tuple(std::move(w2), std::move(cb));
        };
    }

    class PointerAreaBuilder
    {
    public:
        using OnButtonFunction = std::function<EventResult(PointerButtonEvent const&)>;
        using OnMoveFunction = std::function<EventResult(PointerMoveEvent const&)>;

        PointerAreaBuilder()
        {
        }

        Widget operator()(Widget w)
        {
            btl::UniqueId id = btl::makeUniqueId();

            auto w2 = makeWidgetMap<InputAreaTag, ObbTag>(
                    [id](std::vector<InputArea> areas, avg::Obb const& obb)
                    mutable
                    {
                        areas.emplace_back(makeInputArea(id, obb));

                        return areas;
                    })(std::move(w));

            return std::move(w2)
                | (*onDown_)
                | (*onUp_)
                | (*onMove_)
                ;
        }

        template <typename... TTags, typename TFunc, typename... Us>
        PointerAreaBuilder onDown(TFunc f, Us... us) &&
        {
            onDown_ = btl::just([f=std::move(f),
                    sigs=btl::cloneOnCopy(
                        std::make_tuple(std::move(us)...)
                        )](auto w) mutable
            {
                auto w2 = share<TTags...>(std::move(w));

                auto cb = signal::cast<OnButtonFunction>(
                        detail::mapFunctionFromWidget<TTags...>(
                            w2,
                            std::move(*sigs),
                            std::move(f)
                            )
                        );

                return std::move(w2)
                    | makeWidgetMap<InputAreaTag>(
                            [](std::vector<InputArea> areas, auto&& cb) mutable
                            {
                                areas.back() =
                                    std::move(areas.back())
                                        .onDown(std::move(cb));

                                return areas;
                            },
                            std::move(cb));
            });

            return std::move(*this);
        }

        template <typename... TTags, typename T, typename... Us>
        PointerAreaBuilder onUp(T f, Us... us) &&
        {
            onUp_ = btl::just([f=std::move(f),
                    sigs=btl::cloneOnCopy(
                        std::make_tuple(std::move(us)...)
                        )](auto w) mutable
            {
                auto w2 = share<TTags...>(std::move(w));

                auto cb = signal::cast<OnButtonFunction>(
                        detail::mapFunctionFromWidget<TTags...>(
                            w2,
                            std::move(*sigs),
                            std::move(f)
                            )
                        );

                return std::move(w2)
                    | makeWidgetMap<InputAreaTag>(
                            [](std::vector<InputArea> areas, auto&& cb) mutable
                            {
                                areas.back() =
                                    std::move(areas.back())
                                        .onUp(std::move(cb));

                                return areas;
                            },
                            std::move(cb));
            });

            return std::move(*this);
        }

        template <typename... TTags, typename T, typename... Us>
        PointerAreaBuilder onButton(T f, Us... us) &&
        {
            return std::move(*this)
                .onDown<TTags...>(std::move(f), std::move(us)...)
                .template onUp<TTags...>(std::move(f), std::move(us)...)
                ;
        }

        template <typename... TTags, typename T, typename... Us>
        PointerAreaBuilder onMove(T f, Us... us) &&
        {
            onMove_ = btl::just([f=std::move(f),
                    sigs=btl::cloneOnCopy(
                        std::make_tuple(std::move(us)...)
                        )](auto w) mutable
            {
                auto w2 = share<TTags...>(std::move(w));

                auto cb = signal::cast<OnMoveFunction>(
                        detail::mapFunctionFromWidget<TTags...>(
                            w2,
                            std::move(*sigs),
                            std::move(f)
                            )
                        );

                return std::move(w2)
                    | makeWidgetMap<InputAreaTag>(
                            [](std::vector<InputArea> areas, auto&& cb) mutable
                            {
                                areas.back() =
                                    std::move(areas.back())
                                        .onMove(std::move(cb));

                                return areas;
                            },
                            std::move(cb));
            });

            return std::move(*this);
        }

    private:
        btl::option<WidgetMap> onDown_;
        btl::option<WidgetMap> onUp_;
        btl::option<WidgetMap> onMove_;
        btl::option<WidgetMap> onHover_;
    };

    inline auto addPointerArea()
    {
        return PointerAreaBuilder();
    }
} // namespace reactive::widget
