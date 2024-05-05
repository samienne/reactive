#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal2/signal.h"

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
        TextEdit onEnter(signal2::AnySignal<std::function<void()>> cb) &&;
        TextEdit onEnter(std::function<void()> cb) &&;

        AnyWidget build() &&;

        signal2::InputHandle<TextEditState> handle_;
        signal2::AnySignal<TextEditState> state_;
        std::vector<signal2::AnySignal<std::function<void()>>> onEnter_;
    };

    REACTIVE_EXPORT TextEdit textEdit(signal2::InputHandle<TextEditState> handle,
            signal2::AnySignal<TextEditState> state);
} // namespace reactive::widget

