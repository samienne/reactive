#include <avg/font.h>
#include <avg/fontmanager.h>
#include <avg/path.h>
#include <avg/pathspec.h>
#include <avg/fillrule.h>
#include <avg/region.h>
#include <avg/simplepolygon.h>

#include <ase/glxplatform.h>
#include <ase/glxwindow.h>
#include <ase/pipeline.h>
#include <ase/vertexspec.h>
#include <ase/namedvertexspec.h>
#include <ase/vertexbuffer.h>
#include <ase/program.h>
#include <ase/vertexshader.h>
#include <ase/fragmentshader.h>
#include <ase/uniformbuffer.h>
#include <ase/texture.h>
#include <ase/nameduniformbuffer.h>
#include <ase/rendercommand.h>
#include <ase/rendertarget.h>
#include <ase/rendertargetobject.h>
#include <ase/rendercontext.h>
#include <ase/primitivetype.h>
#include <ase/type.h>
#include <ase/blendmode.h>
#include <ase/buffer.h>
#include <ase/async.h>
#include <ase/stringify.h>

#include <iostream>
#include <string>
#include <vector>
#include <utility>

#include <time.h>

static char const* simpleVsSource =
"attribute vec3 pos;\n"
"attribute vec2 textureCoordinate;\n"
"varying vec2 outTexCoord;\n"
"\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(pos - vec3(1.0, 1.0, 1.0), 1.0);\n"
"   outTexCoord = textureCoordinate;"
//"   outTexCoord = pos.xy;"
"}\n";

static char const* simpleFsSource =
//"uniform vec4 color;\n"
//"varying vec4 pos;\n"
"varying vec2 outTexCoord;\n"
"uniform sampler2D texture;\n"
"\n"
"void main()\n"
"{\n"
//"   gl_FragColor = texture2D(texture, outTexCoord);\n"
"   gl_FragColor = vec4(1.0, 1.0, 1.0, 0.1);\n"
"}\n";

time_t getTime()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

std::pair<ase::Buffer, ase::Buffer> triangulate2()
{
    std::vector<ase::Vector2i> p1;
    p1.push_back(ase::Vector2i(50, 200));
    p1.push_back(ase::Vector2i(750, 200));
    p1.push_back(ase::Vector2i(750, 400));
    p1.push_back(ase::Vector2i(50, 400));
    avg::SimplePolygon poly1(std::move(p1));

    std::vector<ase::Vector2i> p2;
    p2.push_back(ase::Vector2i(300, 50));
    p2.push_back(ase::Vector2i(500, 50));
    p2.push_back(ase::Vector2i(500, 550));
    p2.push_back(ase::Vector2i(300, 550));
    avg::SimplePolygon poly2(std::move(p2));

    std::vector<ase::Vector2i> p3;
    p3.push_back(ase::Vector2i(10, 10));
    p3.push_back(ase::Vector2i(40, 10));
    p3.push_back(ase::Vector2i(40, 40));
    p3.push_back(ase::Vector2i(10, 40));
    avg::SimplePolygon poly3(std::move(p3));

    std::vector<avg::SimplePolygon> polygons;
    polygons.push_back(std::move(poly1));
    polygons.push_back(std::move(poly2));
    polygons.push_back(std::move(poly3));

    avg::Region region(std::move(polygons), avg::FILL_EVENODD,
            ase::Vector2f(1.0 / 4.0, 1.0 / 3.0), 100);
    region.offset(avg::JOIN_ROUND, avg::END_CLOSEDLINE, 7.0f / 400.0f);

    std::pair<std::vector<ase::Vector2f>, std::vector<uint16_t> > bufs =
        region.triangulate();

    return std::make_pair(ase::Buffer(bufs.first), ase::Buffer(bufs.second));
}

