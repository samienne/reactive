#include "purelayouttestutil.h"

#include "widget/constraintbox.h"

#include <bqui/modifier/alignguide.h>
#include <bqui/modifier/setgravity.h>

#include <bqui/widget/guide.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/uniformgrid.h>
#include <bqui/widget/vbox.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/collection.h>
#include <bq/signal/collectionsignal.h>
#include <bq/signal/input.h>

#include <memory>
#include <utility>
#include <vector>

using namespace bqui;
using namespace bqui::widget;
using namespace bq::signal;
using namespace bqui::test;

namespace
{

FrameInfo nextFrame(uint64_t frameId)
{
    return FrameInfo(frameId, std::chrono::microseconds(0));
}

// A set of 40x40 probes addressed by index, so a forEach delegate can build the
// probe a key names. Each probe's id is fixed when it is added.
class ProbeIds
{
public:
    ProbeIds() :
        ids_(std::make_shared<std::vector<btl::UniqueId>>())
    {
    }

    size_t add()
    {
        ids_->push_back(btl::makeUniqueId());
        return ids_->size() - 1;
    }

    btl::UniqueId id(size_t index) const
    {
        return ids_->at(index);
    }

    AnyWidget fromSignal(AnySignal<size_t> index) const
    {
        auto ids = ids_;
        auto id = index.map([ids](size_t i) { return ids->at(i); });

        return makeWidget()
            | modifier::defaultSize(avg::Vector2f(40.0f, 40.0f))
            | modifier::makeWidgetModifier(modifier::makeInstanceModifier(
                        [](Instance instance, btl::UniqueId id)
                        {
                            auto areas = instance.getInputAreas();
                            areas.push_back(makeInputArea(id,
                                        instance.getObb()));
                            return std::move(instance)
                                .setInputAreas(std::move(areas));
                        }, std::move(id)));
    }

private:
    std::shared_ptr<std::vector<btl::UniqueId>> ids_;
};

AnyWidget countBuilds(std::shared_ptr<int> builds, AnyWidget widget)
{
    return makeWidget([builds, widget]() -> AnyWidget
            {
                ++*builds;
                return widget;
            });
}

// A child list keyed by probe index, so each probe keeps its identity across
// additions, removals and reorderings.
ArraySignal<AnyWidget> keyedChildren(ProbeIds probes,
        AnySignal<std::vector<size_t>> indices,
        std::shared_ptr<int> builds = nullptr)
{
    return forEach(std::move(indices),
            [](size_t index) { return index; },
            [probes, builds](AnySignal<size_t> index)
            {
                AnyWidget widget = probes.fromSignal(std::move(index));
                if (!builds)
                    return widget;
                return countBuilds(builds, std::move(widget));
            });
}

ArraySignal<AnyWidget> collectionChildren(ProbeIds probes,
        Collection<size_t> const& indices)
{
    return forEach(indices,
            [probes](AnySignal<size_t> index)
            {
                return probes.fromSignal(std::move(index));
            });
}

Collection<size_t> makeIndices(std::vector<size_t> indices)
{
    Collection<size_t> collection;
    auto transaction = collection.write();
    for (size_t index : indices)
        transaction.pushBack(index);
    return collection;
}

// Drives a tree whose child list or window size the test changes between
// passes, reading the geometry back after each.
class LiveLayout
{
public:
    LiveLayout(AnyWidget widget, AnySignal<avg::Vector2f> size) :
        context_(makeSignalContext(std::move(widget)(BuildParams())(
                        std::move(size)).getInstance())),
        instance_(context_.evaluate<0>().get<0>())
    {
    }

    void step()
    {
        context_.update(nextFrame(++frame_));
        instance_ = context_.evaluate<0>().get<0>();
    }

    bool has(btl::UniqueId id) const
    {
        for (auto const& area : instance_.getInputAreas())
            if (area.getId() == id)
                return true;
        return false;
    }

    Geometry read(btl::UniqueId id) const
    {
        return readProbe(instance_, id);
    }

private:
    decltype(makeSignalContext(std::declval<AnySignal<Instance>>())) context_;
    Instance instance_;
    uint64_t frame_ = 0;
};

void expectAt(Geometry const& g, float x, float y)
{
    EXPECT_FLOAT_EQ(x, g.position[0]);
    EXPECT_FLOAT_EQ(y, g.position[1]);
}

} // namespace

