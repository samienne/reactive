#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/font.h>

#include <string>
#include <ostream>

namespace bqui::widget
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

    struct BQUI_EXPORT TextEdit
    {
        operator AnyWidget() const;
        TextEdit onEnter(bq::signal::AnySignal<std::function<void()>> cb) &&;
        TextEdit onEnter(std::function<void()> cb) &&;

        AnyWidget build() &&;

        bq::signal::InputHandle<TextEditState> handle_;
        bq::signal::AnySignal<TextEditState> state_;
        std::vector<bq::signal::AnySignal<std::function<void()>>> onEnter_;
    };

    BQUI_EXPORT TextEdit textEdit(bq::signal::InputHandle<TextEditState> handle,
            bq::signal::AnySignal<TextEditState> state);
} // namespace bqui::widget