std::pair<ase::Buffer, ase::Buffer> triangulate3()
{
    std::vector<avg::Path::SegmentType> segments;
    std::vector<ase::Vector2f> vertices;

    auto spec = avg::PathSpec()
            .start(ase::Vector2f(40.0 / 400, 100.0 / 300))
            .conicTo(ase::Vector2f(380.0 / 400, 40.0 / 300),
                ase::Vector2f(580.0 / 400, 280.0 / 300))
            .conicTo(ase::Vector2f(150.0 / 400, 200.0 / 300),
                ase::Vector2f(480.0 / 400, 280.0 / 300))
            .lineTo(ase::Vector2f(430.0 / 400, 380.0 / 300))
            .cubicTo(ase::Vector2f(280.0 / 400, 100.0 / 300),
                ase::Vector2f(200.0 / 400, 500.0 / 300),
                ase::Vector2f(80.0 / 400, 200.0 / 300))
            .lineTo(ase::Vector2f(40.0 / 400, 100.0 / 300));

    auto path = avg::Path(spec);

    //path += avg::Path(std::move(pathSpec));

    auto path2 = avg::Path(avg::PathSpec()
            .start(ase::Vector2f(-0.5f, -0.5f))
            .lineTo(ase::Vector2f(0.5f, -0.5f))
            .lineTo(ase::Vector2f(0.5f, 0.5f))
            .lineTo(ase::Vector2f(-0.5f, 0.5f))
            //.close();
            );

    auto t = avg::Transform()
        .translate(ase::Vector2f(1.0f, 1.0f))
        .setScale(1.0f);
    path += t * path2;
    t = std::move(t).setScale(0.9f);
    //path += t * path2;
    t = std::move(t)
        .setScale(0.8f)
        .translate(ase::Vector2f(0.3f, 0.0f));
    //path += t * path2;

    //
    // +-----------------------+
    // |.......................|
    // |.+-------------------+.|
    // |.|                   |.|
    // |.|       +-----------|-|----+
    // |.|       |...........| |....|
    // |.|       |...........| |....|
    // |.|       |...........| |....|
    // |.|       |...........| |....|
    // |.|       |...........| |....|
    // |.|       +-----------|-|----+
    // |.|                   |.|
    // |.+-------------------+.|
    // |.......................|
    // +-----------------------+
    //
    //
    //

    avg::Font font("/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf", 0);

    std::string ipsum(
            "Normally, both your asses would be dead as fucking fried\n"
            "chicken, but you happen to pull this shit while I'm in a\n"
            "transitional period so I don't wanna kill you, I wanna help\n"
            "you. But I can't give you this case, it don't belong to me.\n"
            "Besides, I've already been through too much shit this\n"
            "morning over this case to hand it over to your dumb ass.\n\n"

            "My money's in that office, right? If she start giving me\n"
            "some bullshit about it ain't there, and we got to go\n"
            "someplace else and get it, I'm gonna shoot you in the head\n"
            "then and there. Then I'm gonna shoot that bitch in the\n"
            "kneecaps, find out where my goddamn money is. She gonna tell\n"
            "me too. Hey, look at me when I'm talking to you,\n"
            "motherfucker. You listen: we go in there, and that nigga\n"
            "Winston or anybody else is in there, you the first\n"
            "motherfucker to get shot. You understand?");

    path += font.textToPath("PeDdpoOBbqQ\njepujee", 22.0f / 200.0,
            ase::Vector2f(0.2f, 0.5f));

    /*path += font.textToPath(ipsum, 9.0f / 200.0,
            ase::Vector2f(0.05f, 1.5f));*/



    /*path += font.textToPath("DD", 22.0f / 200.0,
            ase::Vector2f(0.2f, 1.5f));*/
    //std::cout << path << std::endl;

    avg::Region region = path.fillRegion(avg::FILL_EVENODD,
            ase::Vector2f(1.0 / 400.0, 1.0 / 300.0), 100);

    //region.offset(avg::JOIN_ROUND, avg::END_OPENBUTT, 1.0f / 400.0);

    std::pair<std::vector<ase::Vector2f>, std::vector<uint16_t> > bufs =
        region.triangulate();

    return std::make_pair(ase::Buffer(bufs.first), ase::Buffer(bufs.second));
}