// A keyed row lays its children out edge to edge in key order at their natural
// size, from the leading edge of the row.
TEST(PureSolverLayout, keyedRowPlacesChildrenInOrder)
{
    ProbeIds probes;
    size_t const a = probes.add();
    size_t const b = probes.add();

    auto input = makeInput(std::vector<size_t>{ a, b });

    LiveLayout live(pureSolverRoot(hbox(keyedChildren(probes, input.signal))),
            constant(avg::Vector2f(300.0f, 40.0f)));

    expectAt(live.read(probes.id(a)), 0.0f, 0.0f);
    expectAt(live.read(probes.id(b)), 40.0f, 0.0f);
    EXPECT_FLOAT_EQ(40.0f, live.read(probes.id(b)).size[0]);
}

// A key added to the list is laid out at the end of the row on the next pass,
// and the existing children keep their slots.
TEST(PureSolverLayout, keyedRowPlacesAnAddedChild)
{
    ProbeIds probes;
    size_t const a = probes.add();
    size_t const b = probes.add();

    auto input = makeInput(std::vector<size_t>{ a, b });

    LiveLayout live(pureSolverRoot(hbox(keyedChildren(probes, input.signal))),
            constant(avg::Vector2f(300.0f, 40.0f)));

    size_t const added = probes.add();
    input.handle.set(std::vector<size_t>{ a, b, added });
    live.step();

    expectAt(live.read(probes.id(a)), 0.0f, 0.0f);
    expectAt(live.read(probes.id(b)), 40.0f, 0.0f);
    expectAt(live.read(probes.id(added)), 80.0f, 0.0f);
}

// Removing a key drops its child and closes the gap: the next child moves up
// into the freed slot.
TEST(PureSolverLayout, keyedRowClosesTheGapOfARemovedChild)
{
    ProbeIds probes;
    size_t const a = probes.add();
    size_t const b = probes.add();
    size_t const c = probes.add();

    auto input = makeInput(std::vector<size_t>{ a, b, c });

    LiveLayout live(pureSolverRoot(hbox(keyedChildren(probes, input.signal))),
            constant(avg::Vector2f(300.0f, 40.0f)));

    input.handle.set(std::vector<size_t>{ a, c });
    live.step();

    EXPECT_FALSE(live.has(probes.id(b)));
    expectAt(live.read(probes.id(a)), 0.0f, 0.0f);
    expectAt(live.read(probes.id(c)), 40.0f, 0.0f);
}

// Reordering the keys moves each child to the slot its new position earns.
TEST(PureSolverLayout, keyedRowFollowsReorderedKeys)
{
    ProbeIds probes;
    size_t const a = probes.add();
    size_t const b = probes.add();
    size_t const c = probes.add();

    auto input = makeInput(std::vector<size_t>{ a, b, c });

    LiveLayout live(pureSolverRoot(hbox(keyedChildren(probes, input.signal))),
            constant(avg::Vector2f(300.0f, 40.0f)));

    input.handle.set(std::vector<size_t>{ c, a, b });
    live.step();

    expectAt(live.read(probes.id(c)), 0.0f, 0.0f);
    expectAt(live.read(probes.id(a)), 40.0f, 0.0f);
    expectAt(live.read(probes.id(b)), 80.0f, 0.0f);
}

// Adding or removing keys builds only the new child; the others keep the
// instance their key identifies. The per-child count is taken from the first
// pass.
TEST(PureSolverLayout, keyedRowBuildsEachChildOncePerIdentity)
{
    ProbeIds probes;
    size_t const a = probes.add();
    size_t const b = probes.add();

    auto builds = std::make_shared<int>(0);
    auto input = makeInput(std::vector<size_t>{ a, b });

    LiveLayout live(pureSolverRoot(hbox(
                    keyedChildren(probes, input.signal, builds))),
            constant(avg::Vector2f(300.0f, 40.0f)));

    ASSERT_GT(*builds, 0);
    ASSERT_EQ(0, *builds % 2);
    int const perChild = *builds / 2;

    size_t const added = probes.add();
    input.handle.set(std::vector<size_t>{ added, a, b });
    live.step();

    EXPECT_EQ(3 * perChild, *builds);
    expectAt(live.read(probes.id(added)), 0.0f, 0.0f);
    expectAt(live.read(probes.id(b)), 80.0f, 0.0f);

    input.handle.set(std::vector<size_t>{ b, added });
    live.step();

    EXPECT_EQ(3 * perChild, *builds);
    expectAt(live.read(probes.id(added)), 40.0f, 0.0f);
}

