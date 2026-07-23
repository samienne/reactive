#include <avg/path.h>
#include <avg/pathbuilder.h>
#include <avg/fillrule.h>
#include <avg/transform.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace avg;

namespace
{

pmr::memory_resource* res()
{
    return pmr::new_delete_resource();
}

Path square()
{
    return PathBuilder(res())
        .start(0.0f, 0.0f)
        .lineTo(10.0f, 0.0f)
        .lineTo(10.0f, 10.0f)
        .lineTo(0.0f, 10.0f)
        .close()
        .build();
}

Path diamond()
{
    return PathBuilder(res())
        .start(5.0f, 0.0f)
        .lineTo(10.0f, 5.0f)
        .lineTo(5.0f, 10.0f)
        .lineTo(0.0f, 5.0f)
        .close()
        .build();
}

// A dome whose curved top reaches its apex exactly at (5, 5).
Path conicDome()
{
    return PathBuilder(res())
        .start(0.0f, 0.0f)
        .conicTo(Vector2f(5.0f, 10.0f), Vector2f(10.0f, 0.0f))
        .close()
        .build();
}

std::vector<PathCrossing> sortedByX(std::vector<PathCrossing> crossings)
{
    std::sort(crossings.begin(), crossings.end(),
            [](PathCrossing const& a, PathCrossing const& b)
            {
                return a.point[0] < b.point[0];
            });
    return crossings;
}

} // namespace

TEST(PathCrossing, horizontalLineThroughSquare)
{
    auto crossings = sortedByX(
            square().lineCrossings(Vector2f(5.0f, 5.0f),
                Vector2f(1.0f, 0.0f)));

    ASSERT_EQ(2u, crossings.size());

    EXPECT_NEAR(0.0f, crossings[0].point[0], 1e-3f);
    EXPECT_NEAR(5.0f, crossings[0].point[1], 1e-3f);
    EXPECT_NEAR(10.0f, crossings[1].point[0], 1e-3f);
    EXPECT_NEAR(5.0f, crossings[1].point[1], 1e-3f);

    // The left crossing lies behind the point, the right one ahead.
    EXPECT_NEAR(-5.0f, crossings[0].lineParam, 1e-3f);
    EXPECT_NEAR(5.0f, crossings[1].lineParam, 1e-3f);

    // Opposite edges of a loop cross in opposite senses.
    EXPECT_EQ(1, crossings[0].windingSign);
    EXPECT_EQ(-1, crossings[1].windingSign);
}

TEST(PathCrossing, verticalLineThroughSquare)
{
    auto crossings = square().lineCrossings(Vector2f(5.0f, 5.0f),
            Vector2f(0.0f, 1.0f));

    EXPECT_EQ(2u, crossings.size());
}

TEST(PathCrossing, diagonalLineThroughSquare)
{
    auto crossings = sortedByX(
            square().lineCrossings(Vector2f(5.0f, 5.0f),
                Vector2f(2.0f, 1.0f)));

    ASSERT_EQ(2u, crossings.size());

    EXPECT_NEAR(0.0f, crossings[0].point[0], 1e-3f);
    EXPECT_NEAR(2.5f, crossings[0].point[1], 1e-3f);
    EXPECT_NEAR(10.0f, crossings[1].point[0], 1e-3f);
    EXPECT_NEAR(7.5f, crossings[1].point[1], 1e-3f);
}

TEST(PathCrossing, convexPolygonContainment)
{
    Path p = square();

    EXPECT_TRUE(p.contains(Vector2f(5.0f, 5.0f)));
    EXPECT_TRUE(p.contains(Vector2f(1.0f, 9.0f)));
    EXPECT_FALSE(p.contains(Vector2f(15.0f, 5.0f)));
    EXPECT_FALSE(p.contains(Vector2f(-1.0f, 5.0f)));
    EXPECT_FALSE(p.contains(Vector2f(5.0f, 20.0f)));
}

TEST(PathCrossing, fillRuleOrientationOnConvexPolygon)
{
    Path p = square();
    Vector2f c(5.0f, 5.0f);

    EXPECT_TRUE(p.contains(c, FILL_EVENODD));
    EXPECT_TRUE(p.contains(c, FILL_NONZERO));

    // The square is wound so its winding number is negative inside.
    EXPECT_FALSE(p.contains(c, FILL_POSITIVE));
    EXPECT_TRUE(p.contains(c, FILL_NEGATIVE));
}

