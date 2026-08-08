#include "constraintbox.h"

#include "constraintlayout.h"

#include "bqui/widget/box.h"
#include "bqui/widget/layout.h"
#include "bqui/widget/widget.h"

#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/handlegravity.h"
#include "bqui/modifier/setid.h"
#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/setwidgetintrospection.h"
#include "bqui/modifier/transform.h"

#include "bqui/provider/providebuildparams.h"

#include "bqui/sizehint.h"

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <avg/obb.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <arrange/expression.h>
#include <arrange/strength.h>
#include <arrange/variable.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace bqui::widget
{

namespace
{

// The same placement step layout() uses: split a child's obb into a transform
// and a size, place the builder, and give the realised instance a fresh id so a
// dynamic list can match it by identity rather than by position.
bq::signal::AnySignal<widget::Instance> buildChild(
        widget::AnyBuilder const& builder,
        bq::signal::AnySignal<avg::Obb> obb)
{
    auto shared = obb.share();
    auto transform = shared.map(&avg::Obb::getTransform);
    auto size = shared.map(&avg::Obb::getSize);

    auto placed = builder.clone()
        | modifier::transformBuilder(std::move(transform));

    return (std::move(placed)(std::move(size))
        | modifier::setElementId(bq::signal::constant(avg::UniqueId()))
        ).getInstance();
}

LayoutSpec makeVerticalSpec(BoxVariables const& container,
        arrange::Variable const& stretch, avg::Vector2f size,
        std::vector<BoxVariables> const& boxes,
        std::vector<SizeHint> const& hints)
{
    LayoutSpec spec;

    auto append = [&spec](std::vector<arrange::Constraint> constraints)
    {
        for (auto& constraint : constraints)
            spec.constraints.push_back(std::move(constraint));
    };

    append(anchorConstraints(container, 0.0f, 0.0f, size[0], size[1]));
    append(boxConstraints(container, boxes, Axis::y));

    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        SizeHintResult height = hints[i].getHeightForWidth(size[0]);
        float minHeight = height[0];
        float naturalHeight = height[1];
        float maxHeight = height[2];

        arrange::Expression childHeight = boxes[i].height();

        // The band each child is kept inside, firmer than the size it settles
        // at within the band.
        spec.constraints.push_back(
                (childHeight >= arrange::Expression(minHeight))
                | arrange::Strength::strong());
        spec.constraints.push_back(
                (childHeight <= arrange::Expression(maxHeight))
                | arrange::Strength::strong());

        if (maxHeight > naturalHeight)
        {
            // A filler grows past its natural size, so it takes no natural pin.
            // Every filler is tied to the container's one stretch variable, so
            // several of them split the leftover space equally; the container's
            // fixed extent then drives that variable to absorb the remainder.
            spec.constraints.push_back(
                    (childHeight == arrange::Expression(stretch))
                    | arrange::Strength::medium());
        }
        else
        {
            spec.constraints.push_back(
                    (childHeight == arrange::Expression(naturalHeight))
                    | arrange::Strength::medium());
        }
    }

    for (BoxVariables const& box : boxes)
    {
        spec.variables.push_back(box.left);
        spec.variables.push_back(box.top);
        spec.variables.push_back(box.right);
        spec.variables.push_back(box.bottom);
    }

    return spec;
}

// The solver works in top-down window space, where y grows downwards from the
// container's top. The widget tree is y-up with the origin at the bottom-left,
// so a child's bottom edge in solver space becomes its origin here.
std::vector<avg::Obb> toObbs(LayoutSolution const& solution,
        std::vector<BoxVariables> const& boxes, avg::Vector2f size)
{
    std::vector<avg::Obb> obbs;
    obbs.reserve(boxes.size());

    for (BoxVariables const& box : boxes)
    {
        avg::Obb solved = readObb(solution, box);
        avg::Vector2f childSize = solved.getSize();
        avg::Vector2f topLeft = solved.getTransform().getTranslation();
        float bottomEdge = topLeft[1] + childSize[1];

        obbs.push_back(avg::Transform().translate(
                    avg::Vector2f(topLeft[0], size[1] - bottomEdge))
                * avg::Obb(childSize));
    }

    return obbs;
}

AnyWidget solverVbox(bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    BoxVariables container;
    arrange::Variable stretch;

    auto boxes = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return bq::signal::AnySignal<BoxVariables>(
                            bq::signal::constant(builder.getBoxVariables()));
                })).share();

    auto hints = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getSizeHint();
                })).share();

    auto widget = makeWidgetWithSize(
            [container, stretch](auto size, auto boxes, auto hints, auto array)
            {
                auto sharedSize = std::move(size).share();

                auto spec = merge(sharedSize.clone(), boxes.clone(),
                        std::move(hints))
                    .map([container, stretch](avg::Vector2f size,
                                std::vector<BoxVariables> const& boxes,
                                std::vector<SizeHint> const& hints)
                        {
                            return makeVerticalSpec(container, stretch, size,
                                    boxes, hints);
                        });

                auto solution = solveLayout(
                        bq::signal::AnySignal<LayoutSpec>(std::move(spec)));

                auto obbs = merge(std::move(solution), std::move(boxes),
                        std::move(sharedSize))
                    .map([](LayoutSolution const& solution,
                                std::vector<BoxVariables> const& boxes,
                                avg::Vector2f size)
                        {
                            return toObbs(solution, boxes, size);
                        });

                auto instances = bq::signal::join(bq::signal::scatter(
                            std::move(array), std::move(obbs), &buildChild));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            },
            boxes,
            hints,
            std::move(array)
            );

    SizeHintMap sizeHintMap = accumulateSizeHints<Axis::y>;

    return std::move(widget)
        | modifier::setSizeHint(hints.map(std::move(sizeHintMap)))
        ;
}

} // namespace

AnyWidget solverVbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return makeWidget([](BuildParams const& params, auto widgets)
        {
            auto builders = widgets.map(
                    [params](widget::AnyWidget const& widget)
                    -> widget::AnyBuilder
                    {
                        return (widget.clone()
                                | modifier::handleGravity()
                                )(params);
                    });

            return solverVbox(std::move(builders));
        },
        provider::provideBuildParams(),
        std::move(widgets)
        );
}

AnyWidget solverVbox(std::vector<AnyWidget> widgets)
{
    std::vector<bq::signal::ArraySignal<widget::AnyWidget>> children;
    children.reserve(widgets.size());

    for (auto&& widget : widgets)
        children.push_back(std::move(widget));

    return solverVbox(
            bq::signal::ArraySignal<widget::AnyWidget>(std::move(children)));
}

} // namespace bqui::widget