int main()
{
    float vertices[] = {
        0.0, 0.0, -0.5, -0.5, 0.0,
        1.0, 0.0, 0.5, -0.5, 0.0,
        0.0, 1.0, -0.5, 0.5, 0.0,

        0.0, 1.0, -0.5, 0.5, 0.0,
        1.0, 0.0, 0.5, -0.5, 0.0,
        1.0, 1.0, 0.5, 0.5, 0.0
    };

    float vertices2[] = {
        0.0, 0.0, -1.0, -1.0, 0.0,
        1.0, 0.0, 0.0, -1.0, 0.0,
        0.0, 1.0, -1.0, 0.0, 0.0,

        0.0, 1.0, -0.5, 0.5, 0.0,
        1.0, 0.0, 0.5, -0.5, 0.0,
        1.0, 1.0, 0.5, 0.5, 0.0
    };

    uint16_t indices[] = {
        0, 1, 2, 2, 1, 5
    };

    ase::GlxPlatform platform;
    ase::RenderContext& bgContext = platform.getDefaultContext();
    ase::RenderContext context(platform);
    ase::GlxWindow window(platform, ase::Vector2i(800, 600));

    ase::VertexShader vs(bgContext, simpleVsSource);
    ase::FragmentShader fs(bgContext, simpleFsSource);
    ase::Program program(bgContext, vs, fs);

    ase::NamedVertexSpec namedSpec(ase::PrimitiveTriangle);
    namedSpec.add("textureCoordinate", 2, ase::TypeFloat, false);
    namedSpec.add("pos", 3, ase::TypeFloat, false);

    ase::NamedVertexSpec namedSpec2(ase::PrimitiveTriangle);
    //ase::NamedVertexSpec namedSpec2(ase::PrimitiveLine);
    namedSpec2.add("pos", 2, ase::TypeFloat, false);

    ase::Pipeline pipeline(bgContext, program, namedSpec);
    ase::Pipeline pipeline2(bgContext, program, namedSpec2);

    ase::Buffer vbBuffer(vertices, sizeof(vertices));
    ase::Buffer vbBuffer2(vertices2, sizeof(vertices2));
    ase::Buffer ibBuffer(indices, sizeof(indices));
    ase::VertexBuffer vb(bgContext, vbBuffer, ase::UsageStaticDraw,
            ase::Async());
    ase::VertexBuffer vb2(bgContext, vbBuffer2, ase::UsageStaticDraw,
            ase::Async());
    ase::IndexBuffer ib(bgContext, ibBuffer, ase::UsageStaticDraw,
            ase::Async());

    auto bufs = triangulate3();
    /*ase::VertexBuffer vb3(bgContext, namedSpec2, triangulate(),
            ase::UsageStaticDraw, ase::Async());*/
    ase::VertexBuffer vb3(bgContext, bufs.first, ase::UsageStaticDraw,
            ase::Async());
    /*ase::IndexBuffer ib3(bgContext, bufs.second, ase::UsageStaticDraw,
            ase::Async());*/
    ase::IndexBuffer ib3;

    bool running = true;
    window.setVisible(true);
    window.setTitle("glX test window");
    window.setCloseCallback([&running]() { running = false; });

    ase::Buffer textureData(256*256*4);
    unsigned char* data = textureData.mapWrite<unsigned char>();
    for (auto i = 0; i < (256*256); ++i)
    {
        data[i*4+0] = 255;
        data[i*4+1] = i / 256;
        data[i*4+2] = 0;
        data[i*4+3] = 255;
    }

    ase::Texture texture(bgContext, ase::Vector2i(256, 256), ase::FORMAT_SRGBA,
            textureData);
    ase::Texture texture2(bgContext, ase::Vector2i(256, 256), ase::FORMAT_SRGBA,
            textureData);

    std::vector<ase::Texture> textures;
    textures.push_back(texture);

    ase::NamedUniformBuffer namedUniforms;
    namedUniforms.uniform1i("texture", 0);

    ase::UniformBuffer uniforms(program, namedUniforms);

    ase::RenderTargetObject rto(bgContext);
    ase::Texture targetTexture(bgContext, ase::Vector2i(800, 600),
            ase::FORMAT_SRGBA, 0);
    rto.setColorTarget(0, targetTexture);

    bgContext.flush();

    time_t start = getTime();
    int n = 0;
    while (running)
    {
        auto events = platform.getEvents();
        window.handleEvents(events);

        window.clear();
        rto.clear();

        ase::RenderCommand c(pipeline, uniforms, vb, ib, {texture},
                ase::BlendSrcAlpha, ase::BlendOneMinusSrcAlpha, 2.0);

        rto.push(std::move(c));

        ase::RenderCommand c2(pipeline, uniforms, vb2, ib, {texture2},
                ase::BlendSrcAlpha, ase::BlendOneMinusSrcAlpha, 1.0);

        rto.push(std::move(c2));

        /*for (auto i = 0; i < 40; ++i)
        {
            ase::RenderCommand c3(pipeline, uniforms, vb,
                    ib, {targetTexture});
            window.push(std::move(c3));

            ase::RenderCommand c4(pipeline, uniforms, vb2, ib,
                    {targetTexture});
            window.push(std::move(c4));
        }*/

        ase::RenderCommand c5(pipeline2, uniforms, vb3, ib3,
                { texture }, ase::BlendSrcAlpha,
                ase::BlendOneMinusSrcAlpha, -3.0);
        window.push(std::move(c5));

        //rto.submitAll(context);
        window.submitAll(context);
        context.present(window);

        ++n;
        time_t now = getTime();
        if (now - start > 10000)
        {
            std::cout << "Rendered " << n << " frames in " << (now-start)
                << "ms. Fps: " << n * 1000 / (now - start) << std::endl;
            start = now;
            n = 0;
        }
    }

    return 0;
}

