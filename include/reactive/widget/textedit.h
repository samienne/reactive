#pragma once

#include "widget.h"

#include "reactive/sizehint.h"
#include "reactive/reactivevisibility.h"

#include "reactive/signal/erasetype.h"
#include "reactive/signal/cast.h"
#include "reactive/signal/inputhandle.h"
#include "reactive/signal/signal.h"

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

        inline bool operator==(TextEditState const& rhs) const
        {
            return text == rhs.text && pos == rhs.pos;
        }

        inline bool operator!=(TextEditState const& rhs) const
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
        operator AnyWidget() const;
        TextEdit onEnter(AnySignal<std::function<void()>> cb) &&;
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

        AnyWidget build() &&;

        signal::InputHandle<TextEditState> handle_;
        AnySignal<TextEditState> state_;
        std::vector<AnySharedSignal<std::function<void()>>> onEnter_;
    };

    REACTIVE_EXPORT TextEdit textEdit(signal::InputHandle<TextEditState> handle,
            AnySignal<TextEditState> state);
} // namespace reactive::widget

