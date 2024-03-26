#include "region.h"

#include "polyline.h"
#include "rect.h"

#include "clipper/clipper.hpp"

#include <mapbox/earcut.hpp>

#include <pmr/vector.h>
#include <pmr/shared_ptr.h>
#include <pmr/queue.h>
#include <pmr/new_delete_resource.h>
#include <pmr/monotonic_buffer_resource.h>

#include <stdexcept>
#include <memory>

namespace mapbox::util {

template <>
struct nth<0, ClipperLib::IntPoint> {
    inline static auto get(const ClipperLib::IntPoint &t) {
        return t.x();
    };
};
template <>
struct nth<1, ClipperLib::IntPoint> {
    inline static auto get(const ClipperLib::IntPoint &t) {
        return t.y();
    };
};
} // namespace mapbox::util

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
    std::shared_ptr<ClipperLib::PolyTree> polyTree_;
    Rect boundingBox_;
    Vector2f pointSize_;
};

namespace
{
    /**
     * @brief Prints the polytree contours in javascript clipper form.
     */
    void printClipperPolyTree(std::ostream& stream,
            ClipperLib::PolyTree const& polyTree, Vector2f pointSize)
    {

        pmr::monotonic_buffer_resource memory(pmr::new_delete_resource());
        pmr::vector<ClipperLib::Path> paths(&memory);
        ClipperLib::PolyTreeToPaths(polyTree, paths);

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
                stream << (float)j->x() * pointSize[0] << ", "
                    << (float)j->y() * pointSize[1];
            }
            stream << "]";
        }
        stream << "]" << std::endl;
    }

    ClipperLib::Path polyLineToClipperPath(pmr::memory_resource* memory,
            PolyLine const& polygon)
    {
        pmr::vector<Vector2i> const& vertices = polygon.getVertices();
        ClipperLib::Path path(memory);

        for (auto const& vertex : vertices)
            path.push_back(ClipperLib::IntPoint(vertex[0], vertex[1]));

        return path;
    }

    pmr::vector<ClipperLib::Path> fillPaths(
            pmr::memory_resource* memory,
            pmr::vector<PolyLine> const& polygons)
    {
        pmr::vector<ClipperLib::Path> result(memory);
        result.reserve(polygons.size());
        for (auto const& polygon : polygons)
        {
            result.push_back(polyLineToClipperPath(memory, polygon));
        }

        return result;
    }

    Rect calculateBoundingBoxForPolyTree(ClipperLib::PolyTree const& tree,
            Vector2f pointSize)
    {
        Rect r;

        for (ClipperLib::PolyNode const* child : tree.Childs)
        {
            for (ClipperLib::IntPoint p : child->Contour)
            {
                r = r.include(Vector2f(
                        static_cast<float>(p.x()) * pointSize[0],
                        static_cast<float>(p.y()) * pointSize[1]
                        ));
            }
        }

        return r;
    }

    pmr::vector<ClipperLib::Path> polyTreeToPaths(
            pmr::memory_resource* memory,
            ClipperLib::PolyTree const& polyTree)
    {
        pmr::vector<ClipperLib::Path> result(memory);
        ClipperLib::PolyTreeToPaths(polyTree, result);

        return result;
    }

    ClipperLib::PolyFillType fillRuleToFillType(FillRule rule)
    {
        switch (rule)
        {
            case FILL_EVENODD:
                return ClipperLib::pftEvenOdd;
                break;
            case FILL_NONZERO:
                return ClipperLib::pftNonZero;
                break;
            case FILL_POSITIVE:
                return ClipperLib::pftPositive;
                break;
            case FILL_NEGATIVE:
                return ClipperLib::pftNegative;
                break;
            default:
                assert(false && "Uknown fill rule");
        }

        return ClipperLib::pftEvenOdd;
    }

    ClipperLib::JoinType joinTypeToClipperType(JoinType join)
    {
        switch (join)
        {
            case JOIN_MITER:
                return ClipperLib::jtMiter;
                break;

            case JOIN_ROUND:
                return ClipperLib::jtRound;
                break;

            case JOIN_SQUARE:
                return ClipperLib::jtSquare;
                break;
        }

        assert(false && "unknown join type");
        return ClipperLib::jtMiter;
    }

    ClipperLib::EndType endTypeToClipperType(EndType end)
    {
        switch (end)
        {
            case END_OPENBUTT:
                return ClipperLib::etOpenButt;
                break;

            case END_OPENROUND:
                return ClipperLib::etOpenRound;
                break;

            case END_CLOSEDLINE:
                return ClipperLib::etClosedLine;
                break;

            case END_OPENSQUARE:
                return ClipperLib::etOpenSquare;
                break;

            case END_CLOSEDPOLYGON:
                return ClipperLib::etClosedPolygon;
                break;
        }

        assert(false && "Unknown end type");
        return ClipperLib::etOpenButt;
    }

    ClipperLib::ClipType operationToClipType(OperationType operation)
    {
        switch (operation) {
        case OperationType::add:
            return ClipperLib::ctUnion;

        case OperationType::intersect:
            return ClipperLib::ctIntersection;

        case OperationType::subtract:
            return ClipperLib::ctDifference;
        }

        assert(false);
        return ClipperLib::ctUnion;
    }

    std::shared_ptr<ClipperLib::PolyTree> operatePaths(
            pmr::memory_resource* memory,
            OperationType operation,
            ClipperLib::Paths const& lhs,
            ClipperLib::Paths const& rhs,
            FillRule rule)
    {
        auto polyTree = pmr::make_shared<ClipperLib::PolyTree>(memory, memory);

        if (lhs.empty() && rhs.empty())
            return polyTree;

        ClipperLib::PolyFillType type = fillRuleToFillType(rule);
        ClipperLib::ClipType clipType = operationToClipType(operation);

        ClipperLib::Clipper c(memory);
        c.StrictlySimple(true);
        c.PreserveCollinear(false);
        c.AddPaths(lhs, ClipperLib::ptSubject, true);
        c.AddPaths(rhs, ClipperLib::ptClip, true);

        c.Execute(clipType, *polyTree, type, type);

        return polyTree;
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
    memory_(memory),
    deferred_(pmr::make_shared<RegionDeferred>(memory, memory))
{
}

Region::Region(pmr::memory_resource* memory,
        pmr::vector<PolyLine> const& polygons, FillRule rule,
        Vector2f pointSize) :
    memory_(memory),
    deferred_(pmr::make_shared<RegionDeferred>(memory, memory))
{
    if (polygons.empty())
        return;


    d()->pointSize_ = pointSize;
    d()->polyTree_ = operatePaths(memory, OperationType::add,
            fillPaths(memory, polygons), ClipperLib::Paths(memory), rule);

    d()->boundingBox_ = calculateBoundingBoxForPolyTree(
            *d()->polyTree_,
            d()->pointSize_
            );
}

Region::Region(pmr::memory_resource* memory,
        pmr::vector<PolyLine> const& polygons, JoinType join,
        EndType end, float width, Vector2f pointSize) :
    memory_(memory)
{
    if (polygons.empty())
        return;

    deferred_ = pmr::make_shared<RegionDeferred>(memory, memory);

    ClipperLib::JoinType joinType = joinTypeToClipperType(join);
    ClipperLib::EndType endType = endTypeToClipperType(end);

    d()->pointSize_ = pointSize;

    auto resultPaths = pmr::make_shared<ClipperLib::PolyTree>(memory, memory);
    ClipperLib::ClipperOffset clipperOffset(memory);
    pmr::vector<ClipperLib::Path> paths = fillPaths(memory, polygons);

    clipperOffset.AddPaths(paths, joinType, endType);
    clipperOffset.Execute(*resultPaths, width / d()->pointSize_[0]);

    d()->polyTree_ = std::move(resultPaths);

    d()->boundingBox_ = calculateBoundingBoxForPolyTree(
            *d()->polyTree_,
            d()->pointSize_
            );
}

Region::Region(pmr::memory_resource* memory,
        OperationType operation,
        pmr::vector<PolyLine> const& lhs,
        pmr::vector<PolyLine> const& rhs,
        FillRule rule,
        Vector2f pointSize) :
    memory_(memory),
    deferred_(pmr::make_shared<RegionDeferred>(memory, memory))
{
    if (lhs.empty() && rhs.empty())
        return;

    deferred_ = pmr::make_shared<RegionDeferred>(memory, memory);

    d()->pointSize_ = pointSize;
    d()->polyTree_ = operatePaths(memory, operation, fillPaths(memory, lhs),
            fillPaths(memory, rhs), rule);

    d()->boundingBox_ = calculateBoundingBoxForPolyTree(
            *d()->polyTree_,
            d()->pointSize_
            );
}

Region::~Region()
{
}

ClipperLib::Paths adjustPointSize(Vector2f oldSize, Vector2f newSize,
        ClipperLib::Paths paths)
{
    if (oldSize == newSize) {
        return paths;
    }

    assert(false);
    return paths;
}

Region Region::operator|(Region const& rhs) const
{
    return operateRegions(d()->memory_, *this, rhs, OperationType::add);
}

Region Region::operator&(Region const& rhs) const
{
    return operateRegions(d()->memory_, *this, rhs, OperationType::intersect);
}

Region Region::operator-(Region const& rhs) const
{
    return operateRegions(d()->memory_, *this, rhs, OperationType::subtract);
}

Region Region::operator^(Region const& /*rhs*/) const
{
    throw std::runtime_error("Unimplemented");
}

Region& Region::operator|=(Region const& rhs)
{
    *this = *this | rhs;
    return *this;
}

Region& Region::operator&=(Region const& rhs)
{
    *this = *this & rhs;
    return *this;
}

Region& Region::operator-=(Region const& rhs)
{
    *this = *this - rhs;
    return *this;
}

Region& Region::operator^=(Region const& rhs)
{
    *this = *this ^ rhs;
    return *this;
}

Region Region::offset(JoinType join, EndType end, float offset) const
{
    ClipperLib::JoinType joinType = joinTypeToClipperType(join);
    ClipperLib::EndType endType = endTypeToClipperType(end);

    auto resultPaths = pmr::make_shared<ClipperLib::PolyTree>(
            d()->getResource(), d()->getResource());
    ClipperLib::ClipperOffset clipperOffset(d()->getResource());
    pmr::vector<ClipperLib::Path> inPaths(d()->getResource());
    ClipperLib::PolyTreeToPaths(*d()->polyTree_, inPaths);

    clipperOffset.AddPaths(inPaths, joinType, endType);
    clipperOffset.Execute(*resultPaths, offset / d()->pointSize_[0]);

    Region result(d()->getResource());
    result.d()->polyTree_ = std::move(resultPaths);
    result.d()->pointSize_ = d()->pointSize_;
    result.d()->boundingBox_ = calculateBoundingBoxForPolyTree(
            *d()->polyTree_,
            d()->pointSize_
            );

    return result;
}

std::pair<pmr::vector<Vector2f>, pmr::vector<uint32_t> >
    Region::triangulate(pmr::memory_resource* memory) const
{
    if (!d() || !d()->polyTree_)
        return {pmr::vector<Vector2f>(memory), pmr::vector<uint32_t>(memory)};

    size_t pointCount = 0;
    ClipperLib::PolyNode* node = d()->polyTree_->GetFirst();
    while (node)
    {
        pointCount += node->Contour.size();
        node = node->GetNext();
    }

    if (pointCount == 0)
        return {pmr::vector<Vector2f>(memory), pmr::vector<uint32_t>(memory)};

    pmr::monotonic_buffer_resource mono(memory);

    pmr::queue<ClipperLib::PolyNode*> stack(&mono);
    for (auto const& child : d()->polyTree_->Childs)
        stack.push(child);

    node = stack.front();
    stack.pop();

    pmr::vector<uint32_t> indices(memory);
    pmr::vector<Vector2f> vertices(memory);

    while (node)
    {
        assert(!node->IsHole());
        assert(ClipperLib::Orientation(node->Contour)
                && "Wrong orientation");

        assert(node->Contour.front() != node->Contour.back());

        size_t pointCount = node->Contour.size();
        for (auto const& child : node->Childs)
            pointCount += child->Contour.size();

        pmr::vector<pmr::vector<ClipperLib::IntPoint>> polygons(&mono);
        polygons.push_back(node->Contour);

        for (auto const& child : node->Childs)
        {
            polygons.push_back(child->Contour);

            for (auto const& grandChild : child->Childs)
                stack.push(grandChild);
        }

        std::vector<uint32_t> triangles = mapbox::earcut(polygons);

        size_t offset = vertices.size();
        for (auto const& polygon : polygons)
        {
            for (auto const& vertex : polygon)
            {
                vertices.emplace_back(
                        (float)vertex.x() * d()->pointSize_[0],
                        (float)vertex.y() * d()->pointSize_[1]
                        );
            }
        }

        for (auto index : triangles)
            indices.push_back(index + offset);

        if (stack.empty())
            break;

        node = stack.front();
        stack.pop();
    }

    return std::make_pair(std::move(vertices), std::move(indices));
}

Region Region::getClipped(Rect const& r) const
{
    float x = d()->pointSize_[0];
    float y = d()->pointSize_[1];

    PolyLine rectPoly(pmr::vector<Vector2i>({
            { (long)(r.getLeft() / x), (long)(r.getBottom() / y ) },
            { (long)(r.getRight() / x), (long)(r.getBottom() / y) },
            { (long)(r.getRight() / x), (long)(r.getTop() / y) },
            { (long)(r.getLeft() / x), (long)(r.getTop() / y) }
            }, memory_));

    Region rectRegion(
            memory_,
            pmr::vector<PolyLine>({rectPoly}, memory_),
            FILL_EVENODD, d()->pointSize_
            );

    return *this & rectRegion;
}

Rect Region::getBoundingBox() const
{
    return d()->boundingBox_;
}

pmr::memory_resource* Region::getResource() const
{
    return memory_;
}

Region Region::withResource(pmr::memory_resource* memory) const
{
    Region result(memory);

    result.d()->polyTree_ = pmr::make_shared<ClipperLib::PolyTree>(memory, memory);
    result.d()->pointSize_ = d()->pointSize_;
    result.d()->boundingBox_ = d()->boundingBox_;

    return result;
}

void Region::ensureUniqueness()
{
    if (deferred_.unique())
        return;

    deferred_ = pmr::make_shared<RegionDeferred>(memory_, *d());
}

Region operator*(avg::Transform const& t, Region&& r)
{
    ClipperLib::PolyNode* node = r.d()->polyTree_.get();

    while (node)
    {
        for (ClipperLib::IntPoint& point : node->Contour)
        {
            Vector2f v(
                    static_cast<float>(point.x()) * r.d()->pointSize_[0],
                    static_cast<float>(point.y()) * r.d()->pointSize_[1]
                    );

            v = t * v;

            point.x() = (long long)(v.x() / r.d()->pointSize_[0]);
            point.y() = (long long)(v.y() / r.d()->pointSize_[1]);
        }

        node = node->GetNext();
    }

    r.d()->boundingBox_ = calculateBoundingBoxForPolyTree(
            *r.d()->polyTree_,
            r.d()->pointSize_
            );

    return std::move(r);
}

Region Region::operateRegions(
        pmr::memory_resource* memory,
        Region const& lhs,
        Region const& rhs,
        OperationType operation)
{
    auto result = Region(memory);

    result.d()->pointSize_ = lhs.d()->pointSize_;
    result.d()->polyTree_ = operatePaths(
            memory,
            operation,
            polyTreeToPaths(memory, *lhs.d()->polyTree_),
            adjustPointSize(
                lhs.d()->pointSize_,
                rhs.d()->pointSize_,
                polyTreeToPaths(memory, *rhs.d()->polyTree_)
                ),
            FillRule::FILL_EVENODD
            );

    result.d()->boundingBox_ = calculateBoundingBoxForPolyTree(
            *result.d()->polyTree_,
            result.d()->pointSize_
            );

    return result;
}

std::ostream& operator<<(std::ostream& stream, avg::Region const& region)
{
    printClipperPolyTree(stream, *region.d()->polyTree_, region.d()->pointSize_);

    return stream;
}

} // namespace avg


