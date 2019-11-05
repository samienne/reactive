#include "widget/scrollbar.h"

#include "widget/margin.h"
#include "widget/onpointerdown.h"
#include "widget/onpointerup.h"
#include "widget/onpointermove.h"
#include "widget/ondraw.h"
#include "widget/bindtheme.h"
#include "widget/binddrawcontext.h"
#include "widget/bindsize.h"
#include "widget/bindhover.h"
#include "widget/widgettransformer.h"

#include "reactive/shapes.h"
#include "reactive/simplesizehint.h"

#include "send.h"

#include <avg/pathbuilder.h>

namespace reactive::widget
{

namespace
{
    template <bool IsHorizontal>
    avg::Rect getSliderRect(avg::Vector2f size, float amount,
            float handleSize)
    {
        int x = IsHorizontal ? 0 : 1;
        int y = IsHorizontal ? 1 : 0;

        float handleLen = std::max(10.0f, size[x] * handleSize);
        float coords[2] = {
            (size[x] - handleLen) * amount,
            size[y] / 2.0f - 10.0f
        };

        float sizes[2] = { handleLen, 20.0f };

        return avg::Rect(
                avg::Vector2f(coords[x], coords[y]),
                avg::Vector2f(sizes[x], sizes[y])
                );
    }

    avg::Path rectToPath(pmr::memory_resource* memory, avg::Rect const& r)
    {
        return makePathFromRect(memory, r, 10.0f);
    }

    template <bool IsHorizontal>
    avg::Drawing drawSlider(
            DrawContext const& drawContext, avg::Vector2f size,
            widget::Theme const& theme, float amount, float handleSize,
            bool hover, bool isDown)
    {
        avg::Color bgColor = theme.getBackground();
        avg::Color fgColor = theme.getSecondary();

        if (isDown)
        {
            fgColor = theme.getEmphasized();
            bgColor = theme.getSecondary();
        }
        else if (hover)
        {
            bgColor = theme.getBackgroundHighlight();
        }

        avg::Pen pen(fgColor, 1.0f);
        avg::Brush brush(bgColor);

        avg::Path slider(rectToPath(
                    drawContext.getResource(),
                    getSliderRect<IsHorizontal>(size, amount, handleSize)
                    ));

        return drawContext.drawing(
            makeShape(std::move(slider), btl::just(brush), btl::just(pen))
            );
    }

    template <bool IsHorizontal>
    avg::Path getScrollLinePath(DrawContext const& drawContext, avg::Vector2f size)
    {

        if (IsHorizontal)
        {
            return drawContext.pathBuilder()
                .start(0.0f, size[1] / 2.0f)
                .lineTo(size[0], size[1] / 2.0f)
                .build();
        }
        else
        {
            return drawContext.pathBuilder()
                .start(size[0] / 2.0f, 0.0f)
                .lineTo(size[0] / 2.0f, size[1])
                .build();
        }
    }

    template <bool IsHorizontal>
    avg::Drawing drawScrollBar(DrawContext const& drawContext, avg::Vector2f size,
            widget::Theme const& theme, float amount, float handleSize,
            bool hover, bool isDown)
    {
        avg::Path line = getScrollLinePath<IsHorizontal>(drawContext, size);

        avg::Pen pen(theme.getBackgroundHighlight(), 1.0f);

        return drawContext.drawing()
            + makeShape(std::move(line), btl::none, btl::just(pen))
            + drawSlider<IsHorizontal>(drawContext, size, theme, amount,
                    handleSize, hover, isDown)
            ;
    }

    template <bool IsHorizontal, typename T, typename U, typename V>
    auto scrollPointerDown(
            signal::InputHandle<btl::option<avg::Vector2f>> downHandle,
            Signal<avg::Vector2f, T> sizeSignal,
            SharedSignal<float, U> amountSignal,
            Signal<float, V> handleSizeSignal)
    {
        return signal::mapFunction(
            [downHandle=std::move(downHandle)]
            (avg::Vector2f size, float amount, float handleSize,
                PointerButtonEvent e) mutable
            {
                auto r = getSliderRect<IsHorizontal>(size, amount,
                        handleSize);

                if (e.button == 1 && r.contains(e.pos))
                    downHandle.set(btl::just(e.pos-r.getCenter()));

                return EventResult::possible;
            },
            std::move(sizeSignal),
            amountSignal,
            std::move(handleSizeSignal)
            );
    }

    template <bool IsHorizontal>
    auto getScrollBarSizeHint()
    {
        std::array<float, 3> main{{50, 100, 10000}};
        std::array<float, 3> aux{{25, 25, 25}};

        if (IsHorizontal)
            return signal::constant(simpleSizeHint(main, aux));
        else
            return signal::constant(simpleSizeHint(aux, main));
    }

