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
template class AVG_INSTANTIATE_TEMPLATE Animated<float>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Vector2f>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Rect>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Transform>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Obb>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Color>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Brush>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Pen>;
template class AVG_INSTANTIATE_TEMPLATE Animated<Curve>;

} // namespace avg

