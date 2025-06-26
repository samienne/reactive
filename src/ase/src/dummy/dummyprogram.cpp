#include "dummyprogram.h"

namespace ase
{

int DummyProgram::getUniformLocation(std::string const& /*name*/) const
{
    return -1;
}

int DummyProgram::getAttribLocation(std::string const& /*name*/) const
{
    return -1;
}

} // namespace ase

