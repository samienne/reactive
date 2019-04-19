#include "widget/button.h"

#include "widget/frame.h"
#include "widget/label.h"

namespace reactive::widget
{

WidgetFactory button(Signal<std::string> label,
        Signal<std::function<void()>> onClick)
{
    return widget::label(std::move(label))
                    | widget::frame()
                    | widget::onClick(0,
                            signal::cast<std::function<void()>>(
                                std::move(onClick)
                                ))
                    ;
}

WidgetFactory button(std::string label, Signal<std::function<void()>> onClick)
{
    return button(signal::constant(std::move(label)), std::move(onClick));
}

} // namespace reactive::widget

