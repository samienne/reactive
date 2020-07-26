#include "dummyapp.h"

namespace reactive
{

void DummyApp::addWindows(std::vector<Window> /*windows*/)
{
}

int DummyApp::run(AnySignal<bool> /*running*/) &&
{
    return 0;
}

int DummyApp::run() &&
{
    return 0;
}

std::unique_ptr<AppImpl> makeAppImpl()
{
    return std::make_unique<DummyApp>();
}

} // namespace reactive

