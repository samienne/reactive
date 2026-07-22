#include "pathcrossing.h"

#include "path.h"
#include "transform.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace avg
{

namespace
{

float const pi = 3.14159265358979323846f;

/** @brief Rotate a vector by ninety degrees counter-clockwise. */
Vector2f perp(Vector2f v)
{
    return Vector2f(-v[1], v[0]);
}

float cross(Vector2f a, Vector2f b)
{
    return a[0] * b[1] - a[1] * b[0];
}

/**
 * @brief Newton-polishes a root of a polynomial given by its coefficients.
 *
 * coeffs are in descending degree order. A handful of iterations pulls a root
 * from a closed-form solver onto the true root, which is what keeps the cubic
 * solver accurate near the multiple-root boundary where the closed form is
 * ill-conditioned.
 */
double polishRoot(double const* coeffs, int degree, double x)
{
    for (int iter = 0; iter < 8; ++iter)
    {
        double value = coeffs[0];
        double deriv = 0.0;
        for (int i = 1; i <= degree; ++i)
        {
            deriv = deriv * x + value;
            value = value * x + coeffs[i];
        }

        if (std::abs(deriv) < 1e-18)
            break;

        double step = value / deriv;
        x -= step;

        if (std::abs(step) < 1e-12)
            break;
    }

    return x;
}

/** @brief Appends the distinct real roots, collapsing near-equal duplicates. */
void addRoot(std::vector<double>& roots, double r)
{
    for (double existing : roots)
    {
        if (std::abs(existing - r) < 1e-4)
            return;
    }

    roots.push_back(r);
}

/** @brief Solves a t + b = 0. */
void solveLinear(double a, double b, std::vector<double>& roots)
{
    if (std::abs(a) < 1e-12)
        return;

    addRoot(roots, -b / a);
}

/** @brief Solves a t^2 + b t + c = 0 for real roots. */
void solveQuadratic(double a, double b, double c, std::vector<double>& roots)
{
    if (std::abs(a) < 1e-12)
    {
        solveLinear(b, c, roots);
        return;
    }

    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0)
        return;

    double sq = std::sqrt(disc);

    // The numerically stable quadratic formula avoids cancellation.
    double q = -0.5 * (b + std::copysign(sq, b));
    addRoot(roots, q / a);
    if (std::abs(q) > 1e-18)
        addRoot(roots, c / q);
}

/** @brief Solves a t^3 + b t^2 + c t + d = 0 for real roots in [0, 1). */
void solveCubic(double a, double b, double c, double d,
        std::vector<double>& roots)
{
    if (std::abs(a) < 1e-12)
    {
        solveQuadratic(b, c, d, roots);
        return;
    }

    double coeffs[4] = { a, b, c, d };

    double bn = b / a;
    double cn = c / a;
    double dn = d / a;

    // Depress to y^3 + p y + q with t = y - bn/3.
    double p = cn - bn * bn / 3.0;
    double q = 2.0 * bn * bn * bn / 27.0 - bn * cn / 3.0 + dn;
    double shift = bn / 3.0;

    double disc = q * q / 4.0 + p * p * p / 27.0;

    std::vector<double> raw;

    if (disc > 1e-12)
    {
        double sq = std::sqrt(disc);
        double u = std::cbrt(-q / 2.0 + sq);
        double v = std::cbrt(-q / 2.0 - sq);
        raw.push_back(u + v - shift);
    }
    else if (disc < -1e-12)
    {
        double r = 2.0 * std::sqrt(-p / 3.0);
        double phi = std::acos(
                std::max(-1.0, std::min(1.0, 3.0 * q / (p * r))));
        for (int k = 0; k < 3; ++k)
            raw.push_back(
                    r * std::cos((phi - 2.0 * pi * k) / 3.0) - shift);
    }
    else
    {
        // The discriminant vanishes, so the cubic has a repeated root.
        double u = std::cbrt(-q / 2.0);
        raw.push_back(2.0 * u - shift);
        raw.push_back(-u - shift);
    }

    for (double r : raw)
        addRoot(roots, polishRoot(coeffs, 3, r));
}

enum class SegKind
{
    line,
    conic,
    cubic,
    arc
};

/** @brief A single path segment resolved into world space. */
struct Seg
{
    SegKind kind;

    // Bezier control points: line uses p[0..1], conic p[0..2], cubic p[0..3].
    Vector2f p[4];

    // Arc data, valid when kind is arc.
    Vector2f center;
    float radius;
    float thetaStart;
    float sweep;
};

Vector2f pointAt(Seg const& seg, float t)
{
    switch (seg.kind)
    {
        case SegKind::line:
            return seg.p[0] + t * (seg.p[1] - seg.p[0]);

        case SegKind::conic:
        {
            float u = 1.0f - t;
            return u * u * seg.p[0] + 2.0f * u * t * seg.p[1]
                + t * t * seg.p[2];
        }

        case SegKind::cubic:
        {
            float u = 1.0f - t;
            return u * u * u * seg.p[0] + 3.0f * u * u * t * seg.p[1]
                + 3.0f * u * t * t * seg.p[2] + t * t * t * seg.p[3];
        }

        case SegKind::arc:
        {
            float theta = seg.thetaStart + t * seg.sweep;
            return seg.center
                + seg.radius * Vector2f(std::cos(theta), std::sin(theta));
        }
    }

    return seg.p[0];
}

Vector2f tangentAt(Seg const& seg, float t)
{
    switch (seg.kind)
    {
        case SegKind::line:
            return seg.p[1] - seg.p[0];

        case SegKind::conic:
        {
            float u = 1.0f - t;
            return 2.0f * u * (seg.p[1] - seg.p[0])
                + 2.0f * t * (seg.p[2] - seg.p[1]);
        }

        case SegKind::cubic:
        {
            float u = 1.0f - t;
            return 3.0f * u * u * (seg.p[1] - seg.p[0])
                + 6.0f * u * t * (seg.p[2] - seg.p[1])
                + 3.0f * t * t * (seg.p[3] - seg.p[2]);
        }

        case SegKind::arc:
        {
            float theta = seg.thetaStart + t * seg.sweep;
            Vector2f radial(std::cos(theta), std::sin(theta));
            // The tangent points along the sweep direction.
            return std::copysign(1.0f, seg.sweep) * perp(radial);
        }
    }

    return seg.p[1] - seg.p[0];
}

/** @brief Signed line-coordinate of a point: n . (point - linePoint). */
float signedDist(Vector2f point, Vector2f linePoint, Vector2f n)
{
    return n.dot(point - linePoint);
}

/**
 * @brief Appends the segment parameters where the line crosses the segment.
 *
 * Roots are kept in the half-open interval [0, 1) so a vertex shared by two
 * segments is owned by exactly one of them.
 */
void appendSegmentRoots(Seg const& seg, Vector2f linePoint, Vector2f n,
        std::vector<float>& out)
{
    std::vector<double> roots;

    if (seg.kind == SegKind::arc)
    {
        // Intersect the line with the arc's circle, then keep the hits whose
        // angle falls inside the swept range.
        Vector2f dir = perp(n) * -1.0f; // A direction along the line.
        Vector2f f = linePoint - seg.center;

        double aa = dir.dot(dir);
        double bb = 2.0 * f.dot(dir);
        double cc = f.dot(f) - (double)seg.radius * seg.radius;

        std::vector<double> us;
        solveQuadratic(aa, bb, cc, us);

        for (double u : us)
        {
            Vector2f x = linePoint + (float)u * dir;
            Vector2f radial = x - seg.center;
            float theta = std::atan2(radial[1], radial[0]);

            // Fraction of the sweep, wrapped into [0, 1).
            float delta = theta - seg.thetaStart;
            float turns = delta / seg.sweep;
            turns -= std::floor(turns);

            if (turns >= 0.0f && turns < 1.0f)
                roots.push_back(turns);
        }
    }
    else
    {
        float g0 = signedDist(seg.p[0], linePoint, n);
        float g1 = signedDist(seg.p[1], linePoint, n);

        if (seg.kind == SegKind::line)
        {
            solveLinear(g1 - g0, g0, roots);
        }
        else if (seg.kind == SegKind::conic)
        {
            float g2 = signedDist(seg.p[2], linePoint, n);
            double a = g0 - 2.0 * g1 + g2;
            double b = -2.0 * g0 + 2.0 * g1;
            double c = g0;
            solveQuadratic(a, b, c, roots);
        }
        else
        {
            float g2 = signedDist(seg.p[2], linePoint, n);
            float g3 = signedDist(seg.p[3], linePoint, n);
            double a = -g0 + 3.0 * g1 - 3.0 * g2 + g3;
            double b = 3.0 * g0 - 6.0 * g1 + 3.0 * g2;
            double c = -3.0 * g0 + 3.0 * g1;
            double d = g0;
            solveCubic(a, b, c, d, roots);
        }
    }

    for (double r : roots)
    {
        if (r >= 0.0 && r < 1.0)
            out.push_back((float)r);
    }
}

/** @brief Walks a path into per-subpath lists of resolved, closed segments. */
std::vector<std::vector<Seg>> resolveSubpaths(Path const& path)
{
    Transform const& transform = path.getTransform();

    std::vector<std::vector<Seg>> subpaths;
    std::vector<Seg> current;
    Vector2f cur(0.0f, 0.0f);
    Vector2f subStart(0.0f, 0.0f);
    bool haveSub = false;

    auto closeCurrent = [&]()
    {
        if (!haveSub)
            return;

        Vector2f diff = cur - subStart;
        if (diff.dot(diff) > 1e-12f)
        {
            Seg seg;
            seg.kind = SegKind::line;
            seg.p[0] = cur;
            seg.p[1] = subStart;
            current.push_back(seg);
        }

        if (!current.empty())
            subpaths.push_back(std::move(current));

        current.clear();
    };

    for (auto i = path.begin(); i != path.end(); ++i)
    {
        switch (i.getType())
        {
            case Path::SegmentType::start:
            {
                closeCurrent();
                subStart = transform * i.getStart().v;
                cur = subStart;
                haveSub = true;
                break;
            }

            case Path::SegmentType::line:
            {
                Seg seg;
                seg.kind = SegKind::line;
                seg.p[0] = cur;
                seg.p[1] = transform * i.getLine().v;
                current.push_back(seg);
                cur = seg.p[1];
                break;
            }

            case Path::SegmentType::conic:
            {
                Seg seg;
                seg.kind = SegKind::conic;
                seg.p[0] = cur;
                seg.p[1] = transform * i.getConic().v1;
                seg.p[2] = transform * i.getConic().v2;
                current.push_back(seg);
                cur = seg.p[2];
                break;
            }

            case Path::SegmentType::cubic:
            {
                Seg seg;
                seg.kind = SegKind::cubic;
                seg.p[0] = cur;
                seg.p[1] = transform * i.getCubic().v1;
                seg.p[2] = transform * i.getCubic().v2;
                seg.p[3] = transform * i.getCubic().v3;
                current.push_back(seg);
                cur = seg.p[3];
                break;
            }

            case Path::SegmentType::arc:
            {
                Seg seg;
                seg.kind = SegKind::arc;
                seg.center = transform * i.getArc().center;
                seg.p[0] = cur;

                Vector2f radial = cur - seg.center;
                seg.radius = std::sqrt(radial.dot(radial));
                seg.thetaStart = std::atan2(radial[1], radial[0]);
                seg.sweep = i.getArc().angle;

                current.push_back(seg);
                cur = pointAt(seg, 1.0f);
                break;
            }
        }
    }

    closeCurrent();

    return subpaths;
}

/** @brief A candidate crossing before transversality has been decided. */
struct RawRoot
{
    std::size_t localIndex;
    float t;
    float loopParam;
};

/**
 * @brief Sign of the signed-distance field within the gap between two roots.
 *
 * Between two consecutive roots the field keeps one sign, so sampling a few
 * interior points and taking the strongest one recovers that sign robustly.
 */
int gapSign(std::vector<Seg> const& segs, Vector2f linePoint, Vector2f n,
        float fromLoop, float toLoop)
{
    float numSegs = (float)segs.size();
    float best = 0.0f;

    float const fractions[3] = { 0.5f, 0.25f, 0.75f };
    for (float frac : fractions)
    {
        float sample = fromLoop + frac * (toLoop - fromLoop);
        float wrapped = std::fmod(sample, numSegs);
        if (wrapped < 0.0f)
            wrapped += numSegs;

        std::size_t idx = (std::size_t)wrapped;
        if (idx >= segs.size())
            idx = segs.size() - 1;

        float t = wrapped - (float)idx;
        float s = signedDist(pointAt(segs[idx], t), linePoint, n);

        if (std::abs(s) > std::abs(best))
            best = s;
    }

    if (best > 0.0f)
        return 1;
    if (best < 0.0f)
        return -1;

    return 0;
}

} // anonymous namespace