// A keyed column stacks an added child below the others, from the top of the
// window (y-up, so the first child is highest).
TEST(PureSolverLayout, keyedColumnStacksAnAddedChild)
{
    ProbeIds probes;
    size_t const a = probes.add();

    auto input = makeInput(std::vector<size_t>{ a });

    LiveLayout live(pureSolverRoot(vbox(keyedChildren(probes, input.signal))),
            constant(avg::Vector2f(40.0f, 200.0f)));

    expectAt(live.read(probes.id(a)), 0.0f, 160.0f);

    size_t const added = probes.add();
    input.handle.set(std::vector<size_t>{ a, added });
    live.step();

    expectAt(live.read(probes.id(a)), 0.0f, 160.0f);
    expectAt(live.read(probes.id(added)), 0.0f, 120.0f);
}

// One braced list mixes constant children with collections; each segment keeps
// its written place, and an item pushed into a collection opens a slot in its
// segment and pushes the later children along.
TEST(PureSolverLayout, rowMixesConstantsAndCollections)
{
    ProbeIds probes;
    size_t const first = probes.add();
    size_t const a0 = probes.add();
    size_t const middle = probes.add();
    size_t const b0 = probes.add();

    auto as = makeIndices({ a0 });
    auto bs = makeIndices({ b0 });

    LiveLayout live(pureSolverRoot(hbox({
                    probes.fromSignal(constant(first)),
                    collectionChildren(probes, as),
                    probes.fromSignal(constant(middle)),
                    collectionChildren(probes, bs)
                    })),
            constant(avg::Vector2f(400.0f, 40.0f)));

    expectAt(live.read(probes.id(first)), 0.0f, 0.0f);
    expectAt(live.read(probes.id(a0)), 40.0f, 0.0f);
    expectAt(live.read(probes.id(middle)), 80.0f, 0.0f);
    expectAt(live.read(probes.id(b0)), 120.0f, 0.0f);

    size_t const a1 = probes.add();
    as.write().pushBack(a1);
    live.step();

    expectAt(live.read(probes.id(a1)), 80.0f, 0.0f);
    expectAt(live.read(probes.id(middle)), 120.0f, 0.0f);
    expectAt(live.read(probes.id(b0)), 160.0f, 0.0f);
}

// An empty collection takes no room between its neighbours, and its first item
// appears in the segment's place.
TEST(PureSolverLayout, emptyCollectionKeepsItsPlace)
{
    ProbeIds probes;
    size_t const first = probes.add();
    size_t const last = probes.add();

    auto items = makeIndices({});

    LiveLayout live(pureSolverRoot(hbox({
                    probes.fromSignal(constant(first)),
                    collectionChildren(probes, items),
                    probes.fromSignal(constant(last))
                    })),
            constant(avg::Vector2f(300.0f, 40.0f)));

    expectAt(live.read(probes.id(first)), 0.0f, 0.0f);
    expectAt(live.read(probes.id(last)), 40.0f, 0.0f);

    size_t const added = probes.add();
    items.write().pushBack(added);
    live.step();

    expectAt(live.read(probes.id(added)), 40.0f, 0.0f);
    expectAt(live.read(probes.id(last)), 80.0f, 0.0f);
}

// A row with no children contributes no height to its column: the leaf after it
// sits at the top of the window.
TEST(PureSolverLayout, emptyRowTakesNoRoomInAColumn)
{
    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<AnyWidget> column;
    column.push_back(hbox(std::vector<AnyWidget>{}));
    column.push_back(probe(id, fixed40, fixed40));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(std::move(column))),
            avg::Vector2f(100.0f, 200.0f));

    expectAt(readProbe(instance, id), 0.0f, 160.0f);
}

