#include "inputarea.h"

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
    obbs_.push_back(transform_ * obb);
    return std::move(*this);
}

InputArea InputArea::onDown(
        std::function<void (ase::PointerButtonEvent const& e)> const& f) &&
{
    onDown_.push_back(f);
    return std::move(*this);
}

InputArea InputArea::onUp(std::function<void (ase::PointerButtonEvent const& e)>
        const& f) &&
{
    onUp_.push_back(f);
    return std::move(*this);
}

void InputArea::emit(ase::PointerButtonEvent const& e) const
{
    /*if (!contains(e.getPos()))
        return;*/

    if (e.getState() == ase::ButtonState::down)
    {
        for (auto const& cb : onDown_)
            cb(e.transform(transform_.inverse()));
    }
    else if (e.getState() == ase::ButtonState::up)
    {
        for (auto const& cb : onUp_)
            cb(e.transform(transform_.inverse()));
    }
}

std::vector<
std::function<void (ase::PointerButtonEvent const& e)>
> const& InputArea::getOnDowns() const
{
    return onDown_;
}

std::vector<
std::function<void (ase::PointerButtonEvent const& e)>
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