    template <bool IsHorizontal>
    auto bindSliderObb(
            SharedSignal<float> amount,
            SharedSignal<float> handleSize)
    {
        return makeWidgetTransformer()
            .compose(bindSize())
            .bind([=](auto size)
            {
                auto obb = signal::map([]
                        (avg::Vector2f size, float amount, float handleSize)
                        {
                            return avg::Obb(getSliderRect<IsHorizontal>(
                                        size,
                                        amount,
                                        handleSize));
                        }, size.clone(), amount, handleSize);

                return bindData(std::move(obb));
            });
    }

    template <bool IsHorizontal>
    auto bindHoverOnSlider(
            SharedSignal<float> amount,
            SharedSignal<float> handleSize)
    {
        return makeWidgetTransformer()
            .compose(bindSliderObb<IsHorizontal>(amount, handleSize))
            .bind([=](auto obb)
            {
                return bindHover(std::move(obb));
            });
    }

    template <bool IsHorizontal>
    auto makeScrollBar(
        signal::InputHandle<float> scrollHandle,
        SharedSignal<float> amount,
        SharedSignal<float> handleSize)
    {
        return makeWidgetTransformer()
            .compose(bindDrawContext(), bindSize(), bindTheme())
            .compose(bindHoverOnSlider<IsHorizontal>(amount, handleSize))
            .bind([=](auto drawContext, auto size,
                            auto theme, auto hover) mutable
            {
                auto downOffset = signal::input<btl::option<avg::Vector2f>>(btl::none);
                auto isDown = signal::map(&btl::option<avg::Vector2f>::valid,
                        downOffset.signal);

                return makeWidgetTransformer()
                    .compose(onPointerDown(scrollPointerDown<IsHorizontal>(
                            downOffset.handle, size.clone(), amount, handleSize)
                        ))
                    .compose(onPointerUp([handle=downOffset.handle]() mutable
                        {
                            handle.set(btl::none);
                            return EventResult::accept;
                        }))
                    .compose(onPointerMove(signal::mapFunction(
                        [scrollHandle]
                        (btl::option<avg::Vector2f> downOffset,
                            avg::Vector2f size, float handleSize,
                            PointerMoveEvent const& e) mutable -> EventResult
                        {
                            if (!downOffset.valid())
                                return EventResult::possible;

                            if (handleSize == 1.0f)
                                return EventResult::possible;

                            int const axis = IsHorizontal ? 0 : 1;
                            float offset = (*downOffset)[axis];
                            float handleLen  = std::max(10.0f, handleSize * size[axis]);
                            float lineLen = size[axis] - handleLen;

                            float pos = e.pos[axis] - offset - handleLen / 2.0f;

                            float len = pos / lineLen;

                            scrollHandle.set(std::max(0.0f, std::min(len, 1.0f)));
                            return EventResult::accept;
                        }, downOffset.signal, size.clone(), handleSize))
                    )
                    .values(
                            std::move(drawContext),
                            size.clone(),
                            std::move(theme),
                            amount,
                            handleSize,
                            std::move(hover),
                            std::move(isDown)
                            )
                    .bind(onDraw(drawScrollBar<IsHorizontal>))
                    ;
            });
    }
} // anonymous namespace

template <bool IsHorizontal>
WidgetFactory scrollBar(
        signal::InputHandle<float> scrollHandle,
        SharedSignal<float> amount,
        SharedSignal<float> handleSize)
{
    return makeWidgetFactory()
        | makeScrollBar<IsHorizontal>(scrollHandle, amount, handleSize)
        | widget::margin(signal::constant(5.0f))
        | setSizeHint(getScrollBarSizeHint<IsHorizontal>())
        ;
}

WidgetFactory hScrollBar(
        signal::InputHandle<float> handle,
        Signal<float> amount,
        Signal<float> handleSize)
{
    return scrollBar<true>(std::move(handle), signal::share(std::move(amount)),
            signal::share(signal::map([](float v)
                    {
                        return std::min(1.0f, v);
                    }, std::move(handleSize))));
}

WidgetFactory vScrollBar(
        signal::InputHandle<float> handle,
        Signal<float> amount,
        Signal<float> handleSize)
{
    return scrollBar<false>(std::move(handle), signal::share(std::move(amount)),
            signal::share(signal::map([](float v)
                    {
                        return std::min(1.0f, v);
                    }, std::move(handleSize))));
}

} // namespace reactive::widget

