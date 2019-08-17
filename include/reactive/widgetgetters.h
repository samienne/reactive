#pragma once

#include "drawcontext.h"
#include "widgettraits.h"
#include "widget.h"
#include "inputarea.h"

#include <avg/drawing.h>

#include <vector>

namespace reactive
{
    struct DrawContextTag { using Type = DrawContext; };
    struct DrawingTag { using Type = avg::Drawing; };
    struct SizeTag { using Type = avg::Vector2f; };
    struct ObbTag { using Type = avg::Obb; };
    struct InputAreaTag { using Type = std::vector<reactive::InputArea>; };
    struct ThemeTag { using Type = widget::Theme; };
    struct KeyboardInputTag { using Type = std::vector<KeyboardInput>; };

    template <typename T>
    struct WidgetGetter
    {
        template <typename TWidget>
        void get()
        {
        }
    };

    template <>
    struct WidgetGetter<DrawContext>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetDrawContextType<TWidget>
        {
            return std::forward<TWidget>(widget).getDrawContext();
        }
    };

    template <>
    struct WidgetGetter<DrawContextTag>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetDrawContextType<TWidget>
        {
            return std::forward<TWidget>(widget).getDrawContext();
        }
    };

    template <>
    struct WidgetGetter<avg::Drawing>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetDrawingType<TWidget>
        {
            return std::forward<TWidget>(widget).getDrawing();
        }
    };

    template <>
    struct WidgetGetter<DrawingTag>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetDrawingType<TWidget>
        {
            return std::forward<TWidget>(widget).getDrawing();
        }
    };

    template <>
    struct WidgetGetter<std::vector<InputArea>>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetAreasType<TWidget>
        {
            return std::forward<TWidget>(widget).getInputAreas();
        }
    };

    template <>
    struct WidgetGetter<InputAreaTag>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetAreasType<TWidget>
        {
            return std::forward<TWidget>(widget).getInputAreas();
        }
    };

    template <>
    struct WidgetGetter<avg::Obb>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetObbType<TWidget>
        {
            return std::forward<TWidget>(widget).getObb();
        }
    };

    template <>
    struct WidgetGetter<ObbTag>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetObbType<TWidget>
        {
            return std::forward<TWidget>(widget).getObb();
        }
    };

    template <>
    struct WidgetGetter<ase::Vector2f>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> std::decay_t<WidgetSizeType<TWidget>>
        {
            return btl::clone(std::forward<TWidget>(widget).getSize());
        }
    };

    template <>
    struct WidgetGetter<SizeTag>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> std::decay_t<WidgetSizeType<TWidget>>
        {
            return btl::clone(std::forward<TWidget>(widget).getSize());
        }
    };

    template <>
    struct WidgetGetter<std::vector<KeyboardInput>>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetKeyboardInputsType<TWidget>
        {
            return std::forward<TWidget>(widget).getKeyboardInputs();
        }
    };

    template <>
    struct WidgetGetter<KeyboardInputTag>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetKeyboardInputsType<TWidget>
        {
            return std::forward<TWidget>(widget).getKeyboardInputs();
        }
    };

    template <>
    struct WidgetGetter<widget::Theme>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetThemeType<TWidget>
        {
            return std::forward<TWidget>(widget).getTheme();
        }
    };

    template <>
    struct WidgetGetter<ThemeTag>
    {
        template <typename TWidget>
        auto get(TWidget&& widget) -> WidgetThemeType<TWidget>
        {
            return std::forward<TWidget>(widget).getTheme();
        }
    };

    template <typename T, typename TWidget>
    auto get(TWidget&& widget)
        //-> decltype(WidgetGetter<T>().get(std::forward<TWidget>(widget)))
    {
        return btl::clone(WidgetGetter<T>().get(std::forward<TWidget>(widget)));
    }

    template <typename T>
    struct Dependent { using type = T; };

    template <>
    struct Dependent<avg::Vector2f> { using type = avg::Obb; };

    template <>
    struct Dependent<SizeTag> { using type = avg::Obb; };

    template <typename T>
    using DependentType = typename Dependent<T>::type;
} // reactive

