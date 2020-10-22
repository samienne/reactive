#include "region.h"

#include "simplepolygon.h"
#include "rect.h"

#include "poly2tri/poly2tri.h"
#include "clipper/clipper.hpp"

#include "debug.h"

#include <pmr/vector.h>
#include <pmr/shared_ptr.h>
#include <pmr/queue.h>
#include <pmr/new_delete_resource.h>
#include <pmr/monotonic_buffer_resource.h>

#include <stack>
#include <queue>
#include <stdexcept>
#include <memory>

namespace avg
{

class RegionDeferred
{
public:
    RegionDeferred(pmr::memory_resource* memory);
    RegionDeferred(RegionDeferred const&) = default;
    RegionDeferred(RegionDeferred&&) = default;

    pmr::memory_resource* getResource() const;

public:
    pmr::memory_resource* memory_ = nullptr;
    std::shared_ptr<ClipperLib::PolyTree> paths_;
    Vector2f pixelSize_;
    float resPerPixel_;
};

namespace
{
    /**
     * @brief Prints the polytree contours in javascript clipper form.
     */
    void printClipperPolyTree(std::ostream& stream,
            ClipperLib::PolyTree const& polyTree,
            float resPerPixel, Vector2f pixelSize)
    {

        pmr::monotonic_buffer_resource memory(pmr::new_delete_resource());
        pmr::vector<ClipperLib::Path> paths(&memory);
        ClipperLib::PolyTreeToPaths(polyTree, paths);

        float xRes = resPerPixel / pixelSize[0];
        float yRes = resPerPixel / pixelSize[1];

        stream << "[";
        for (auto i = paths.begin(); i != paths.end(); ++i)
        {
            if (i != paths.begin())
                stream << ",";
            stream << "[";
            for (auto j = i->begin(); j != i->end(); ++j)
            {
                if (j != i->begin())
                    stream << ", ";
                stream << (float)j->X/xRes << ", " << (float)j->Y/yRes;
            }
            stream << "]";
        }
        stream << "]" << std::endl;
    }

    ClipperLib::Path toPath(pmr::memory_resource* memory,
            SimplePolygon const& polygon)
    {
        pmr::vector<Vector2i> const& vertices = polygon.getVertices();
        ClipperLib::Path path(memory);
        for (auto const& vertex : vertices)
            path.push_back(ClipperLib::IntPoint(vertex[0], vertex[1]));

        return path;
    }

    pmr::vector<ClipperLib::Path> fillPaths(
            pmr::memory_resource* memory,
            pmr::vector<SimplePolygon> const& polygons)

