#include "vertexspec.h"

#include "namedvertexspec.h"
#include "program.h"

#include <algorithm>

namespace ase
{

VertexSpec::VertexSpec() :
    primitiveType_(PrimitiveTriangle),
    stride_(0)
{
}

VertexSpec::VertexSpec(Program const& program, NamedVertexSpec const& spec) :
    primitiveType_(spec.getPrimitiveType()),
    stride_(spec.getStride())
{
    std::vector<NamedVertexSpec::Spec> const& specs = spec.getSpecs();
    for (auto i = specs.begin(); i != specs.end(); ++i)
    {
        NamedVertexSpec::Spec const& namedSpec = *i;
        Spec binarySpec;

        binarySpec.attribLoc = program.getAttribLocation(namedSpec.attrib);
        binarySpec.size = namedSpec.size;
        binarySpec.type = namedSpec.type;
        binarySpec.normalized = namedSpec.normalized;
        binarySpec.pointer = namedSpec.pointer;

        if (binarySpec.attribLoc != -1)
            specs_.push_back(binarySpec);
    }

    std::sort(specs_.begin(), specs_.end());
}

VertexSpec::~VertexSpec()
{
}

std::vector<VertexSpec::Spec> const& VertexSpec::getSpecs() const
{
    return specs_;
}

PrimitiveType VertexSpec::getPrimitiveType() const
{
    return primitiveType_;
}

size_t VertexSpec::getStride() const
{
    return stride_;
}

bool VertexSpec::operator==(VertexSpec const& rhs) const
{
    return specs_ == rhs.specs_;
}

bool VertexSpec::operator!=(VertexSpec const& rhs) const
{
    return specs_ != rhs.specs_;
}

bool VertexSpec::operator<(VertexSpec const& rhs) const
{
    return specs_ < rhs.specs_;
}

bool VertexSpec::isEmpty() const
{
    return specs_.empty();
}

} // namespace

