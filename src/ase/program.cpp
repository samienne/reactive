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

Program::Program(RenderContext& context, VertexShader const& vertexShader,
        FragmentShader const& fragmentShader) :
    deferred_(makeImpl(
                context,
                std::move(vertexShader),
                std::move(fragmentShader)))
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

std::shared_ptr<ProgramImpl> Program::makeImpl(RenderContext& context,
        VertexShader const& vertexShader, FragmentShader const& fragmentShader)
{
    return context.getPlatform().makeProgramImpl(context, vertexShader,
            fragmentShader);
}

} // namespace

