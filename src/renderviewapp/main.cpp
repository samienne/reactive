// A bqui app showing a render view next to ordinary widgets. Its callback
// draws a lit, rotating cube with raw ase calls.
//
// Usage: renderviewapp [--headless] [endpoint]
//
// With no arguments it opens a normal window. An endpoint (as inspectorapp
// takes it) drives it remotely and implies --headless.

#include <bqui/widget/button.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/renderview.h>
#include <bqui/widget/vbox.h>
#include <bqui/modifier/ondraw.h>

#include <bqui/renderview.h>
#include <bqui/window.h>
#include <bqui/app.h>

#include <bq/signal/constant.h>

#include <avg/brush.h>
#include <avg/color.h>
#include <avg/shape.h>

#include <ase/buffer.h>
#include <ase/commandbuffer.h>
#include <ase/fragmentshader.h>
#include <ase/matrix.h>
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

#include <Eigen/Geometry>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace bqui;

namespace
{
    char const* blockSource =
        "#version 330\n"
        "#extension GL_ARB_shading_language_420pack : require\n"
        "layout(std140, binding = 0) uniform Block\n"
        "{\n"
        "    mat4 mvp;\n"
        "    mat4 model;\n"
        "    mat4 normalMatrix;\n"
        "    vec4 eye;\n"
        "    vec4 baseColor;\n"
        "    vec4 ambient;\n"
        "    vec4 lights[2];\n"
        "    vec4 lightColor[2];\n"
        "} block;\n";

    size_t const blockFloats = 3 * 16 + 7 * 4;

    char const* vsSource =
        "in vec3 pos;\n"
        "in vec3 normal;\n"
        "out vec3 vPos;\n"
        "out vec3 vNormal;\n"
        "void main()\n"
        "{\n"
        "    vPos = (block.model * vec4(pos, 1.0)).xyz;\n"
        "    vNormal = mat3(block.normalMatrix) * normal;\n"
        "    gl_Position = block.mvp * vec4(pos, 1.0);\n"
        "}\n";

    char const* fsSource =
        "in vec3 vPos;\n"
        "in vec3 vNormal;\n"
        "out vec4 result;\n"
        "void main()\n"
        "{\n"
        "    vec3 n = normalize(vNormal);\n"
        "    vec3 v = normalize(block.eye.xyz - vPos);\n"
        "    vec3 base = block.baseColor.rgb;\n"
        "    vec3 color = block.ambient.rgb * base;\n"
        "    for (int i = 0; i < 2; ++i)\n"
        "    {\n"
        "        vec4 light = block.lights[i];\n"
        "        vec3 l = normalize(light.w == 0.0\n"
        "            ? -light.xyz : light.xyz - vPos);\n"
        "        vec3 h = normalize(l + v);\n"
        "        float diffuse = max(dot(n, l), 0.0);\n"
        "        float specular = diffuse > 0.0\n"
        "            ? pow(max(dot(n, h), 0.0), 40.0) : 0.0;\n"
        "        color += block.lightColor[i].rgb\n"
        "            * (base * diffuse + vec3(0.5) * specular);\n"
        "    }\n"
        "    result = vec4(color, 1.0);\n"
        "}\n";

