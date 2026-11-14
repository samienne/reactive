#include "uniformblock.h"

#include <stdexcept>

namespace ase
{

ShaderInterface::Block const& findDeclaredBlock(ShaderInterface const& iface,
        std::string_view blockName)
{
    ShaderInterface::Block const* block = iface.findBlock(blockName);
    if (!block)
    {
        throw std::invalid_argument("No block '" + std::string(blockName)
                + "' declared");
    }

    return *block;
}

void checkUniformBlockLayout(ShaderInterface::Block const& block,
        size_t structSize, std::vector<UniformMemberLayout> const& members)
{
    if (structSize != block.size)
    {
        throw std::invalid_argument("Struct for block '" + block.name + "' is "
                + std::to_string(structSize) + " bytes, the block is "
                + std::to_string(block.size));
    }

    for (auto const& member : members)
    {
        ShaderInterface::Member const* declared = block.findMember(member.name);
        if (!declared)
        {
            throw std::invalid_argument("Block '" + block.name
                    + "' has no member '" + member.name + "'");
        }

        if (member.offset != declared->offset)
        {
            throw std::invalid_argument("Member '" + block.name + "."
                    + member.name + "' is at offset "
                    + std::to_string(member.offset) + " in the struct, "
                    + std::to_string(declared->offset) + " in the block");
        }

        if (member.size != declared->getSize())
        {
            throw std::invalid_argument("Member '" + block.name + "."
                    + member.name + "' is " + std::to_string(member.size)
                    + " bytes in the struct, "
                    + std::to_string(declared->getSize()) + " in the block");
        }
    }

    for (auto const& declared : block.members)
    {
        size_t listed = 0;
        for (auto const& member : members)
        {
            if (member.name == declared.name)
                ++listed;
        }

        if (listed != 1)
        {
            throw std::invalid_argument("Member '" + block.name + "."
                    + declared.name + "' is listed "
                    + std::to_string(listed) + " times, expected once");
        }
    }
}

} // namespace ase
