#pragma once

#include "reactive/widgetfactory.h"
#include "reactive/sizehint.h"

#include "reactive/signal/cast.h"
#include "reactive/signal/inputhandle.h"
#include "reactive/signaltype.h"

#include <avg/font.h>

#include <string>
#include <ostream>

namespace reactive
{
    namespace widget
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

        struct TextEdit
        {
            operator WidgetFactory() const;
            TextEdit onEnter(signal2::Signal<std::function<void()>> cb) &&;
            TextEdit onEnter(std::function<void()> cb) &&;

            template <typename T, typename U>
            TextEdit onEnter(signal2::Signal<T, U> cb) &&
            {
                return std::move(*this)
                    .onEnter(signal::cast<std::function<void()>>(std::move(cb)));
            }

            signal::InputHandle<TextEditState> handle_;
            signal2::Signal<TextEditState> state_;
            std::vector<signal2::SharedSignal<std::function<void()>>> onEnter_;
        };

        TextEdit textEdit(signal::InputHandle<TextEditState> handle,
                signal2::Signal<TextEditState> state);
    }
}