std::vector<PathCrossing> pathLineCrossings(Path const& path,
        Vector2f linePoint, Vector2f lineDir)
{
    std::vector<PathCrossing> result;

    float dirLen = std::sqrt(lineDir.dot(lineDir));
    if (dirLen < 1e-12f)
        return result;

    Vector2f unitDir = lineDir / dirLen;
    Vector2f n = perp(lineDir);

    std::vector<std::vector<Seg>> subpaths = resolveSubpaths(path);

    std::size_t globalBase = 0;
    for (auto const& segs : subpaths)
    {
        std::vector<RawRoot> roots;

        for (std::size_t s = 0; s < segs.size(); ++s)
        {
            std::vector<float> ts;
            appendSegmentRoots(segs[s], linePoint, n, ts);
            std::sort(ts.begin(), ts.end());

            for (float t : ts)
            {
                RawRoot r;
                r.localIndex = s;
                r.t = t;
                r.loopParam = (float)s + t;
                roots.push_back(r);
            }
        }

        std::size_t const count = roots.size();
        if (count > 0)
        {
            float numSegs = (float)segs.size();

            // The gap after root i runs to root i+1 (wrapping the loop). A root
            // is a genuine crossing when the field changes sign across it, i.e.
            // the gaps on its two sides disagree.
            std::vector<int> gaps(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                float from = roots[i].loopParam;
                float to = roots[(i + 1) % count].loopParam;
                if (to <= from)
                    to += numSegs;

                gaps[i] = gapSign(segs, linePoint, n, from, to);
            }

            for (std::size_t i = 0; i < count; ++i)
            {
                int before = gaps[(i + count - 1) % count];
                int after = gaps[i];
                if (before == 0 || after == 0 || before == after)
                    continue;

                Seg const& seg = segs[roots[i].localIndex];
                Vector2f point = pointAt(seg, roots[i].t);
                Vector2f tangent = tangentAt(seg, roots[i].t);

                PathCrossing crossing;
                crossing.point = point;
                crossing.lineParam = unitDir.dot(point - linePoint);
                crossing.segmentT = roots[i].t;
                crossing.segmentIndex = globalBase + roots[i].localIndex;
                crossing.windingSign =
                    cross(tangent, lineDir) >= 0.0f ? 1 : -1;

                result.push_back(crossing);
            }
        }

        globalBase += segs.size();
    }

    return result;
}

} // namespace avg