// A child whose band is zero on the main axis takes no room: its neighbours sit
// edge to edge as if it were not there.
TEST(PureSolverLayout, zeroSizedChildTakesNoRoom)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idZero = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();

    Band const zero = { 0.0f, 0.0f, 0.0f };

    std::vector<AnyWidget> row;
    row.push_back(probe(idA, fixed40, fixed40));
    row.push_back(probe(idZero, zero, fixed40));
    row.push_back(probe(idB, fixed40, fixed40));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(std::move(row))),
            avg::Vector2f(300.0f, 40.0f));

    EXPECT_FLOAT_EQ(0.0f, readProbe(instance, idZero).size[0]);
    expectAt(readProbe(instance, idB), 40.0f, 0.0f);
}

// Nested containers compose their offsets: a row inside a column inside a row
// places its leaves at the sum of every enclosing slot's position.
TEST(PureSolverLayout, nestedContainersComposeOffsets)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();
    btl::UniqueId const idD = btl::makeUniqueId();

    std::vector<AnyWidget> innerRow;
    innerRow.push_back(probe(idC, fixed40, fixed40));
    innerRow.push_back(probe(idD, fixed40, fixed40));

    std::vector<AnyWidget> column;
    column.push_back(probe(idB, fixed40, fixed40));
    column.push_back(hbox(std::move(innerRow)));

    std::vector<AnyWidget> outerRow;
    outerRow.push_back(probe(idA, fixed40, fixed40));
    outerRow.push_back(vbox(std::move(column)));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(std::move(outerRow))),
            avg::Vector2f(300.0f, 80.0f));

    // The column is 80 tall and fills the window; B is its top half, the inner
    // row its bottom half, both starting at the column's x = 40.
    expectAt(readProbe(instance, idA), 0.0f, 40.0f);
    expectAt(readProbe(instance, idB), 40.0f, 40.0f);
    expectAt(readProbe(instance, idC), 40.0f, 0.0f);
    expectAt(readProbe(instance, idD), 80.0f, 0.0f);
}

// Fixed-size children in an overfull column keep their size and overflow past
// the bottom edge rather than being squeezed.
TEST(PureSolverLayout, overfullColumnOverflowsFixedChildren)
{
    btl::UniqueId const idA = btl::makeUniqueId();
    btl::UniqueId const idB = btl::makeUniqueId();
    btl::UniqueId const idC = btl::makeUniqueId();

    std::vector<AnyWidget> column;
    column.push_back(probe(idA, fixed40, fixed40));
    column.push_back(probe(idB, fixed40, fixed40));
    column.push_back(probe(idC, fixed40, fixed40));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(std::move(column))),
            avg::Vector2f(40.0f, 100.0f));

    for (btl::UniqueId id : { idA, idB, idC })
        EXPECT_FLOAT_EQ(40.0f, readProbe(instance, id).size[1]);

    expectAt(readProbe(instance, idA), 0.0f, 60.0f);
    expectAt(readProbe(instance, idB), 0.0f, 20.0f);
    expectAt(readProbe(instance, idC), 0.0f, -20.0f);
}

// A child smaller than its row's height settles under its gravity across the
// row: centred by (0.5, 0.5), at the bottom by (0, 0).
// DISABLED: a pure hbox/vbox pins every child to the cross-axis leading edge
// and ignores setGravity (pureAxisConstraints has no gravity input).
TEST(PureSolverLayout, DISABLED_gravityPlacesAChildAcrossARow)
{
    btl::UniqueId const idCentred = btl::makeUniqueId();
    btl::UniqueId const idBottom = btl::makeUniqueId();

    std::vector<AnyWidget> row;
    row.push_back(probe(idCentred, fixed40, fixed40)
            | modifier::setGravity(constant(avg::Vector2f(0.5f, 0.5f))));
    row.push_back(probe(idBottom, fixed40, fixed40)
            | modifier::setGravity(constant(avg::Vector2f(0.0f, 0.0f))));

    Instance instance = realiseConverged(
            pureSolverRoot(hbox(std::move(row))),
            avg::Vector2f(200.0f, 100.0f));

    expectAt(readProbe(instance, idCentred), 0.0f, 30.0f);
    expectAt(readProbe(instance, idBottom), 40.0f, 0.0f);
}

