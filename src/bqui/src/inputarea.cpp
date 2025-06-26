#include "bqui/inputarea.h"

#include "bqui/pointerbuttonevent.h"
#include "bqui/hoverevent.h"

namespace bqui
{

InputArea::InputArea(btl::UniqueId id, avg::Obb const& obb) :
    id_(id),
    obbs_({obb})
{
}

InputArea::InputArea(btl::UniqueId id, std::vector<avg::Obb>&& obbs) :
    id_(id),
    obbs_(std::move(obbs))
{
}

btl::UniqueId InputArea::getId() const
{
    return id_;
}

bool InputArea::contains(avg::Vector2f pos) const
{
    auto t = transform_.inverse();
    pos =  t.getTranslation() + t.getRsMatrix() * pos;

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
        btl::Function<EventResult (PointerButtonEvent const& e)>
        f) &&
{
    onDown_.push_back(std::move(f));
    return std::move(*this);
}

InputArea InputArea::onUp(btl::Function<EventResult (PointerButtonEvent const& e)>
        f) &&
{
    onUp_.push_back(std::move(f));
    return std::move(*this);
}

InputArea InputArea::onMove(btl::Function<EventResult (PointerMoveEvent const& e)>
        f) &&
{
    onMove_.push_back(std::move(f));
    return std::move(*this);
}

InputArea InputArea::onHover(btl::Function<void (HoverEvent const& e)> f) &&
{
    onHover_.push_back(std::move(f));
    return std::move(*this);
}

EventResult InputArea::emitButtonEvent(PointerButtonEvent const& e) const
{
    if (e.state == ase::ButtonState::down)
    {
        bool possible = false;

        for (auto const& cb : onDown_)
        {
            EventResult r = cb(transformPointerButtonEvent(
                        e, transform_.inverse()));

            switch (r)
            {
                case EventResult::accept:
                    return r;
                    break;
                case EventResult::possible:
                    possible = true;
                    break;
                case EventResult::reject:
                    break;
            }
        }

        if (possible)
            return EventResult::possible;

        return EventResult::reject;
    }
    else if (e.state == ase::ButtonState::up)
    {
        bool possible = false;

        for (auto const& cb : onUp_)
        {
            EventResult r = cb(transformPointerButtonEvent(
                        e, transform_.inverse()));

            switch (r)
            {
                case EventResult::accept:
                    return r;
                    break;
                case EventResult::possible:
                    possible = true;
                    break;
                case EventResult::reject:
                    break;
            }
        }

        if (possible)
            return EventResult::possible;

        return EventResult::reject;
    }

    return EventResult::reject;
}

EventResult InputArea::emitMoveEvent(PointerMoveEvent const& e) const
{
    auto event = transformPointerMoveEvent(e, transform_.inverse());

    if (e.hover && !contains(e.pos))
        event.hover = false;

    bool possible = false;
    for (auto const& cb : onMove_)
    {
        EventResult r = cb(event);
        switch (r)
        {
            case EventResult::accept:
                return r;
                break;
            case EventResult::possible:
                possible = true;
                break;
            case EventResult::reject:
                break;
        }
    }

    if (possible || (onMove_.empty() && !onUp_.empty()))
        return EventResult::possible;

    return EventResult::reject;
}

void InputArea::emitHoverEvent(ase::HoverEvent const& event) const
{
    for (auto const& cb : onHover_)
        cb(event);
}

std::vector<
btl::Function<EventResult (PointerButtonEvent const& e)>
> const& InputArea::getOnDowns() const
{
    return onDown_;
}

std::vector<
btl::Function<EventResult (PointerButtonEvent const& e)>
> const& InputArea::getOnUps() const
{
    return onUp_;
}

std::ostream& operator<<(std::ostream& stream,
        bqui::InputArea const& area)
{
    stream << "InputArea{"
        << area.transform_ << ","
        << "handlers: " << area.onDown_.size();

    for (auto obb : area.obbs_)
        stream << ", " << obb;

    stream << "}";
    return stream;
}

auto makeInputArea(btl::UniqueId id, std::vector<avg::Obb>&& obbs) -> InputArea
{
    return InputArea{id, std::move(obbs)};
}

auto makeInputArea(btl::UniqueId id, avg::Obb const& obb)
    -> InputArea
{
    return InputArea(id, obb);
}

}

