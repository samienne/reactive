#include "inputarea.h"

#include "pointerbuttonevent.h"

namespace reactive
{

InputArea::InputArea(avg::Obb const& obb) :
    obbs_({obb})
{
}

InputArea::InputArea(std::vector<avg::Obb>&& obbs) :
    obbs_(std::move(obbs))
{
}

bool InputArea::contains(avg::Vector2f pos) const
{
    pos = transform_.inverse() * pos;
    bool b = true;
    for (auto const& obb : obbs_)
        b = b && obb.contains(pos);

    return b;
}

bool InputArea::acceptsButtonEvent(PointerButtonEvent const& e) const
{
    return (!onUp_.empty() || !onDown_.empty())
        && contains(e.pos);
}

bool InputArea::acceptsMoveEvent(PointerMoveEvent const& e) const
{
    return !onMove_.empty() && contains(e.pos);
}

avg::Transform InputArea::getTransform() const
{
    return transform_;
}

std::vector<avg::Obb> const& InputArea::getObbs() const
{
    return obbs_;
}

InputArea InputArea::transform(avg::Transform const& t) &&
{
    transform_ = t * transform_;
    /*for (auto& obb : obbs_)
        obb = t * obb;*/

    return std::move(*this);
}

InputArea InputArea::clip(avg::Obb const& obb) &&
{
    obbs_.push_back(transform_.inverse() * obb);
    return std::move(*this);
}

InputArea InputArea::onDown(
        btl::Function<void (PointerButtonEvent const& e)>
        f) &&
{
    onDown_.push_back(std::move(f));
    return std::move(*this);
}

InputArea InputArea::onUp(btl::Function<void (PointerButtonEvent const& e)>
        f) &&
{
    onUp_.push_back(std::move(f));
    return std::move(*this);
}

InputArea InputArea::onMove(btl::Function<void (PointerMoveEvent const& e)>
        f) &&
{
    onMove_.push_back(std::move(f));
    return std::move(*this);
}

void InputArea::emitButtonEvent(PointerButtonEvent const& e) const
{
    if (e.state == ase::ButtonState::down)
    {
        for (auto const& cb : onDown_)
            cb(transformPointerButtonEvent(e, transform_.inverse()));
    }
    else if (e.state == ase::ButtonState::up)
    {
        for (auto const& cb : onUp_)
            cb(transformPointerButtonEvent(e, transform_.inverse()));
    }
}

void InputArea::emitMoveEvent(PointerMoveEvent const& e) const
{
    for (auto const& cb : onMove_)
        cb(transformPointerMoveEvent(e, transform_.inverse()));
}

std::vector<
btl::Function<void (PointerButtonEvent const& e)>
> const& InputArea::getOnDowns() const
{
    return onDown_;
}

std::vector<
btl::Function<void (PointerButtonEvent const& e)>
> const& InputArea::getOnUps() const
{
    return onUp_;
}

std::ostream& operator<<(std::ostream& stream,
        reactive::InputArea const& area)
{
    stream << "InputArea{"
        << area.transform_ << ","
        << "handlers: " << area.onDown_.size();

    for (auto obb : area.obbs_)
        stream << ", " << obb;

    stream << "}";
    return stream;
}

auto makeInputArea(std::vector<avg::Obb>&& obbs) -> InputArea
{
    return InputArea{std::move(obbs)};
}

auto makeInputArea(avg::Obb const& obb)
    -> InputArea
{
    return InputArea(obb);
}

} // namespace