    {
        pmr::vector<ClipperLib::Path> result(memory);
        result.reserve(polygons.size());
        for (auto const& polygon : polygons)
        {
            ClipperLib::Path path = toPath(memory, polygon);
            result.push_back(path);
        }

        return result;
    }
} // anonymous namespace

RegionDeferred::RegionDeferred(pmr::memory_resource* memory) :
    memory_(memory)
{
}

pmr::memory_resource* RegionDeferred::getResource() const
{
    return memory_;
}

Region::Region(pmr::memory_resource* memory) :
    memory_(memory)
{
}

Region::Region(pmr::memory_resource* memory,
        pmr::vector<SimplePolygon> const& polygons, FillRule rule,
        Vector2f pixelSize, float resPerPixel) :
    memory_(memory)
{
    if (polygons.empty())
        return;

    deferred_ = pmr::make_shared<RegionDeferred>(memory, memory);
    ClipperLib::PolyFillType type;
    switch (rule)
    {
        case FILL_EVENODD:
            type = ClipperLib::pftEvenOdd;
            break;
        case FILL_NONZERO:
            type = ClipperLib::pftNonZero;
            break;
        case FILL_POSITIVE:
            type = ClipperLib::pftPositive;
            break;
        case FILL_NEGATIVE:
            type = ClipperLib::pftNegative;
            break;
        default:
            assert(false && "Uknown fill rule");
    }

    d()->pixelSize_ = pixelSize;
    d()->resPerPixel_ = resPerPixel;

    pmr::vector<ClipperLib::Path> paths = fillPaths(memory, polygons);

    //ClipperLib::SimplifyPolygons(paths, d()->paths_, type);
    ClipperLib::Clipper c(memory);
    c.StrictlySimple(true);
    c.PreserveCollinear(false);
    c.AddPaths(paths, ClipperLib::ptSubject, true);
    d()->paths_ = pmr::make_shared<ClipperLib::PolyTree>(memory, memory);
    c.Execute(ClipperLib::ctUnion, *d()->paths_, type, type);

    //printClipperPolyTree(d()->paths_);

    /*auto n = d()->paths_->GetFirst();
    while (n)
    {
        assert(ClipperLib::Orientation(n->Contour) != n->IsHole());
        n = n->GetNext();
    }*/
}

Region::Region(pmr::memory_resource* memory,
        pmr::vector<SimplePolygon> const& polygons, JoinType join,
        EndType end, float width, Vector2f pixelSize,
        float resPerPixel) :
    memory_(memory)
{
    if (polygons.empty())
        return;

    deferred_ = pmr::make_shared<RegionDeferred>(memory, memory);

    ClipperLib::JoinType joinType = ClipperLib::jtMiter;
    switch (join)
    {
        case JOIN_MITER:
            joinType = ClipperLib::jtMiter;
            break;

        case JOIN_ROUND:
            joinType = ClipperLib::jtRound;
            break;

        case JOIN_SQUARE:
            joinType = ClipperLib::jtSquare;
            break;
    }

    ClipperLib::EndType endType = ClipperLib::etOpenButt;
    switch (end)
    {
        case END_OPENBUTT:
            endType = ClipperLib::etOpenButt;
            break;

        case END_OPENROUND:
            endType = ClipperLib::etOpenRound;
            break;

        case END_CLOSEDLINE:
            endType = ClipperLib::etClosedLine;
            break;

        case END_OPENSQUARE:
            endType = ClipperLib::etOpenSquare;
            break;

        case END_CLOSEDPOLYGON:
            endType = ClipperLib::etClosedPolygon;
            break;
    }

    d()->pixelSize_ = pixelSize;
    d()->resPerPixel_ = resPerPixel;

    float xRes = (float)d()->resPerPixel_ / d()->pixelSize_[0];
    //float yRes = (float)d()->resPerPixel_ / d()->pixelSize_[1];

    auto resultPaths = pmr::make_shared<ClipperLib::PolyTree>(memory, memory);
    ClipperLib::ClipperOffset clipperOffset(memory);
    pmr::vector<ClipperLib::Path> paths = fillPaths(memory, polygons);

    clipperOffset.AddPaths(paths, joinType, endType);
    clipperOffset.Execute(*resultPaths, width * xRes);

    d()->paths_ = std::move(resultPaths);
}

Region::~Region()
{
}

Region Region::operator|(Region const& /*region*/) const
{
    throw std::runtime_error("Unimplemented");
}

Region Region::operator&(Region const& region) const
{
    pmr::vector<ClipperLib::Path> subjPaths(memory_);
    ClipperLib::PolyTreeToPaths(*d()->paths_, subjPaths);

    pmr::vector<ClipperLib::Path> clipPaths(memory_);
    ClipperLib::PolyTreeToPaths(*region.d()->paths_, clipPaths);

    ClipperLib::Clipper c(d()->getResource());
    c.StrictlySimple(true);
    c.PreserveCollinear(false);
    c.AddPaths(subjPaths, ClipperLib::ptSubject, true);
    c.AddPaths(clipPaths, ClipperLib::ptClip, true);

    std::shared_ptr<ClipperLib::PolyTree> tree =
        pmr::make_shared<ClipperLib::PolyTree>(memory_, memory_);

    c.Execute(ClipperLib::ctIntersection, *tree);

    Region result(memory_);

    result.deferred_ = pmr::make_shared<RegionDeferred>(d()->getResource(),
            d()->getResource());
    result.d()->paths_ = std::move(tree);
    result.d()->resPerPixel_ = d()->resPerPixel_;
    result.d()->pixelSize_ = d()->pixelSize_;

    return result;
}

Region Region::operator^(Region const& /*region*/) const
{
    throw std::runtime_error("Unimplemented");
}

Region& Region::operator|=(Region const& /*region*/) const
{
    throw std::runtime_error("Unimplemented");
}

Region& Region::operator&=(Region const& /*region*/) const
{
    throw std::runtime_error("Unimplemented");
}

Region& Region::operator^=(Region const& /*region*/) const
{
    throw std::runtime_error("Unimplemented");
}

Region Region::offset(JoinType join, EndType end, float offset) const
{
    ClipperLib::JoinType joinType = ClipperLib::jtMiter;
    switch (join)
    {
        case JOIN_MITER:
            joinType = ClipperLib::jtMiter;
            break;

        case JOIN_ROUND:
            joinType = ClipperLib::jtRound;
            break;

        case JOIN_SQUARE:
            joinType = ClipperLib::jtSquare;
            break;
    }

    ClipperLib::EndType endType = ClipperLib::etOpenButt;
    switch (end)
    {
        case END_OPENBUTT:
            endType = ClipperLib::etOpenButt;
            break;

        case END_OPENROUND:
            endType = ClipperLib::etOpenRound;
            break;

        case END_CLOSEDLINE:
            endType = ClipperLib::etClosedLine;
            break;

        case END_OPENSQUARE:
            endType = ClipperLib::etOpenSquare;
            break;

        case END_CLOSEDPOLYGON:
            endType = ClipperLib::etClosedPolygon;
            break;
    }

    float xRes = (float)d()->resPerPixel_ / d()->pixelSize_[0];
    float yRes = (float)d()->resPerPixel_ / d()->pixelSize_[1];

    auto resultPaths = pmr::make_shared<ClipperLib::PolyTree>(
            d()->getResource(), d()->getResource());
    ClipperLib::ClipperOffset clipperOffset(d()->getResource());
    pmr::vector<ClipperLib::Path> inPaths(d()->getResource());
    ClipperLib::PolyTreeToPaths(*d()->paths_, inPaths);

    clipperOffset.AddPaths(inPaths, joinType, endType);
    clipperOffset.Execute(*resultPaths, offset * std::max(xRes, yRes));

    Region result(d()->getResource());
    result.d()->paths_ = std::move(resultPaths);
    result.d()->pixelSize_ = d()->pixelSize_;
    result.d()->resPerPixel_ = d()->resPerPixel_;

    return result;
}

std::pair<pmr::vector<Vector2f>, pmr::vector<uint16_t> >
    Region::triangulate(pmr::memory_resource* memory) const
{
    if (!d())
        return {pmr::vector<Vector2f>(memory), pmr::vector<uint16_t>(memory)};

    size_t pointCount = 0;
    ClipperLib::PolyNode* node = d()->paths_->GetFirst();
    while (node)
    {
        pointCount += node->Contour.size();
        node = node->GetNext();
    }

    if (pointCount == 0)
        return {pmr::vector<Vector2f>(memory), pmr::vector<uint16_t>(memory)};

    pmr::monotonic_buffer_resource mono(memory);

    const float xRes = (float)d()->resPerPixel_ / d()->pixelSize_[0];
    const float yRes = (float)d()->resPerPixel_ / d()->pixelSize_[1];

    pmr::queue<ClipperLib::PolyNode*> stack(&mono);
    for (auto const& child : d()->paths_->Childs)
        stack.push(child);

    node = stack.front();
    stack.pop();

    pmr::vector<uint16_t> indices(memory);
    pmr::vector<Vector2f> vertices(memory);
    //DBG("Top level paths: %1", d()->paths_.Childs.size());
    while (node)
    {
        assert(!node->IsHole());
        assert(ClipperLib::Orientation(node->Contour)
                && "Wrong orientation");

        //points.clear();
        pmr::vector<p2t::Point*> polyline(&mono);
        assert(node->Contour.front() != node->Contour.back());

        size_t pointCount = node->Contour.size();
        for (auto const& child : node->Childs)
            pointCount += child->Contour.size();

        pmr::vector<p2t::Point> points(&mono);
        points.reserve(pointCount);

        for (auto const& pt : node->Contour)
        {
            points.push_back(p2t::Point(
                        memory_,
                        (float)pt.X / xRes,
                        (float)pt.Y / yRes));
            polyline.push_back(&points.back());
        }

        /*DBG("Polyline size: %1, children: %2", polyline.size(),
                node->ChildCount());*/
        p2t::CDT cdt(polyline);

        for (auto const& child : node->Childs)
        {
            polyline.clear();
            //assert(child->IsHole());
            //assert(!ClipperLib::Orientation(child->Contour)
                    //&& "Wrong orientation");
            for (auto const& pt : child->Contour)
            {
                assert(child->Contour.front() != child->Contour.back());
                points.push_back(p2t::Point(
                            &mono,
                            (float)pt.X / xRes,
                            (float)pt.Y / yRes));
                polyline.push_back(&points.back());
            }

            //DBG("hole %1", polyline.size());
            cdt.AddHole(polyline);

            for (auto const& grandChild : child->Childs)
                stack.push(grandChild);
        }

        //assert(nextPoint <= points.size() && "Too many points");
        cdt.Triangulate();

        pmr::vector<p2t::Triangle*> const& triangles = cdt.GetTriangles();

        for (auto const& triangle : triangles)
        {
            for (int l = 0; l < 3; ++l)
            {
                /*size_t index = (size_t)((char*)(*k)->GetPoint(l)
                        - (char*)points.data()) / sizeof(p2t::Point);
                indices.push_back(index);*/

#if 0
                p2t::Point& p = *(*k)->GetPoint(l);
                p2t::Point& p2 = *(*k)->GetPoint((l+1)%3);

                //if (!(*k)->constrained_edge[(*k)->EdgeIndex(&p, &p2)])
                    //continue;

                vertices.push_back(Vector2f(p.x, p.y));
                vertices.push_back(Vector2f(p2.x, p2.y));
#else
                p2t::Point& p = *(triangle)->GetPoint(l);
                vertices.push_back(Vector2f(p.x, p.y));
#endif
            }
        }

        if (stack.empty())
            break;

        node = stack.front();
        stack.pop();
    }

    return std::make_pair(std::move(vertices), std::move(indices));
}

Region Region::getClipped(Rect const& r) const
{
    float xRes = (float)d()->resPerPixel_ / d()->pixelSize_[0];
    float yRes = (float)d()->resPerPixel_ / d()->pixelSize_[1];

    SimplePolygon rectPoly(pmr::vector<Vector2i>({
            { (long)(r.getLeft() * xRes), (long)(r.getBottom() * yRes ) },
            { (long)(r.getRight() * xRes), (long)(r.getBottom() * yRes) },
            { (long)(r.getRight() * xRes), (long)(r.getTop() * yRes) },
            { (long)(r.getLeft() * xRes), (long)(r.getTop() * yRes) }
            }, memory_));

    Region rectRegion(
            memory_,
            pmr::vector<SimplePolygon>({rectPoly}, memory_),
            FILL_EVENODD, d()->pixelSize_,
            d()->resPerPixel_
            );

    return *this & rectRegion;
}

void Region::ensureUniqueness()
{
    if (deferred_.unique())
        return;

    deferred_ = pmr::make_shared<RegionDeferred>(memory_, *d());
}

std::ostream& operator<<(std::ostream& stream, avg::Region const& region)
{
    printClipperPolyTree(stream, *region.d()->paths_, region.d()->resPerPixel_,
            region.d()->pixelSize_);

    return stream;
}

} // namespace


