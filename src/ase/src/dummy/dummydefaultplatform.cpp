#include "dummyplatform.h"

#include "platform.h"

namespace ase
{

// On platforms with no OS windowing backend the headless platform is also the
// default. Backends that have a real one (wgl/glx) define this themselves.
Platform makeDefaultPlatform()
{
    return makeDummyPlatform();
}

} // namespace ase
