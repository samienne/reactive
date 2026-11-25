// A headless, remote-driven bqui app showing a render view next to ordinary
// widgets. Its callback clears the view and draws two overlapping triangles
// with raw ase calls: a near red one submitted first and a far green one
// submitted after it, so the red one stays on top only if depth testing
// works. A small yellow triangle marks the view's top-left corner.
//
// Launched like inspectorapp: the remote endpoint comes from argv[1], else
// from the REACTIVE_REMOTE_ENDPOINT environment variable as App reads it.

#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/renderview.h>
#include <bqui/widget/vbox.h>

#include <bqui/renderview.h>
#include <bqui/window.h>
#include <bqui/app.h>

#include <bq/signal/constant.h>

#include <ase/buffer.h>
#include <ase/commandbuffer.h>
#include <ase/fragmentshader.h>
#include <ase/namedvertexspec.h>
#include <ase/pipeline.h>
#include <ase/program.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/uniformbuffer.h>
#include <ase/uniformbufferrange.h>
#include <ase/uniformset.h>
#include <ase/vertexbuffer.h>
#include <ase/vertexshader.h>
#include <ase/vertexspec.h>

#include <array>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace bqui;

namespace
{
    char const* vsSource =
        "#version 330\n"
        "#extension GL_ARB_shading_language_420pack : require\n"
        "layout(std140, binding = 0) uniform Block\n"
        "{\n"
        "    mat4 transform;\n"
        "    vec4 color;\n"
        "} block;\n"
        "in vec3 pos;\n"
        "in vec3 normal;\n"
        "out vec4 vColor;\n"
        "void main()\n"
        "{\n"
        "    vColor = vec4(block.color.rgb * max(normal.z, 0.0),"
        " block.color.a);\n"
        "    gl_Position = block.transform * vec4(pos, 1.0);\n"
        "}\n";

    char const* fsSource =
        "#version 330\n"
        "in vec4 vColor;\n"
        "out vec4 result;\n"
        "void main()\n"
        "{\n"
        "    result = vColor;\n"
        "}\n";

    struct Triangle
    {
        ase::VertexBuffer vertices;
        ase::UniformBuffer uniforms;
        ase::UniformSet uniformSet;
    };

    class TriangleRenderer
    {
    public:
        void render(RenderFrame& frame)
        {
            auto& context = frame.getRenderContext();
            if (!pipeline_)
                init(context);

            for (auto const& target : frame.getTargets())
            {
                // Separate submits: the queue keeps their order, which a
                // single buffer's draw sort would not.
                ase::CommandBuffer clear;
                clear.pushClear(target.framebuffer, 0.1f, 0.2f, 0.5f, 1.0f);
                context.getMainRenderQueue().submit(std::move(clear));

                for (auto const& triangle : triangles_)
                {
                    ase::CommandBuffer commands;
                    commands.push(target.framebuffer, *pipeline_,
                            triangle.uniformSet, triangle.vertices,
                            std::nullopt, {}, 0.0f);
                    context.getMainRenderQueue().submit(std::move(commands));
                }
            }
        }

    private:
        void init(ase::RenderContext& context)
        {
            auto program = context.makeProgram(
                    context.makeVertexShader(vsSource),
                    context.makeFragmentShader(fsSource));

            ase::NamedVertexSpec spec;
            spec.add("pos", 3, ase::TypeFloat, false)
                .add("normal", 3, ase::TypeFloat, false);

            pipeline_.emplace(context.makePipeline(program,
                        ase::VertexSpec(program, std::move(spec))));

            ase::CommandBuffer uploads;

            // Near red, far green overlapping it, yellow top-left marker.
            add(context, uploads, {{ -0.7f, -0.6f, 0.2f,  0.5f, -0.6f, 0.2f,
                    -0.1f, 0.6f, 0.2f }}, { 1.0f, 0.0f, 0.0f, 1.0f });
            add(context, uploads, {{ -0.4f, -0.4f, 0.6f,  0.8f, -0.4f, 0.6f,
                    0.2f, 0.7f, 0.6f }}, { 0.0f, 1.0f, 0.0f, 1.0f });
            add(context, uploads, {{ -1.0f, 1.0f, 0.0f,  -1.0f, 0.7f, 0.0f,
                    -0.8f, 1.0f, 0.0f }}, { 1.0f, 1.0f, 0.0f, 1.0f });

            context.getMainRenderQueue().submit(std::move(uploads));
        }

        void add(ase::RenderContext& context, ase::CommandBuffer& uploads,
                std::array<float, 9> positions, std::array<float, 4> color)
        {
            std::vector<float> vertices;
            for (size_t i = 0; i < 9; i += 3)
            {
                vertices.insert(vertices.end(), {
                        positions[i], positions[i + 1], positions[i + 2],
                        0.0f, 0.0f, 1.0f });
            }

            std::vector<float> block = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
                color[0], color[1], color[2], color[3] };

            Triangle triangle{ context.makeVertexBuffer(),
                context.makeUniformBuffer(), context.makeUniformSet() };

            uploads.pushUpload(triangle.vertices, ase::Buffer(vertices),
                    ase::Usage::StaticDraw);
            uploads.pushUpload(triangle.uniforms, ase::Buffer(block),
                    ase::Usage::StaticDraw);
            triangle.uniformSet.bindUniformBufferRange(0,
                    ase::UniformBufferRange{ 0, block.size() * sizeof(float),
                    triangle.uniforms });

            triangles_.push_back(std::move(triangle));
        }

        std::optional<ase::Pipeline> pipeline_;
        std::vector<Triangle> triangles_;
    };
} // anonymous namespace

int main(int argc, char** argv)
{
    TriangleRenderer renderer;
    RenderHandle handle([&renderer](RenderFrame& frame)
            {
                renderer.render(frame);
            });

    auto content = widget::vbox({
            widget::label(std::string("Render view demo")),
            widget::hbox({
                    widget::renderView(handle.makeView()),
                    widget::label(std::string("Beside the view")),
                    }),
            });

    try
    {
        App app;
        app.headless(true);
        if (argc > 1)
            app.setRemoteEndpoint(argv[1]);

        return app.addWindow(
                    window(bq::signal::constant<std::string>(
                            "Render View Demo")),
                    std::move(content))
            .run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "renderviewapp: " << e.what() << "\n";
        return 1;
    }
}