TEST(PathCrossing, concavePolygonContainment)
{
    // A "U" shape: two upright legs joined by a base, with a notch on top.
    Path u = PathBuilder(res())
        .start(0.0f, 0.0f)
        .lineTo(10.0f, 0.0f)
        .lineTo(10.0f, 10.0f)
        .lineTo(6.0f, 10.0f)
        .lineTo(6.0f, 4.0f)
        .lineTo(4.0f, 4.0f)
        .lineTo(4.0f, 10.0f)
        .lineTo(0.0f, 10.0f)
        .close()
        .build();

    EXPECT_TRUE(u.contains(Vector2f(2.0f, 5.0f)));  // left leg
    EXPECT_TRUE(u.contains(Vector2f(8.0f, 5.0f)));  // right leg
    EXPECT_TRUE(u.contains(Vector2f(5.0f, 2.0f)));  // base
    EXPECT_FALSE(u.contains(Vector2f(5.0f, 7.0f))); // notch
    EXPECT_FALSE(u.contains(Vector2f(12.0f, 5.0f)));
}

TEST(PathCrossing, evenOddAndNonzeroDisagree)
{
    // Two nested squares wound the same way. Even-odd leaves the inner region
    // empty (a hole); nonzero fills it (winding two).
    Path p = PathBuilder(res())
        .start(0.0f, 0.0f)
        .lineTo(10.0f, 0.0f)
        .lineTo(10.0f, 10.0f)
        .lineTo(0.0f, 10.0f)
        .close()
        .start(3.0f, 3.0f)
        .lineTo(7.0f, 3.0f)
        .lineTo(7.0f, 7.0f)
        .lineTo(3.0f, 7.0f)
        .close()
        .build();

    Vector2f center(5.0f, 5.0f);

    EXPECT_FALSE(p.contains(center, FILL_EVENODD));
    EXPECT_TRUE(p.contains(center, FILL_NONZERO));

    // A point in the ring between the squares is inside under both rules.
    Vector2f ring(1.0f, 5.0f);
    EXPECT_TRUE(p.contains(ring, FILL_EVENODD));
    EXPECT_TRUE(p.contains(ring, FILL_NONZERO));
}

TEST(PathCrossing, conicLobeContainment)
{
    Path dome = conicDome();

    EXPECT_TRUE(dome.contains(Vector2f(5.0f, 2.0f)));
    EXPECT_TRUE(dome.contains(Vector2f(5.0f, 4.9f)));
    EXPECT_FALSE(dome.contains(Vector2f(5.0f, 5.1f)));
    EXPECT_FALSE(dome.contains(Vector2f(5.0f, -1.0f)));
}

TEST(PathCrossing, conicCrossingCount)
{
    // A line below the apex cuts the curved top twice.
    auto crossings = conicDome().lineCrossings(Vector2f(0.0f, 2.0f),
            Vector2f(1.0f, 0.0f));

    EXPECT_EQ(2u, crossings.size());
}

TEST(PathCrossing, tangentTouchIsNotCounted)
{
    Path dome = conicDome();

    // The line y = 5 grazes the apex: a double root, no sign change.
    auto grazing = dome.lineCrossings(Vector2f(0.0f, 5.0f),
            Vector2f(1.0f, 0.0f));
    EXPECT_EQ(0u, grazing.size());

    // Just below the apex the line genuinely cuts through, twice.
    auto cutting = dome.lineCrossings(Vector2f(0.0f, 4.9f),
            Vector2f(1.0f, 0.0f));
    EXPECT_EQ(2u, cutting.size());
}

TEST(PathCrossing, cubicLobeContainment)
{
    // A closed teardrop whose curved top peaks around (5, 6).
    Path teardrop = PathBuilder(res())
        .start(0.0f, 0.0f)
        .cubicTo(Vector2f(2.0f, 8.0f), Vector2f(8.0f, 8.0f),
                Vector2f(10.0f, 0.0f))
        .close()
        .build();

    EXPECT_TRUE(teardrop.contains(Vector2f(5.0f, 3.0f)));
    EXPECT_FALSE(teardrop.contains(Vector2f(5.0f, 7.0f)));
    EXPECT_FALSE(teardrop.contains(Vector2f(5.0f, -1.0f)));
}

