#pragma once

#include "reactive/widgetfactory.h"
#include "reactive/sizehint.h"

#include "reactive/signal/erasetype.h"
#include "reactive/signal/cast.h"
#include "reactive/signal/inputhandle.h"
#include "reactive/signal.h"
#include "reactive/reactivevisibility.h"

#include <avg/font.h>

#include <string>
#include <ostream>

namespace reactive::widget
{
    struct TextEditState
    {
        inline TextEditState(std::string str) :
            text(std::move(str)),
            pos(text.size())
        {
        }

        inline bool operator==(TextEditState const& rhs)
        {
            return text == rhs.text && pos == rhs.pos;
        }

        inline bool operator!=(TextEditState const& rhs)
        {
            return !(*this == rhs);
        }

        std::string text;
        size_t pos = 0;
    };

    inline std::ostream& operator<<(std::ostream& stream,
            TextEditState const& s)
    {
        return stream << s.text << std::endl;
    }

    struct REACTIVE_EXPORT TextEdit
    {
        operator WidgetFactory() const;
        TextEdit onEnter(Signal<std::function<void()>> cb) &&;
        TextEdit onEnter(std::function<void()> cb) &&;

        template <typename T, typename U>
        TextEdit onEnter(Signal<T, U> cb) &&
        {
            return std::move(*this)
                .onEnter(signal::eraseType(
                            signal::cast<std::function<void()>>(std::move(cb))
                            ))
                ;
        }

        signal::InputHandle<TextEditState> handle_;
        Signal<TextEditState> state_;
        std::vector<SharedSignal<std::function<void()>>> onEnter_;
    };

    REACTIVE_EXPORT TextEdit textEdit(signal::InputHandle<TextEditState> handle,
            Signal<TextEditState> state);
} // namespace reactive::widget

