#include "dummyprogram.h"

namespace ase
{

DummyProgram::DummyProgram(ShaderInterface iface) :
    interface_(std::move(iface))
{
}

int DummyProgram::getUniformLocation(std::string const& /*name*/) const
{
    return -1;
}

int DummyProgram::getAttribLocation(std::string const& name) const
{
    auto const* attribute = interface_.findAttribute(name);
    return attribute ? attribute->location : -1;
}

ShaderInterface const& DummyProgram::getInterface() const
{
    return interface_;
}

std::optional<ShaderInterface> DummyProgram::reflect() const
{
    return std::nullopt;
}

} // namespace ase