TEST(PathCrossing, cubicThreeRealRoots)
{
    // A wiggling cubic that a single line crosses three times, exercising the
    // three-real-root branch of the solver. The subpath closes with a straight
    // edge that adds one more crossing, so the loop has four in total.
    Path p = PathBuilder(res())
        .start(0.0f, 0.0f)
        .cubicTo(Vector2f(3.0f, 15.0f), Vector2f(6.0f, -5.0f),
                Vector2f(9.0f, 10.0f))
        .close()
        .build();

    auto crossings = p.lineCrossings(Vector2f(0.0f, 5.0f),
            Vector2f(1.0f, 0.0f));

    ASSERT_EQ(4u, crossings.size());

    std::size_t onCubic = 0;
    for (auto const& c : crossings)
    {
        if (c.segmentIndex == 0)
            ++onCubic;
    }
    EXPECT_EQ(3u, onCubic);
}

TEST(PathCrossing, sharedVertexCountsOnce)
{
    // The line y = 5 passes exactly through the left and right vertices of the
    // diamond. Each shared vertex must be counted once.
    auto crossings = sortedByX(
            diamond().lineCrossings(Vector2f(5.0f, 5.0f),
                Vector2f(1.0f, 0.0f)));

    ASSERT_EQ(2u, crossings.size());
    EXPECT_NEAR(0.0f, crossings[0].point[0], 1e-3f);
    EXPECT_NEAR(10.0f, crossings[1].point[0], 1e-3f);

    EXPECT_TRUE(diamond().contains(Vector2f(5.0f, 5.0f)));
}

TEST(PathCrossing, vertexTouchIsNotCounted)
{
    // The line y = 0 touches only the bottom vertex of the diamond, where both
    // edges rise away from it: a touch, not a crossing.
    auto crossings = diamond().lineCrossings(Vector2f(5.0f, 0.0f),
            Vector2f(1.0f, 0.0f));

    EXPECT_EQ(0u, crossings.size());
}

TEST(PathCrossing, arcNearFullCircle)
{
    // An arc sweeping almost all the way round approximates a disc.
    Path circle = PathBuilder(res())
        .start(5.0f, 0.0f)
        .arc(Vector2f(0.0f, 0.0f), 6.2f)
        .close()
        .build();

    EXPECT_TRUE(circle.contains(Vector2f(0.0f, 0.0f)));
    EXPECT_TRUE(circle.contains(Vector2f(0.0f, 4.0f)));
    EXPECT_FALSE(circle.contains(Vector2f(10.0f, 0.0f)));
    EXPECT_FALSE(circle.contains(Vector2f(0.0f, 6.0f)));
}

TEST(PathCrossing, arcWrapsAngleOrigin)
{
    // The sweep runs from -0.5 rad to +0.5 rad, straddling the angle origin,
    // so the intersection at angle 0 must be recognised as inside the sweep.
    float const r = 5.0f;
    Vector2f start(r * std::cos(-0.5f), r * std::sin(-0.5f));

    Path arc = PathBuilder(res())
        .start(start)
        .arc(Vector2f(0.0f, 0.0f), 1.0f)
        .build();

    auto crossings = sortedByX(
            arc.lineCrossings(Vector2f(0.0f, 0.0f),
                Vector2f(1.0f, 0.0f)));

    // The circle meets the line at angle 0 (inside the sweep) and angle pi
    // (outside it); the implicit closing chord contributes the second hit.
    ASSERT_EQ(2u, crossings.size());
    EXPECT_NEAR(5.0f, crossings[1].point[0], 1e-3f);
    EXPECT_NEAR(0.0f, crossings[1].point[1], 1e-3f);

    EXPECT_TRUE(arc.contains(Vector2f(4.6f, 0.0f)));
    EXPECT_FALSE(arc.contains(Vector2f(4.0f, 0.0f)));
}

