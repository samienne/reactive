#include "adder.h"
#include "spinner.h"

#include <reactive/widget/frame.h>
#include <reactive/widget/label.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/focusgroup.h>
#include <reactive/filler.h>
#include <reactive/vbox.h>
#include <reactive/hbox.h>

#include <reactive/signal/onchange.h>

#include <reactive/signal/tween.h>
#include <reactive/signal/simpledelegate.h>
#include <reactive/signal/databind.h>

#include <btl/collection.h>

using namespace reactive;

reactive::WidgetFactory adder()
{
    using namespace reactive;

    btl::collection<std::string> collection;

    auto delegate = [collection](Signal<std::string> data,
            signal::IndexSignal index) mutable
        -> WidgetFactory
    {
        auto size = signal::input(ase::Vector2f(100, 100));

        auto sizeStr = signal::map([](ase::Vector2f size,
                    std::string const& data) -> std::string
                {
                    return data + "(" + std::to_string(size[0]) + ", "
                        + std::to_string(size[1]) + ")";
                }, size.signal, data);

        auto remove = [collection](size_t index, ClickEvent const&) mutable
        {
            auto w = collection.writer();
            w.erase(w.begin() + index);
        };

        auto textState = signal::input(widget::TextEditState(""));

        return hbox({
                //widget::label(sizeStr),
                widget::textEdit(textState.handle, std::move(sizeStr)),
                //makeSpinner()
                hfiller(),
                widget::label(signal::constant("X"))
                | widget::frame()
                | onClick(1, signal::mapFunction(remove, index)),
                })
                | trackSize(size.handle)
                //| widget::focusGroup()
                ;
    };

    {
        auto w = collection.writer();
        w.push_back("Test item");
        w.push_back("Test item");
    }

    auto items = vbox(
            signal::dataBind(
                collection,
                signal::simpleDelegate(std::move(delegate))
                )
            );

    auto textData = signal::input(widget::TextEditState("Test"));
    auto textHandle = textData.handle;
    auto text = makeSignal(signal::share(textData.signal));

    auto doInsert = [collection, textHandle](
            widget::TextEditState const& textEdit) mutable
    {
        auto w = collection.writer();
        w.push_back(textEdit.text);
        textHandle.set(widget::TextEditState(""));
    };

    auto inserter = signal::share(
            signal::mapFunction(doInsert, text)
            );

    auto textEdit = widget::textEdit(textData.handle, text)
        .onEnter(inserter);

    auto button = widget::label(signal::constant("Add"))
        | onClick(1, inserter);

    return vbox({
            std::move(items) | widget::focusGroup(),
            hbox({std::move(textEdit), std::move(button)})
            //| widget::focusGroup()
            })
            | widget::focusGroup()
            ;
}