    std::vector<float> cubeVertices()
    {
        // Per face: the normal n, then in-plane axes u and v with u x v = n.
        float const faces[6][9] = {
            { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
            { -1, 0, 0, 0, 0, 1, 0, 1, 0 },
            { 0, 1, 0, 0, 0, 1, 1, 0, 0 },
            { 0, -1, 0, 1, 0, 0, 0, 0, 1 },
            { 0, 0, 1, 1, 0, 0, 0, 1, 0 },
            { 0, 0, -1, 0, 1, 0, 1, 0, 0 },
        };
        float const corners[6][2] = {
            { -1, -1 }, { 1, -1 }, { 1, 1 },
            { -1, -1 }, { 1, 1 }, { -1, 1 },
        };

        std::vector<float> vertices;
        for (auto const& f : faces)
        {
            for (auto const& c : corners)
            {
                for (int i = 0; i < 3; ++i)
                {
                    vertices.push_back(
                            f[i] + c[0] * f[3 + i] + c[1] * f[6 + i]);
                }
                vertices.insert(vertices.end(), { f[0], f[1], f[2] });
            }
        }
        return vertices;
    }

    ase::Matrix4f perspective(float fovY, float aspect, float near, float far)
    {
        float f = 1.0f / std::tan(fovY / 2.0f);
        ase::Matrix4f m = ase::Matrix4f::Zero();
        m(0, 0) = f / aspect;
        m(1, 1) = f;
        m(2, 2) = (far + near) / (near - far);
        m(2, 3) = 2.0f * far * near / (near - far);
        m(3, 2) = -1.0f;
        return m;
    }

    void appendMat4(std::vector<float>& block, ase::Matrix4f const& m)
    {
        block.insert(block.end(), m.data(), m.data() + 16);
    }

    void appendVec4(std::vector<float>& block, Eigen::Vector3f const& v,
            float w = 0.0f)
    {
        block.insert(block.end(), { v.x(), v.y(), v.z(), w });
    }

    class CubeRenderer
    {
    public:
        void setPaused(bool paused)
        {
            paused_ = paused;
        }

        bool isPaused() const
        {
            return paused_;
        }

        void render(RenderFrame& frame)
        {
            auto& context = frame.getRenderContext();
            if (!pipeline_)
                init(context);

            if (!paused_)
            {
                float dt = std::chrono::duration<float>(
                        frame.getDt()).count();
                angle_ += std::min(dt, 0.1f) * 0.9f;
            }

            for (auto const& target : frame.getTargets())
            {
                ase::CommandBuffer setup;
                setup.pushUpload(*uniforms_,
                        ase::Buffer(makeBlock(target.size)),
                        ase::Usage::StreamDraw);
                setup.pushClear(target.framebuffer, 0.015f, 0.02f, 0.03f,
                        1.0f);
                context.getMainRenderQueue().submit(std::move(setup));

                ase::CommandBuffer draw;
                draw.push(target.framebuffer, *pipeline_, *uniformSet_,
                        *vertices_, std::nullopt, {}, 0.0f);
                context.getMainRenderQueue().submit(std::move(draw));
            }
        }

        void release()
        {
            pipeline_.reset();
            vertices_.reset();
            uniforms_.reset();
            uniformSet_.reset();
        }

    private:
        std::vector<float> makeBlock(ase::Vector2i size) const
        {
            float aspect = size.y() > 0
                ? static_cast<float>(size.x()) / static_cast<float>(size.y())
                : 1.0f;

            Eigen::Affine3f view = Eigen::Translation3f(0.0f, 0.0f, -7.0f)
                * Eigen::AngleAxisf(0.35f, Eigen::Vector3f::UnitX());
            Eigen::Affine3f model(Eigen::AngleAxisf(angle_,
                        Eigen::Vector3f(0.3f, 1.0f, 0.2f).normalized()));

            ase::Matrix4f normal = ase::Matrix4f::Identity();
            normal.topLeftCorner<3, 3>() =
                model.linear().inverse().transpose();

            std::vector<float> block;
            appendMat4(block, perspective(0.75f, aspect, 0.5f, 20.0f)
                    * view.matrix() * model.matrix());
            appendMat4(block, model.matrix());
            appendMat4(block, normal);
            appendVec4(block, view.inverse().translation());
            appendVec4(block, Eigen::Vector3f(0.75f, 0.75f, 0.75f));
            appendVec4(block, Eigen::Vector3f(0.06f, 0.06f, 0.08f));
            // A light with w = 1 sits at xyz; with w = 0 it shines along xyz.
            appendVec4(block, Eigen::Vector3f(-3.0f, 4.0f, 4.0f), 1.0f);
            appendVec4(block, Eigen::Vector3f(-1.0f, -0.2f, -0.3f));
            appendVec4(block, Eigen::Vector3f(1.0f, 0.75f, 0.45f));
            appendVec4(block, Eigen::Vector3f(0.2f, 0.35f, 0.9f));
            return block;
        }

        void init(ase::RenderContext& context)
        {
            auto program = context.makeProgram(
                    context.makeVertexShader(
                        std::string(blockSource) + vsSource),
                    context.makeFragmentShader(
                        std::string(blockSource) + fsSource));

            ase::NamedVertexSpec spec;
            spec.add("pos", 3, ase::TypeFloat, false)
                .add("normal", 3, ase::TypeFloat, false);

            pipeline_.emplace(context.makePipeline(program,
                        ase::VertexSpec(program, std::move(spec))));

            vertices_.emplace(context.makeVertexBuffer());
            uniforms_.emplace(context.makeUniformBuffer());
            uniformSet_.emplace(context.makeUniformSet());
            uniformSet_->bindUniformBufferRange(0, ase::UniformBufferRange{
                    0, blockFloats * sizeof(float), *uniforms_ });

            ase::CommandBuffer uploads;
            uploads.pushUpload(*vertices_, ase::Buffer(cubeVertices()),
                    ase::Usage::StaticDraw);
            context.getMainRenderQueue().submit(std::move(uploads));
        }

        std::optional<ase::Pipeline> pipeline_;
        std::optional<ase::VertexBuffer> vertices_;
        std::optional<ase::UniformBuffer> uniforms_;
        std::optional<ase::UniformSet> uniformSet_;
        std::atomic<bool> paused_ = false;
        float angle_ = 0.6f;
    };

    avg::Drawing drawCaptionStrip(avg::DrawContext const& context,
            avg::Vector2f size)
    {
        float const height = std::min(28.0f, size.y());

        auto path = context.pathBuilder()
            .start(0.0f, 0.0f)
            .lineTo(size.x(), 0.0f)
            .lineTo(size.x(), height)
            .lineTo(0.0f, height)
            .close()
            .build();

        return avg::Shape(std::move(path)).fill(
                avg::Brush(avg::Color(0.0f, 0.0f, 0.0f, 0.6f)));
    }
} // anonymous namespace

int main(int argc, char** argv)
{
    bool headless = false;
    std::string endpoint;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--headless")
            headless = true;
        else
            endpoint = arg;
    }
    if (!endpoint.empty())
        headless = true;

    CubeRenderer renderer;
    RenderHandle handle([&renderer](RenderFrame& frame)
            {
                renderer.render(frame);
            },
            [&renderer]()
            {
                renderer.release();
            });
    handle.setContinuous(true);

    auto togglePause = [&renderer, &handle]()
    {
        bool paused = !renderer.isPaused();
        renderer.setPaused(paused);
        handle.setContinuous(!paused);
    };

    auto content = widget::vbox({
            widget::label(std::string("Render view: a lit, rotating cube")),
            widget::hbox({
                    widget::renderView(handle.makeView())
                        | modifier::onDraw(drawCaptionStrip),
                    widget::vbox({
                            widget::label(std::string("Key light: warm")),
                            widget::label(std::string("Fill light: cool")),
                            widget::button("Pause / resume",
                                    bq::signal::constant(togglePause)),
                            }),
                    }),
            });

    try
    {
        App app;
        app.headless(headless);
        if (!endpoint.empty())
            app.setRemoteEndpoint(endpoint);

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