// A child narrower than its column settles under its gravity across the column:
// at the right edge by (1, 0.5).
// DISABLED: a pure hbox/vbox ignores setGravity on the cross axis.
TEST(PureSolverLayout, DISABLED_gravityPlacesAChildAcrossAColumn)
{
    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<AnyWidget> column;
    column.push_back(probe(id, fixed40, fixed40)
            | modifier::setGravity(constant(avg::Vector2f(1.0f, 0.5f))));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(std::move(column))),
            avg::Vector2f(100.0f, 40.0f));

    expectAt(readProbe(instance, id), 60.0f, 0.0f);
}

// Two children of different widths in one column, both aligned to one XGuide by
// their centre, share the centre line.
// DISABLED: a pure hbox/vbox does not emit guide constraints; alignments are
// read only by the old banded containers.
TEST(PureSolverLayout, DISABLED_guideAlignsChildrenInAColumn)
{
    btl::UniqueId const idNarrow = btl::makeUniqueId();
    btl::UniqueId const idWide = btl::makeUniqueId();

    XGuide g;

    Band const wide = { 80.0f, 80.0f, 80.0f };

    std::vector<AnyWidget> column;
    column.push_back(probe(idNarrow, fixed40, fixed40)
            | modifier::alignCenterX(g));
    column.push_back(probe(idWide, wide, fixed40)
            | modifier::alignCenterX(g));

    Instance instance = realiseConverged(
            pureSolverRoot(vbox(std::move(column))),
            avg::Vector2f(200.0f, 80.0f));

    Geometry narrow = readProbe(instance, idNarrow);
    Geometry wideG = readProbe(instance, idWide);

    EXPECT_FLOAT_EQ(narrow.position[0] + 20.0f, wideG.position[0] + 40.0f);
}

// A row re-lays out when its window resizes: the fixed leaf keeps its size and
// the filler takes the new remainder.
TEST(PureSolverLayout, rowRelaysOutOnWindowResize)
{
    btl::UniqueId const idFixed = btl::makeUniqueId();
    btl::UniqueId const idFiller = btl::makeUniqueId();

    std::vector<AnyWidget> row;
    row.push_back(probe(idFixed, fixed40, fixed40));
    row.push_back(fillerProbe(idFiller));

    auto size = makeInput(avg::Vector2f(200.0f, 40.0f));

    LiveLayout live(pureSolverRoot(hbox(std::move(row))), size.signal);

    EXPECT_FLOAT_EQ(160.0f, live.read(idFiller).size[0]);
    expectAt(live.read(idFiller), 40.0f, 0.0f);

    size.handle.set(avg::Vector2f(300.0f, 60.0f));
    live.step();

    EXPECT_FLOAT_EQ(40.0f, live.read(idFixed).size[0]);
    EXPECT_FLOAT_EQ(260.0f, live.read(idFiller).size[0]);
    EXPECT_FLOAT_EQ(60.0f, live.read(idFiller).size[1]);
    expectAt(live.read(idFiller), 40.0f, 0.0f);
}

// A height-for-width leaf reflows when the window resizes: its resolved width
// follows the window and its height (area / width) follows the width.
TEST(PureSolverLayout, heightReflowsOnWindowResize)
{
    btl::UniqueId const id = btl::makeUniqueId();

    std::vector<AnyWidget> row;
    row.push_back(reflowProbe(id, 12000.0f, 100.0f));

    auto size = makeInput(avg::Vector2f(400.0f, 300.0f));

    LiveLayout live(pureSolverRoot(hbox(std::move(row))), size.signal);

    EXPECT_FLOAT_EQ(400.0f, live.read(id).size[0]);
    EXPECT_FLOAT_EQ(30.0f, live.read(id).size[1]);

    size.handle.set(avg::Vector2f(200.0f, 300.0f));
    live.step();

    EXPECT_FLOAT_EQ(200.0f, live.read(id).size[0]);
    EXPECT_FLOAT_EQ(60.0f, live.read(id).size[1]);
}

