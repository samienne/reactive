#include "vertexspec.h"

#include "namedvertexspec.h"
#include "program.h"
#include "shaderinterface.h"

#include <algorithm>
#include <stdexcept>

namespace ase
{

VertexSpec::VertexSpec() :
    primitiveType_(PrimitiveTriangle),
    stride_(0)
{
}

VertexSpec::VertexSpec(ShaderInterface const& iface,
        NamedVertexSpec const& spec) :
    primitiveType_(spec.getPrimitiveType()),
    stride_(spec.getStride())
{
    for (auto const& namedSpec : spec.getSpecs())
    {
        auto const* attribute = iface.findAttribute(namedSpec.attrib);
        if (!attribute)
        {
            throw std::invalid_argument("Vertex attribute '"
                    + namedSpec.attrib + "' is not declared by the program");
        }

        Spec binarySpec;
        binarySpec.attribLoc = attribute->location;
        binarySpec.size = namedSpec.size;
        binarySpec.type = namedSpec.type;
        binarySpec.normalized = namedSpec.normalized;
        binarySpec.pointer = namedSpec.pointer;

        specs_.push_back(binarySpec);
    }

    std::sort(specs_.begin(), specs_.end());
}

VertexSpec::VertexSpec(Program const& program, NamedVertexSpec const& spec) :
    VertexSpec(program.getInterface(), spec)
{
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
