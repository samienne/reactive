#include "region.h"

#include "simplepolygon.h"
#include "rect.h"

#include "poly2tri/poly2tri.h"
#include "clipper/clipper.hpp"

#include "debug.h"

#include <stack>
#include <queue>
#include <stdexcept>
#include <memory>

namespace avg
{

class RegionDeferred
{
public:
    RegionDeferred();
    RegionDeferred(RegionDeferred const&) = default;
    RegionDeferred(RegionDeferred&&) = default;
    ~RegionDeferred();

public:
    std::shared_ptr<ClipperLib::PolyTree> paths_;
    Vector2f pixelSize_;
    size_t resPerPixel_;
};

namespace
{
    /**
     * @brief Prints the polytree contours in javascript clipper form.
     */
    void printClipperPolyTree(std::ostream& stream,
            ClipperLib::PolyTree const& polyTree,
            int resPerPixel, Vector2f pixelSize)
    {

        std::vector<ClipperLib::Path> paths;
        ClipperLib::PolyTreeToPaths(polyTree, paths);

        float xRes = (float)resPerPixel / pixelSize[0];
        float yRes = (float)resPerPixel / pixelSize[1];

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

    ClipperLib::Path toPath(SimplePolygon const& polygon)
    {
        std::vector<Vector2i> const& vertices = polygon.getVertices();
        ClipperLib::Path path;
        for (auto const& vertex : vertices)
            path.push_back(ClipperLib::IntPoint(vertex[0], vertex[1]));

        return path;
    }

    std::vector<ClipperLib::Path> fillPaths(
            std::vector<SimplePolygon> const& polygons)

    {
        std::vector<ClipperLib::Path> result;
        result.reserve(polygons.size());
        for (auto const& polygon : polygons)
        {
            ClipperLib::Path path = toPath(polygon);
            result.push_back(path);
        }

        return result;
    }
} // anonymous namespace

RegionDeferred::RegionDeferred()
{
}

RegionDeferred::~RegionDeferred()
{
}

Region::Region()
{
}

Region::Region(std::vector<SimplePolygon> const& polygons, FillRule rule,
        Vector2f pixelSize, size_t resPerPixel)
{
    if (polygons.empty())
        return;

    deferred_ = std::make_shared<RegionDeferred>();
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

    std::vector<ClipperLib::Path> paths = fillPaths(polygons);

    //ClipperLib::SimplifyPolygons(paths, d()->paths_, type);
    ClipperLib::Clipper c;
    c.StrictlySimple(true);
    c.PreserveCollinear(false);
    c.AddPaths(paths, ClipperLib::ptSubject, true);
    d()->paths_ = std::make_shared<ClipperLib::PolyTree>();
    c.Execute(ClipperLib::ctUnion, *d()->paths_, type, type);

    //printClipperPolyTree(d()->paths_);

    /*auto n = d()->paths_->GetFirst();
    while (n)
    {
        assert(ClipperLib::Orientation(n->Contour) != n->IsHole());
        n = n->GetNext();
    }*/
}

Region::Region(std::vector<SimplePolygon> const& polygons, JoinType join,
        EndType end, float width, Vector2f pixelSize,
        size_t resPerPixel)
{
    if (polygons.empty())
        return;

    deferred_ = std::make_shared<RegionDeferred>();

    ClipperLib::JoinType joinType;
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
        default:
            assert(false && "Unknown jointype");
    }

    ClipperLib::EndType endType;
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
        default:
            assert(false && "Unknown end type");
    }

    d()->pixelSize_ = pixelSize;
    d()->resPerPixel_ = resPerPixel;

    float xRes = (float)d()->resPerPixel_ / d()->pixelSize_[0];
    //float yRes = (float)d()->resPerPixel_ / d()->pixelSize_[1];

    auto resultPaths = std::make_shared<ClipperLib::PolyTree>();
    ClipperLib::ClipperOffset clipperOffset;
    std::vector<ClipperLib::Path> paths = fillPaths(polygons);

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
    std::vector<ClipperLib::Path> subjPaths;
    ClipperLib::PolyTreeToPaths(*d()->paths_, subjPaths);

    std::vector<ClipperLib::Path> clipPaths;
    ClipperLib::PolyTreeToPaths(*region.d()->paths_, clipPaths);

    ClipperLib::Clipper c;
    c.StrictlySimple(true);
    c.PreserveCollinear(false);
    c.AddPaths(subjPaths, ClipperLib::ptSubject, true);
    c.AddPaths(clipPaths, ClipperLib::ptClip, true);

