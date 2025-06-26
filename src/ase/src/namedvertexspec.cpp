#include "namedvertexspec.h"

#include "type.h"

#include <algorithm>

namespace ase
{

NamedVertexSpec::NamedVertexSpec() :
    primitiveType_(PrimitiveTriangle),
    stride_(0),
    next_(0)
{
}

NamedVertexSpec::NamedVertexSpec(PrimitiveType primitiveType, size_t stride) :
    primitiveType_(primitiveType),
    stride_(stride),
    next_(0)
{
}

NamedVertexSpec::~NamedVertexSpec()
{
}

NamedVertexSpec& NamedVertexSpec::add(std::string const& attrib, int size,
        Type type, bool normalized, size_t pointer)
{
    Spec spec;
    spec.attrib = attrib;
    spec.size = size;
    spec.type = type;
    spec.normalized = normalized;
    spec.pointer = pointer ? pointer : next_;

    specs_.push_back(spec);
    next_ += pointer + size * typeSize(type);

    return *this;
}

std::vector<NamedVertexSpec::Spec> const& NamedVertexSpec::getSpecs() const
{
    return specs_;
}

PrimitiveType NamedVertexSpec::getPrimitiveType() const
{
    return primitiveType_;
}

size_t NamedVertexSpec::getStride() const
{
    return std::max(stride_, next_);
}

bool NamedVertexSpec::operator==(NamedVertexSpec const& rhs) const
{
    return specs_ == rhs.specs_;
}

bool NamedVertexSpec::operator!=(NamedVertexSpec const& rhs) const
{
    return specs_ != rhs.specs_;
}

} // namespace