TEST(PathCrossing, boundaryConvention)
{
    // A boundary point is classified by the same one-sided ray count. A point
    // whose ray leaves through the far side reads as inside; one whose ray
    // points away from the shape reads as outside.
    Path p = square();

    EXPECT_TRUE(p.contains(Vector2f(5.0f, 0.0f)));   // bottom edge, ray exits
    EXPECT_FALSE(p.contains(Vector2f(10.0f, 5.0f))); // right edge, ray leaves
}

TEST(PathCrossing, emptyPath)
{
    Path p(res());

    EXPECT_TRUE(p.lineCrossings(Vector2f(0.0f, 0.0f),
                Vector2f(1.0f, 0.0f)).empty());
    EXPECT_FALSE(p.contains(Vector2f(0.0f, 0.0f)));
}

TEST(PathCrossing, singlePointPath)
{
    Path p = PathBuilder(res())
        .start(3.0f, 3.0f)
        .build();

    EXPECT_TRUE(p.lineCrossings(Vector2f(0.0f, 0.0f),
                Vector2f(1.0f, 0.0f)).empty());
    EXPECT_FALSE(p.contains(Vector2f(3.0f, 3.0f)));
}

TEST(PathCrossing, degenerateDirection)
{
    EXPECT_TRUE(square().lineCrossings(Vector2f(5.0f, 5.0f),
                Vector2f(0.0f, 0.0f)).empty());
}

TEST(PathCrossing, respectsTransform)
{
    // The crossing runs in resolved space, so a translated square is hit where
    // it visually sits, not at its pre-transform coordinates.
    Path p = avg::translate(Vector2f(100.0f, 0.0f)) * square();

    EXPECT_TRUE(p.contains(Vector2f(105.0f, 5.0f)));
    EXPECT_FALSE(p.contains(Vector2f(5.0f, 5.0f)));

    auto crossings = p.lineCrossings(Vector2f(105.0f, 5.0f),
            Vector2f(1.0f, 0.0f));
    EXPECT_EQ(2u, crossings.size());
}

TEST(PathCrossing, fullCrossingListForKnownLine)
{
    // Assert the complete crossing list for a horizontal line through a square.
    auto crossings = sortedByX(
            square().lineCrossings(Vector2f(5.0f, 5.0f),
                Vector2f(1.0f, 0.0f)));

    ASSERT_EQ(2u, crossings.size());

    EXPECT_NEAR(0.0f, crossings[0].point[0], 1e-3f);
    EXPECT_NEAR(5.0f, crossings[0].point[1], 1e-3f);
    EXPECT_EQ(1, crossings[0].windingSign);

    EXPECT_NEAR(10.0f, crossings[1].point[0], 1e-3f);
    EXPECT_NEAR(5.0f, crossings[1].point[1], 1e-3f);
    EXPECT_EQ(-1, crossings[1].windingSign);

    // Winding signs cancel over a full loop.
    int sum = crossings[0].windingSign + crossings[1].windingSign;
    EXPECT_EQ(0, sum);
}

TEST(PathCrossing, memberLineCrossingsSurface)
{
    // Exercise lineCrossings as a Path member, holding the path in a named
    // value so the call is made on an lvalue Path.
    Path p = square();

    std::vector<PathCrossing> crossings = p.lineCrossings(
            Vector2f(5.0f, 5.0f), Vector2f(1.0f, 0.0f));

    EXPECT_EQ(2u, crossings.size());

    // A degenerate direction still yields no crossings through the member.
    EXPECT_TRUE(
            p.lineCrossings(Vector2f(5.0f, 5.0f), Vector2f(0.0f, 0.0f))
                .empty());
}

TEST(PathCrossing, memberContainsSurface)
{
    // Exercise contains as a Path member with each fill rule reachable.
    Path p = square();

    EXPECT_TRUE(p.contains(Vector2f(5.0f, 5.0f)));
    EXPECT_TRUE(p.contains(Vector2f(5.0f, 5.0f), FILL_EVENODD));
    EXPECT_TRUE(p.contains(Vector2f(5.0f, 5.0f), FILL_NONZERO));
    EXPECT_FALSE(p.contains(Vector2f(15.0f, 5.0f)));
}
