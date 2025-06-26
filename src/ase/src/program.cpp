#include "program.h"

#include "programimpl.h"
#include "vertexshader.h"
#include "fragmentshader.h"
#include "rendercontext.h"
#include "platform.h"

#include "debug.h"

#include <stdexcept>

namespace ase
{

Program::Program(std::shared_ptr<ProgramImpl> impl) :
    deferred_(std::move(impl))
{
}

Program::~Program()
{
}

int Program::getUniformLocation(std::string const& name) const
{
    if (d())
        return d()->getUniformLocation(name);

    return -1;
}

int Program::getAttribLocation(std::string const& name) const
{
    if (d())
        return d()->getAttribLocation(name);

    return -1;
}

bool Program::operator==(Program const& rhs) const
{
    return deferred_ == rhs.deferred_;
}

bool Program::operator!=(Program const& rhs) const
{
    return deferred_ != rhs.deferred_;
}

bool Program::operator<(Program const& rhs) const
{
    return deferred_ < rhs.deferred_;
}

} // namespace

