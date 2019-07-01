#include "widget/textedit.h"

#include "widget/clip.h"
#include "widget/label.h"
#include "widget/frame.h"
#include "widget/onkeyevent.h"

#include "reactive/simplesizehint.h"

#include "signal/tween.h"
#include "signal/cache.h"
#include "signal/combine.h"

#include "stream/iterate.h"
#include "stream/pipe.h"

#include "clickevent.h"
#include "send.h"

#include "debug.h"

#include <avg/textextents.h>
#include <avg/pathbuilder.h>

#include <algorithm>

namespace reactive::widget
{

TextEdit::operator WidgetFactory() const
{
    auto draw = [](ase::Vector2f size, widget::Theme const& theme,
            TextEditState const& state, float percentage)
        -> avg::Drawing
    {
        auto height = theme.getTextHeight();
        auto const& font = theme.getFont();
        auto text = utf8::split(utf8::asUtf8(state.text),
                utf8::floor(state.text, state.pos));
        auto const te1 = font.getTextExtents(text.first, height);

        auto offset = ase::Vector2f(
                -te1.bearing[0],
                (size[1] - height) * 0.5f + font.getDescender(height));

        auto& color = theme.getPrimary();

        auto textEntry1 = avg::TextEntry(
                font,
                avg::Transform()
                    .scale(height)
                    .translate(offset),
                text.first.str(),
                btl::just(avg::Brush(color)),
                btl::none);

        auto textEntry2 = avg::TextEntry(
                font,
                avg::Transform()
                    .scale(height)
                    .translate(te1.advance + offset),
                text.second.str(),
                btl::just(avg::Brush(color)),
                btl::none);

        auto texts = avg::Drawing(textEntry1) + textEntry2;

        if (percentage > 0.0f)
        {
            auto line = avg::PathBuilder()
                    .start(ase::Vector2f(0.0f, 0.0f))
                    .lineTo(ase::Vector2f(0.0f, font.getLinegap(height)))
                    .build();

            line += te1.advance;

            texts = std::move(texts)
                + avg::Shape()
                .setPath(line)
                .setPen(btl::just(
                            avg::Pen(
                                avg::Brush(theme.getEmphasized() * percentage),
                                1.0f)
                            )
                        );
        }

        return texts;
    };

    using Events = btl::variant<KeyEvent, ClickEvent>;

    auto update = [](TextEditState state, Events const& e,
            widget::Theme const& theme,
            std::vector<std::function<void()>> const& onEnter)
        -> TextEditState
    {
        if (e.is<KeyEvent>())
        {
            auto& keyEvent = e.get<KeyEvent>();

            if (!keyEvent.isDown())
                return state;

            // Sanitize state
            state.pos = std::min(state.pos, state.text.size());

            if (keyEvent.getKey() == ase::KeyCode::left)
                state.pos = (--utf8::floor(state.text, state.pos)).index();
            else if (keyEvent.getKey() == ase::KeyCode::right)
                state.pos = (++utf8::floor(state.text, state.pos)).index();
            else if (keyEvent.getKey() == ase::KeyCode::returnKey)
            {
                for (auto&& f : onEnter)
                    f();
            }
            else if (keyEvent.hasSymbol()
                    && keyEvent.getKey() != ase::KeyCode::backSpace)
            {
                state.text.insert(state.pos, keyEvent.getText());
                state.pos += keyEvent.getText().size();
            }
            else if (state.pos != 0
                    && keyEvent.getKey() == ase::KeyCode::backSpace)
            {
                auto i = --utf8::floor(state.text, state.pos);
                utf8::erase(state.text, i);
                state.pos = i.index();
            }
        }
        else if (e.is<ClickEvent>())
        {
            auto& clickEvent = e.get<ClickEvent>();

            state.pos = theme.getFont().getCharacterIndex(
                    utf8::asUtf8(state.text),
                    theme.getTextHeight(),
                    clickEvent.getPos());
        }

        return state;
    };

    auto keyStream = stream::pipe<Events>();

    auto theme = signal::input(widget::Theme());
    auto requestFocus = stream::pipe<bool>();

    auto focus = signal::input(false);
    auto focusPercentage = signal::tween(
            std::chrono::milliseconds(500),
            0.0f,
            std::move(focus.signal),
            signal::TweenType::pingpong
            );

    auto frameColor = signal::map(&widget::Theme::getSecondary, theme.signal);

    auto oldState = signal::share(btl::clone(state_));

    auto newState = signal::tee(
            stream::iterate(update, oldState, std::move(keyStream.stream),
                std::move(theme.signal), signal::combine(onEnter_)),
            handle_ );

    return makeWidgetFactory()
        | trackTheme(theme.handle)
        | trackFocus(focus.handle)
        | onDraw<SizeTag, ThemeTag>(draw, std::move(newState),
                std::move(focusPercentage))
        | widget::margin(signal::constant(5.0f))
        | widget::clip()
        | widget::frame(std::move(frameColor))
        | focusOn(std::move(requestFocus.stream))
        | onClick(1,
                [requestHandle=requestFocus.handle, keyHandle=keyStream.handle]
                (ClickEvent const& e)
                {
                    requestHandle.push(true);
                    keyHandle.push(e);
                })
        | widget::onKeyEvent(sendKeysTo(keyStream.handle))
        | setSizeHint(signal::constant(simpleSizeHint(250.0f, 40.0f)))
        ;
}

TextEdit TextEdit::onEnter(Signal<std::function<void()>> cb) &&
{
    onEnter_.push_back(signal::share(std::move(cb)));
    return std::move(*this);
}

TextEdit TextEdit::onEnter(std::function<void()> cb) &&
{
    return std::move(*this).onEnter(signal::share(signal::constant(std::move(cb))));
}

TextEdit textEdit(signal::InputHandle<TextEditState> handle,
        Signal<TextEditState> state)
{
    return TextEdit{std::move(handle), std::move(state), {}};
}

} // widget::reactive