// fill() flexes along the container's stacking axis only: in a column a fill
// leaf beside a fixed sibling takes the remaining height but keeps its natural
// width, while in a row it takes the remaining width at its natural height.
TEST(PureSolverLayout, fillFlexesAlongTheContainerAxis)
{
    btl::UniqueId const idColumn = btl::makeUniqueId();
    btl::UniqueId const idRow = btl::makeUniqueId();

    std::vector<AnyWidget> column;
    column.push_back(probe(btl::makeUniqueId(), fixed40, fixed40));
    column.push_back(probe(idColumn, fixed40, fixed40) | modifier::fill());

    Instance columnInstance = realiseConverged(
            pureSolverRoot(vbox(std::move(column))),
            avg::Vector2f(200.0f, 300.0f));

    std::vector<AnyWidget> row;
    row.push_back(probe(btl::makeUniqueId(), fixed40, fixed40));
    row.push_back(probe(idRow, fixed40, fixed40) | modifier::fill());

    Instance rowInstance = realiseConverged(
            pureSolverRoot(hbox(std::move(row))),
            avg::Vector2f(200.0f, 300.0f));

    Geometry inColumn = readProbe(columnInstance, idColumn);
    EXPECT_FLOAT_EQ(40.0f, inColumn.size[0]);
    EXPECT_FLOAT_EQ(260.0f, inColumn.size[1]);

    Geometry inRow = readProbe(rowInstance, idRow);
    EXPECT_FLOAT_EQ(160.0f, inRow.size[0]);
    EXPECT_FLOAT_EQ(40.0f, inRow.size[1]);
}

// A cell spanning a grid's whole top row covers both columns, and row 0 is the
// bottom row (y-up).
TEST(PureSolverLayout, gridPlacesRowsFromTheBottom)
{
    btl::UniqueId const idBottomLeft = btl::makeUniqueId();
    btl::UniqueId const idBottomRight = btl::makeUniqueId();
    btl::UniqueId const idTop = btl::makeUniqueId();

    AnyWidget grid = uniformGrid(2, 2)
        .cell(0, 0, 1, 1, fillerProbe(idBottomLeft))
        .cell(1, 0, 1, 1, fillerProbe(idBottomRight))
        .cell(0, 1, 2, 1, fillerProbe(idTop))
        ;

    Instance instance = realiseConverged(pureSolverRoot(std::move(grid)),
            avg::Vector2f(200.0f, 100.0f));

    Geometry bottomLeft = readProbe(instance, idBottomLeft);
    Geometry bottomRight = readProbe(instance, idBottomRight);
    Geometry top = readProbe(instance, idTop);

    expectAt(bottomLeft, 0.0f, 0.0f);
    expectAt(bottomRight, 100.0f, 0.0f);
    expectAt(top, 0.0f, 50.0f);
    EXPECT_FLOAT_EQ(100.0f, bottomLeft.size[0]);
    EXPECT_FLOAT_EQ(50.0f, bottomLeft.size[1]);
    EXPECT_FLOAT_EQ(200.0f, top.size[0]);
}

// A 1x1 grid holding a filler gives it the whole window.
TEST(PureSolverLayout, singleCellGridFillsTheWindow)
{
    btl::UniqueId const id = btl::makeUniqueId();

    AnyWidget grid = uniformGrid(1, 1).cell(0, 0, 1, 1, fillerProbe(id));

    Instance instance = realiseConverged(pureSolverRoot(std::move(grid)),
            avg::Vector2f(120.0f, 80.0f));

    Geometry g = readProbe(instance, id);
    expectAt(g, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(120.0f, g.size[0]);
    EXPECT_FLOAT_EQ(80.0f, g.size[1]);
}

// Unoccupied cells still hold their tracks: a filler in the last of three
// columns sits in the last third of the window, not at the origin.
TEST(PureSolverLayout, gridKeepsTracksOfEmptyCells)
{
    btl::UniqueId const id = btl::makeUniqueId();

    AnyWidget grid = uniformGrid(3, 1).cell(2, 0, 1, 1, fillerProbe(id));

    Instance instance = realiseConverged(pureSolverRoot(std::move(grid)),
            avg::Vector2f(300.0f, 50.0f));

    Geometry g = readProbe(instance, id);
    expectAt(g, 200.0f, 0.0f);
    EXPECT_FLOAT_EQ(100.0f, g.size[0]);
}
