#include "animated.h"

#include "vector.h"
#include "transform.h"
#include "obb.h"
#include "color.h"
#include "brush.h"
#include "pen.h"
#include "curve.h"

namespace avg
{
template class Animated<float>;
template class Animated<Vector2f>;
template class Animated<Rect>;
template class Animated<Transform>;
template class Animated<Obb>;
template class Animated<Color>;
template class Animated<Brush>;
template class Animated<Pen>;
template class Animated<Curve>;

} // namespace avg