    std::shared_ptr<ClipperLib::PolyTree> tree =
        std::make_shared<ClipperLib::PolyTree>();

    c.Execute(ClipperLib::ctIntersection, *tree);

    Region result;

    result.deferred_ = std::make_shared<RegionDeferred>();
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
    ClipperLib::JoinType joinType;
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
        default:
            assert(false && "Unknown jointype");
    }

    ClipperLib::EndType endType;
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
        default:
            assert(false && "Unknown end type");
    }

    float xRes = (float)d()->resPerPixel_ / d()->pixelSize_[0];
    float yRes = (float)d()->resPerPixel_ / d()->pixelSize_[1];

    //std::vector<ClipperLib::Path> resultPaths;
    auto resultPaths = std::make_shared<ClipperLib::PolyTree>();
    ClipperLib::ClipperOffset clipperOffset;
    std::vector<ClipperLib::Path> inPaths;
    ClipperLib::PolyTreeToPaths(*d()->paths_, inPaths);

    clipperOffset.AddPaths(inPaths, joinType, endType);
    clipperOffset.Execute(*resultPaths, offset * std::max(xRes, yRes));

    Region result;
    result.d()->paths_ = std::move(resultPaths);
    result.d()->pixelSize_ = d()->pixelSize_;
    result.d()->resPerPixel_ = d()->resPerPixel_;
    return result;
}

std::pair<std::vector<Vector2f>, std::vector<uint16_t> >
    Region::triangulate() const
{
    if (!d())
        return {{}, {}};

    //std::vector<ClipperLib::Path> const& paths = d()->paths_;

    //size_t pointCount = d()->fillPaths(paths, d()->polygons_);
    size_t pointCount = 0;
    ClipperLib::PolyNode* node = d()->paths_->GetFirst();
    while (node)
    {
        pointCount += node->Contour.size();
        node = node->GetNext();
    }

    if (pointCount == 0)
        return {{}, {}};

    const float xRes = (float)d()->resPerPixel_ / d()->pixelSize_[0];
    const float yRes = (float)d()->resPerPixel_ / d()->pixelSize_[1];

    std::queue<ClipperLib::PolyNode*> stack;
    std::vector<uint16_t> indices;
    for (auto const& child : d()->paths_->Childs)
        stack.push(child);

    node = stack.front();
    stack.pop();

    std::vector<Vector2f> vertices;
    //DBG("Top level paths: %1", d()->paths_.Childs.size());
    while (node)
    {
        assert(!node->IsHole());
        assert(ClipperLib::Orientation(node->Contour)
                && "Wrong orientation");

        //points.clear();
        std::vector<p2t::Point*> polyline;
        assert(node->Contour.front() != node->Contour.back());

        size_t pointCount = node->Contour.size();
        for (auto const& child : node->Childs)
            pointCount += child->Contour.size();

        std::vector<p2t::Point> points;
        points.reserve(pointCount);

        for (auto const& pt : node->Contour)
        {
            points.push_back(p2t::Point((float)pt.X / xRes,
                        (float)pt.Y / yRes));
            polyline.push_back(&points.back());
        }

        /*DBG("Polyline size: %1, children: %2", polyline.size(),
                node->ChildCount());*/
        p2t::CDT cdt(polyline);

        for (auto const& child : node->Childs)
        {
            polyline.clear();
            assert(child->IsHole());
            assert(!ClipperLib::Orientation(child->Contour)
                    && "Wrong orientation");
            for (auto const& pt : child->Contour)
            {
                assert(child->Contour.front() != child->Contour.back());
                points.push_back(p2t::Point((float)pt.X / xRes,
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

        std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();

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

    SimplePolygon rectPoly({
            { (long)(r.getLeft() * xRes), (long)(r.getBottom() * yRes ) },
            { (long)(r.getRight() * xRes), (long)(r.getBottom() * yRes) },
            { (long)(r.getRight() * xRes), (long)(r.getTop() * yRes) },
            { (long)(r.getLeft() * xRes), (long)(r.getTop() * yRes) }
            });

    Region rectRegion({rectPoly}, FILL_EVENODD, d()->pixelSize_, d()->resPerPixel_);

    return *this & rectRegion;
}

void Region::ensureUniqueness()
{
    if (deferred_.unique())
        return;

    deferred_ = std::make_shared<RegionDeferred>(*d());
}

std::ostream& operator<<(std::ostream& stream, avg::Region const& region)
{
    printClipperPolyTree(stream, *region.d()->paths_, region.d()->resPerPixel_,
            region.d()->pixelSize_);

    return stream;
}

} // namespace


