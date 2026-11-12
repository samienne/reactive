#include <ase/dummyplatform.h>
#include <ase/matrix.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

using namespace ase;

TEST(rendercontext, dummyClipSpaceAdjustmentIsIdentity)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    EXPECT_TRUE(context.getClipSpaceAdjustment().isIdentity());
}
