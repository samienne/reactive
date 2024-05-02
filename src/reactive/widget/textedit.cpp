#include "widget/textedit.h"

#include "widget/providetheme.h"
#include "widget/margin.h"
#include "widget/ondraw.h"
#include "widget/clip.h"
#include "widget/frame.h"
#include "widget/trackfocus.h"
#include "widget/focuson.h"
#include "widget/onkeyevent.h"
#include "widget/ontextevent.h"
#include "widget/onclick.h"
#include "widget/setsizehint.h"

#include "reactive/simplesizehint.h"

#include "reactive/signal2/combine.h"

#include "stream/iterate.h"
#include "stream/pipe.h"

#include "clickevent.h"
#include "send.h"


#include <avg/curve/curves.h>
#include <avg/textextents.h>
#include <avg/pathbuilder.h>

#include <algorithm>
#include <variant>

namespace reactive::widget
{

namespace
{
    auto drawTextEdit(
            avg::DrawContext const& drawContext,
            ase::Vector2f size,
            widget::Theme const& theme,
            TextEditState const& state,
            float percentage
            )
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
                std::make_optional(avg::Brush(color)),
                std::nullopt);

        auto textEntry2 = avg::TextEntry(
                font,
                avg::Transform()
                    .scale(height)
                    .translate(te1.advance + offset),
                text.second.str(),
                std::make_optional(avg::Brush(color)),
                std::nullopt);

        avg::Drawing texts = drawContext.drawing(textEntry1) + textEntry2;

        if (percentage > 0.0f)
        {
            auto line = drawContext.pathBuilder()
                    .start(ase::Vector2f(0.0f, 0.0f))
                    .lineTo(ase::Vector2f(0.0f, font.getLinegap(height)))
                    .build();

            line += te1.advance;

            texts = std::move(texts)
                + avg::Shape(line)
                .stroke(avg::Pen(
                            avg::Brush(theme.getEmphasized() * percentage),
                            1.0f)
                       )
                ;
        }

        return texts;
    }

    using Events = std::variant<KeyEvent, TextEvent, ClickEvent>;

    auto updateTextEdit(TextEditState state, Events const& e,
            widget::Theme const& theme,
            std::vector<std::function<void()>> const& onEnter)
    {
        if (std::holds_alternative<KeyEvent>(e))
        {
            auto& keyEvent = std::get<KeyEvent>(e);

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
        else if (std::holds_alternative<TextEvent>(e))
        {
            auto& textEvent = std::get<TextEvent>(e);
            state.text.insert(state.pos, textEvent.getText());
            state.pos += textEvent.getText().size();
        }
        else if (std::holds_alternative<ClickEvent>(e))
        {
            auto& clickEvent = std::get<ClickEvent>(e);

            state.pos = theme.getFont().getCharacterIndex(
                    utf8::asUtf8(state.text),
                    theme.getTextHeight(),
                    clickEvent.getPos());
        }

        return state;
    }

    auto makeTextEdit(
            signal2::AnySignal<widget::Theme> theme,
            signal2::InputHandle<TextEditState> handle,
            std::vector<signal2::AnySignal<std::function<void()>>> const& onEnter,
            signal2::AnySignal<TextEditState> oldState)
    {
        auto keyStream = stream::pipe<Events>();

        auto requestFocus = stream::pipe<bool>();

        auto focus = signal2::makeInput(false);

        auto focusPercentage = std::move(focus.signal)
            .map([](bool b) -> avg::Animated<float>
                {
                    return b
                        ? avg::infiniteAnimation(0.0f, 1.0f, avg::curve::linear, 0.5f,
                                avg::RepeatMode::reverse)
                        : avg::Animated<float>(0.0f)
                        ;
                });

        auto frameColor = theme.map([](auto const& theme)
                {
                    return theme.getSecondary();
                });

        auto newState = stream::iterate(
                updateTextEdit,
                oldState,
                std::move(keyStream.stream),
                theme,
                signal2::combine(onEnter)
                ).tee(handle);

        return makeWidget()
            | trackFocus(focus.handle)
            | widget::onDraw(
                    std::move(drawTextEdit),
                    theme,
                    std::move(newState),
                    std::move(focusPercentage)
                    )
            | widget::margin(signal2::constant(5.0f))
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
            | widget::onTextEvent(sendKeysTo(keyStream.handle))
            | setSizeHint(signal2::constant(simpleSizeHint(250.0f, 40.0f)))
            ;
    }

} // anonymous namespace

TextEdit::operator AnyWidget() const
{
    return makeWidget(
            makeTextEdit,
            provideTheme(),
            handle_,
            onEnter_,
            state_.share()
            );
}

TextEdit TextEdit::onEnter(signal2::AnySignal<std::function<void()>> cb) &&
{
    onEnter_.push_back(std::move(cb).share());
    return std::move(*this);
}

TextEdit TextEdit::onEnter(std::function<void()> cb) &&
{
    return std::move(*this).onEnter(signal2::constant(std::move(cb)));
}

AnyWidget TextEdit::build() &&
{
    return *this;
}

TextEdit textEdit(signal2::InputHandle<TextEditState> handle,
        signal2::AnySignal<TextEditState> state)
{
    return TextEdit{std::move(handle), std::move(state), {}};
}

} // widget::reactive

