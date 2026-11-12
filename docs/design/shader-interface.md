# Design: the shader interface is declared, not introspected

> **Status: design, steps 1-2 implemented.** Pins how `ase` programs describe
> their inputs - vertex attributes, uniform blocks and samplers - and how
> callers set them. Replaces the earlier mix of GLSL layout qualifiers, GL
> introspection and hand-computed byte offsets with one declaration that every
> backend consumes and the GL backend merely validates. The backdrop-effect
> work (pass uniform blocks, multi-sampler passes) builds on this. Verified
> against `d1633d89` (2026-11-12); the "Why" citations describe `a201d674`, the commit
> this work started from; the "Vulkan readiness" section records owner
> decisions of 2026-11-11/12, implemented in `4a31efc0` (the pipeline
> formats) and `3ceaee0c` (the remaining enablers, 2026-11-12) except the
> compiler component, which waits for Vulkan work. Migration steps 1 (the
> `ase` declaration, language query and program creation), 2
> (`UniformBlock<T>`, `UniformValues`, `UniformRing`, the declaration-aware
> `UniformSet`, and the painter on top of them) and 5 (the Vulkan readiness
> enablers) are implemented; steps 3 and 4 are still ahead of the code.

## Why

The contract between a shader and the C++ that feeds it is split three ways
today, and no single place owns it:

- **GLSL text** pins uniform-block and sampler bindings with `layout(binding =
  N)`, which GLSL 330 only allows under a required
  `GL_ARB_shading_language_420pack` extension (`src/avg/src/painter.cpp:24-26`;
  the text pipeline repeats it at `:54-56` and pins its `atlas` sampler the
  same way at `:83-85`). The shader source is the only place the block and
  sampler bindings are stated.
- **C++ integers** are used directly as GL binding points: `UniformSet` maps an
  integer to a buffer range (`src/ase/include/ase/uniformset.h:22`,
  `uniformbufferrange.h:11-13`) and the GL backend passes that integer to
  `glBindBufferRange` (`src/ase/src/gl/glrenderstate.cpp:512`). Textures are a
  positional vector on the draw (`src/ase/include/ase/drawcommand.h:24`) bound to
  unit = index (`glrenderstate.cpp:463-466`). Nothing in `ase` connects either
  integer to a name in the program.
- **Names via introspection** are used for vertex attributes only. `GlProgram`
  reads active attributes and uniforms after link
  (`src/ase/src/gl/glprogram.cpp:112-139`); `VertexSpec` resolves each
  `NamedVertexSpec` entry with `getAttribLocation`
  (`src/ase/src/vertexspec.cpp:27`). The introspected uniform names are stored
  and never read.

That split produces concrete defects:

- An attribute name the shader does not declare is **silently dropped**
  (`vertexspec.cpp:33-34`). A typo renders garbage instead of failing.
- The dummy backend answers `-1` for every name
  (`src/ase/src/dummy/dummyprogram.cpp:6-14`), so every headless `VertexSpec` is
  empty and tests cannot check a vertex layout. `DummyPipeline` keeps no spec at
  all (`src/ase/include/ase/dummypipeline.h:10`).
- Uniform data is raw bytes at offsets the caller computes by hand for std140
  (`painter.cpp:167`, a 64-byte `mat4`). Nothing checks the size or layout
  against the program.
- Updating a block goes through `pushUpload`
  (`src/ase/include/ase/commandbuffer.h:41`), a non-draw command. The GL queue
  sorts only runs of draws between non-draw commands
  (`glrenderstate.cpp:191-203`), so every uniform update is also a batching
  fence.
- `bindUniformBufferRange` inserts and never overwrites
  (`src/ase/src/gl/gluniformset.cpp:17`); the painter's re-bind on resize
  (`painter.cpp:180`) keeps the first entry. It works only because the range is
  identical.
- Backends planned for other shading languages have no GL-style introspection
  to lean on. WGSL, Metal and HLSL bind by slot; names are not a portable
  contract there.

## Goals

- One declaration per program that states every input and is the contract for
  all backends.
- Deterministic layouts computed by `ase`, so a caller never hand-counts bytes.
- Loud failure on any mismatch between declaration, shader and caller, in debug
  builds on backends that can check; never a silent drop.
- Two ways to set constants, both resolved against the same declaration: a typed
  struct for engine shaders, a by-name setter for data-driven parameters.
- Headless tests that can assert vertex layouts and uniform contents.
- No string lookups on the per-draw path.
- Backends state which shader languages they accept, so a caller can ship
  several variants of one shader and pick at runtime without guessing.

## Non-goals

- A shader compiler, cross-compiler or shading language of our own inside
  `ase`. Sources stay backend-specific bytes; only their interface and language
  are described portably. Compilation and cross-compilation happen in a
  separate component above `ase`, at build time or at runtime (see "Shader
  languages" and "Vulkan readiness").
- Runtime discovery of what a shader wants. The caller declares, the backend
  checks. Producing a declaration from shader source is the compiler
  component's job, never a backend's (see "Reflection").
- Push constants, SSBOs, images or compute. Blocks, attributes and 2D samplers
  cover the renderer and the effect plan.

## The declaration

A `ShaderInterface` is a plain value, built in C++ next to the shader sources and
passed at **program** creation together with the compiled shader objects:

```cpp
ase::ShaderInterface iface;
iface.attribute("pos",   0, ase::TypeFloat, 3)
    .attribute("color", 1, ase::TypeFloat, 4)
    .block("MatrixBlock", 0, ase::BlockLayout::Std140, {
        { "worldViewProj", ase::UniformType::Mat4 },
    })
    .sampler("atlas", 0);

auto program = context.makeProgram(vs, fs, iface);
```

It belongs to the program, not to a shader, because the interface spans stages
(an attribute is a vertex-stage input, a block may be read by both stages) and
bindings are program-level state: GL applies attribute locations before link
and block and sampler bindings after link, and the other APIs express the same
thing as a pipeline or bind-group layout. Shader objects therefore stay plain
per-stage compile artifacts with no interface of their own. The declaration is
**mandatory**, not an optional hint: every backend consumes it, so a program
behaves identically everywhere and there is no "works on GL, empty on dummy"
path.

It holds three lists:

- **Attributes**: name, location, component type and count. The location is the
  contract; the name is what the shader source calls it.
- **Blocks**: name, binding, layout rule, and members with name, type and
  optional array size. Member offsets, strides and the block size are
  **computed by `ase`** from the types and the layout rule. std140 is fully
  specified by the GL spec, so this is a table lookup plus alignment, not a GPU
  query; `GL_UNIFORM_OFFSET` is never asked. Other rules (std430, Metal's,
  WGSL's) plug in per backend without touching callers.
- **Samplers**: name, unit, sampler kind (2D for now).

Rules:

- **The declaration is the contract.** The shader source must agree with it; the
  caller's data must agree with it. Neither side is inferred from the other.
- **Slots are the portable part, names are the convenience.** Every attribute,
  block and sampler carries an integer slot. Backends bind by slot. Names exist
  so that C++ and shader text can refer to the same thing and so that
  data-driven callers can set a member without knowing its offset.
- **Names resolve once.** Pipeline creation turns attribute names into locations
  and uniform-set creation turns member names into offsets. After that, draws
  carry integers only.
- **Unknown is an error.** A declared attribute the shader lacks, a declared
  member the shader lays out differently, a `set()` of an undeclared name, a
  draw with fewer textures than declared samplers: all fail in debug builds.
  Nothing is dropped.

## Shader languages

A backend reports the shader languages it accepts as an **ordered list of enum
values**, most preferred first. Stage and language are separate enums: a stage
says what a shader does (vertex, fragment), a language says what its bytes are.

A language value carries its dialect. GLSL 330, GLSL ES 300 and GLSL 450 are
not interchangeable - different versions, different built-ins, different
layout rules - so the enum has **one value per dialect** (`Glsl330`,
`GlslEs300`, `Glsl450`, `SpirV`, `Wgsl`, `Msl`, ...) rather than a language plus
a free version number. A single enum keeps the list comparable with `==`,
makes "which variants do I have" a lookup in a map keyed by one value, and
rules out combinations that do not exist (there is no "SPIR-V 3.30"). The cost
is a new enumerator when a dialect is added, which is exactly the point where
new shader variants have to be produced anyway.

Shader creation takes the stage, the language and the source **as bytes**,
since SPIR-V is binary and text sources are just bytes that happen to be
printable. A backend handed a language it did not list fails loudly at
creation; it never guesses or transpiles.

```cpp
for (auto language : context.getShaderLanguages())
    if (auto const* variant = shaders.find(language))
        return context.makeShader(ase::ShaderStage::Fragment, language,
                variant->bytes);
throw ase::UnsupportedShaderLanguage(...);
```

The caller walks the backend's list and supplies the first variant it has.
Where the variants come from is not `ase`'s concern. `avg`'s fixed shaders
(the painter's) are **authored once** in GLSL and can be **compiled ahead of
time** to every target dialect (GLSL 330 for desktop GL, GLSL ES for GLES,
SPIR-V, MSL, WGSL) with the outputs committed, so no shader tooling runs for
them at application start or in CI. `avg`'s effect shaders are generated as
GLSL text at runtime and cannot be precompiled; they go through the compiler
component described under "Vulkan readiness", which produces bytes for
whichever language the backend listed. `avg` picks the language from its own
preference list (SPIR-V first, then GLSL 330) intersected with the backend's
list, and obtains the bytes and the declaration for the chosen language
through one function, so the component has a single seam to replace. Today
there is one backend and one dialect, GLSL 330, and that function is a GLSL
passthrough over hand-written declarations.

## What each backend does with it

Every backend consumes the declaration; none may ignore it.

**OpenGL.** Before link, `glBindAttribLocation` for every declared attribute.
After link, `glUniformBlockBinding` for every block (looked up by name with
`glGetUniformBlockIndex`, which is already loaded,
`src/ase/src/gl/glfunctions.h:58`) and `glUniform1iv` for every sampler
(loaded at :18). The shader no longer needs
`layout(binding)` or the 420pack extension; if it keeps them, they must agree
with the declaration and the validator says so when they do not.

In debug builds the GL backend then compares what it compiled against the
declaration (see "Reflection"); a mismatch is a hard error with the program
name, the member and both values. In release the comparison compiles out.
Introspection thus moves from source of truth to assertion.

**Dummy.** Stores the declaration and nothing else. `DummyPipeline` keeps the
resolved vertex spec, `DummyUniformSet` records its bindings, and uploads keep
their bytes in the command buffer, so a headless test can read back "attribute
`pos` at location 0, 3 floats" or "member `worldViewProj` of draw 7 equals this
matrix".

**Vulkan, WebGPU, Metal.** Need the declaration as their pipeline layout,
bind-group layout or argument table; without it a pipeline cannot be created at
all. Nothing is applied to the shader after the fact, so the declaration is the
only source, which is why it is mandatory rather than inferred.

The declaration carries no descriptor-set numbers and no stage flags, and
needs neither. The intended Vulkan convention is that uniform blocks occupy
set 0 and samplers set 1, each at its declared binding or unit, and that every
binding is visible to both stages, which are GL's semantics. The compiler
component (see "Vulkan readiness") emits the matching decorations into the
SPIR-V, so a Vulkan backend builds its descriptor-set and pipeline layouts
from the declaration alone and the GL backend, which has separate namespaces
for blocks and samplers, ignores the sets.

## Reflection

"Introspection" today means two different things, and the design separates
them.

**(a) Reflecting a compiled program** is an optional backend capability:
`Program::reflect()` returns the interface the backend actually compiled, or
nothing. GL implements it with the introspection calls that exist now
(`src/ase/src/gl/glprogram.cpp:112-139`, extended to blocks and samplers). The
dummy backend returns nothing. A SPIR-V backend returns nothing too: it could
reflect the binary on the CPU, but there is no reason to pay for that at runtime
when the declaration already says the same thing. The **only** runtime use of
`reflect()` is the debug-build comparison against the declaration, failing
loudly on any difference. Nothing in the renderer or in `avg` depends on it
being present, so a backend that cannot reflect loses a check, not a feature.

**(b) Producing a declaration from a shader** is a per-language problem, not a
per-backend one. GLSL can only be reflected by compiling it on a live GL
context; SPIR-V reflection is pure CPU; WGSL and MSL need their own tooling.
That work belongs in the **compiler component** above `ase` (see "Vulkan
readiness"), never in a backend: it compiles the authored GLSL to SPIR-V,
reflects the SPIR-V, and returns the target-language bytes together with the
declaration they satisfy, so the two agree by construction. Run ahead of time,
when a shader changes, it can also emit the declaration and the matching C++
struct for the typed-block path into committed source. `ase` itself never
derives a declaration from bytes, so dummy and GL see the same declaration
whichever way it was produced.

Together: one component is the source of truth for both the shader variants
and their declarations; the backends consume both and GL additionally
double-checks in debug builds.

## Setting constants: two styles, one declaration

Engine shaders have a fixed, known layout and a handful of members; effect
passes have parameters that arrive by name from the render tree and animate.
Both resolve against the same block declaration, so the two styles never
disagree about offsets.

**Typed block.** A C++ struct mirrors the block. `UniformBlock<T>` checks the
struct's size against the computed block size at construction and, per member
named with `blockMember<&T::m>`, the member's offset and size against the
computed layout; every declared member must be named. Writes are plain field
assignment.

```cpp
struct MatrixBlock { ase::Matrix4f worldViewProj; };

ase::UniformBlock<MatrixBlock> matrices(iface, "MatrixBlock", {
    ase::blockMember<&MatrixBlock::worldViewProj>("worldViewProj"),
});

matrices->worldViewProj = projection;
auto range = ring.write(matrices);   // copies the struct into this frame's ring
set.bindUniformBufferRange("MatrixBlock", range);
```

`blockMember` takes a member pointer rather than an `offsetof` macro, so it
works for members with constructors (Eigen types) and needs no preprocessor.
std140 padding mistakes in the struct are caught by the offset and size checks
against the computed layout (a `Matrix3f` member for a `mat3` fails, since the
block pads each column to 16 bytes), and on GL by the debug validator against
the compiled program. This is the fast path for the painter and for built-in
effect shaders.

**By-name.** A `UniformValues` object created from a block declaration holds a
CPU shadow of the block, zero until set. `set(name, value)` looks the name up
in the block's member list, checks the type (`UniformTypeOf<T>` maps `float`,
`Vector2f`..`Vector4f`, `Matrix2f`..`Matrix4f`, `int32_t`/`uint32_t` and
`std::array` integer vectors; an array member takes a `std::vector` of exactly
its length), and writes at the declared offset, padding matrix columns to
16 bytes. The effect scheduler fills pass parameters this way from animated
render-tree values without a struct per effect.

```cpp
ase::UniformValues params(iface, "BlurParams");
params.set("radius", 12.0f);
params.set("direction", ase::Vector2f(1, 0));
params.set("weights", std::vector<float>{ 0.5f, 0.25f });
set.bindUniformBufferRange("BlurParams", ring.write(params));
```

Both produce the same bytes for the same block; a test can build one of each
and compare.

## Uploading and binding

**Per-frame ring buffer.** Instead of `pushUpload` into a dedicated
`UniformBuffer` per block, the renderer owns a `UniformRing`: one
`UniformBuffer` per frame in flight (a constructor parameter, default 3, see
"Open questions"). `beginFrame()` moves to the next frame's buffer; `write()`
copies a block's bytes to the next aligned slot of that buffer and returns a
range; the draw's `UniformSet` binds that range to the block's slot.
`glBindBufferRange` already does exactly this (`glrenderstate.cpp:512`) and
needs no change. `endFrame(uploads)` pushes the frame's single upload of the
written span into `uploads`, which the owner submits ahead of the frame's draw
commands: the GL queue executes a command buffer in order, so the upload has to
precede the draws that bind into it, and the ring cannot know the span until
the draws are recorded. A constant change is thus no longer a fence and
batching is unaffected. Ranges start at multiples of
`RenderContext::getUniformBufferOffsetAlignment()`, which GL reads from
`GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT` at context creation and the dummy fixes
at 256, the strictest value GL permits. The size per frame is the sum of block
sizes rounded to that alignment, a few kilobytes for the renderer and a few
hundred bytes per effect pass.

**UniformSet from the declaration.** A set is created by
`RenderContext::makeUniformSet(iface)` and knows the interface's blocks.
`bindUniformBufferRange` takes the block's name or its declared binding; a
rebind **overwrites** the previous range on every backend (the insert-only
behaviour of `gluniformset.cpp:17` is gone). Binding a block the interface
does not declare, or a range smaller than the block, throws
`std::invalid_argument`. The checks run at bind time in the `UniformSet`
handle; the GL draw path still iterates integer bindings only.

**Textures stay positional.** The draw's texture vector is ordered by declared
sampler unit: index i is bound to unit i, exactly as today
(`glrenderstate.cpp:465`), and the declaration states which sampler lives at
unit i. Nothing checks yet that the vector has at least as many entries as
there are declared samplers. A by-name helper that builds the vector from
`{name, texture}` pairs is a thin convenience on top.

**Vertex specs.** `NamedVertexSpec` loses its dependency on `Program`: it is
resolved against the declaration's attribute list, so the same spec resolves
identically on GL and dummy. Pipelines keep the resolved spec and the dummy one
exposes it.

## Cross-language note

WGSL (`@group(g) @binding(b)`), Metal (`[[buffer(n)]]`, `[[texture(n)]]`) and
HLSL (`register(bN)`, `register(tN)`) all bind by integer slot and offer no
runtime name lookup comparable to GL's. The declaration above is slot-first for
that reason: a backend for any of them binds by the declared integers and
ignores the names, while GLSL callers keep names in their shader text and let
the GL backend apply the slots. The by-name setter costs nothing at draw time on
any backend because it resolves at creation.

## Vulkan readiness

A read-only investigation of `ase` against Vulkan (2026-11-11) found the
abstraction already Vulkan-shaped where it matters - a CPU-recorded command
buffer translated at submit, a declared interface with integer slots, a
per-frame ring, per-window backpressure with a status-returning present - and
a handful of places where GL assumptions leak through the public API. The
owner settled those as follows (2026-11-11/12). None of this is Vulkan code;
it pins the contract so a Vulkan backend can be written without reopening the
API.

**No backend thread in the contract.** The GL backend executes submissions on
a dedicated thread only because a GL context is bound to one thread; the dummy
backend runs `submit` inline. Neither is the contract. The contract is that a
submission may execute now or later, and that the backend owns the lifetime
of everything a submitted frame references - buffers, textures, framebuffers,
uniform ranges - until that frame completes, where completion is the frame
fence and the number of frames in flight is bounded by the window's existing
`canAcquire` budget. A Vulkan backend records and submits on the calling
thread; if several threads submit, the per-queue external synchronisation
Vulkan requires lives inside the backend. Callers may assume neither that a
backend thread exists nor that it does not.

**Shader compilation lives above `ase`.** `ase` keeps the language list,
byte-based shader creation and the declared interface, and never compiles or
transpiles. A separate compiler library, placed between `ase` and `avg` in the
dependency order, takes GLSL source and a target language and returns the
bytes for that language together with the `ShaderInterface` obtained by
reflecting the SPIR-V it compiled on the way. glslang and SPIRV-Cross are its
subprojects; the dummy backend and `ase` never link them. For GL 3.3 the
component passes GLSL 330 through or cross-compiles to it.

Two facts force this shape. SPIR-V bakes its binding decorations into the
module, so a backend cannot remap them after the fact the way GL does with
`glBindAttribLocation` and `glUniformBlockBinding`; when the component that
emits the module also emits the interface, the two agree by construction. And
`avg` generates its effect shaders as GLSL text at runtime, which stays valid
on every backend only if something at runtime can turn GLSL into the backend's
language. This refines the earlier offline-only position rather than reversing
it: ahead-of-time compilation with committed outputs remains the right path
for the painter's fixed shaders, the component is the runtime path, and the
same component serves as the offline tool.

**One queryable clip-space adjustment; every other convention fixed.**
`ase`'s API conventions are GL's: scissor rectangles have a bottom-left
origin, texel rows are uploaded bottom-up, viewports follow GL. Backends absorb
these where the conversion is free or one-time; on Vulkan that is a negative
viewport height, CPU arithmetic on the scissor rectangle and row order at
upload. The one thing a backend cannot fix after the fact is the clip-space
depth range and Y direction baked into the caller's projection matrix, so the
render context exposes a single clip-space adjustment matrix - identity on
GL; a Y flip and z' = z * 0.5 + 0.5 on Vulkan - which `avg`'s painter
multiplies into its projection once, when the projection is built
(`Painter::setSize`). Fragment-coordinate sampling inside effect shaders needs
no adjustment: within one backend the written and the sampled texture share
one memory orientation, so it is self-consistent. Only the painter's
projection carries the adjustment: the effect pass vertex shader
(`passVertexSource`) and the painter's present quad emit GL clip coordinates
directly, so a backend whose adjustment is not the identity needs those two
paths adjusted before it can rely on the matrix.

**Pipelines declare their target formats.** Implemented in `4a31efc0`. The
pipeline description is constructed from a required colour format and an
optional depth format (a sample count can join them later), passed at pipeline
creation; there is no `makePipelineWithBlend`, since callers build a
description anyway. Vulkan, Metal and WebGPU all need the attachment formats
at pipeline creation, Vulkan even with dynamic rendering, so a Vulkan backend
creates its pipeline object eagerly at creation with no lazy per-format
variants and no first-draw hitch. GL and dummy ignore
the formats for state and, in debug builds, validate the bound framebuffer
against them at draw time. There is no GL-side cache of format-only variants:
a GL pipeline is a shared program handle plus a few bytes of state, nothing
worth caching. Deduplicating programs across formats is `avg`'s job, in its
`PassPipelineCache`, which keys programs by shader source and pipelines by
program, geometry and format. A corollary of the same reasoning:
a texture's size and format are fixed at creation, not mutable through upload;
a full upload asserts that they match.

**`FORMAT_SRGB` is removed.** The three-byte RGB8 format has no Vulkan
mapping (it is not a required format for anything) and nothing uses it outside
the format tables and one unit test. It goes rather than being emulated, so
the exhaustive format switches stay honest.

**Swapchain recreation is backend-internal.** `PresentStatus` does not gain
suboptimal or out-of-date values. A Vulkan window recreates its swapchain
inside present or resize without the caller knowing, exactly as the GL window's
default framebuffer resizes itself; `avg` and `bqui` keep ignoring the status.
The status stays for failures the caller must act on, such as a lost device,
which is what it was introduced for.

**`avg` selects its shader language through one seam.** Instead of hardcoding
GLSL 330, `avg` walks a preference list - SPIR-V first, then GLSL 330 -
against the languages the render context lists and takes the first match. One
`avg` function takes the chosen language and returns the shader bytes together
with the `ShaderInterface`; today it is a GLSL passthrough over the
hand-written declarations. The compiler component replaces that one function
when Vulkan work starts and not before, so glslang and SPIRV-Cross are not a
dependency of any current build.

**What exists.** No Vulkan backend is implemented. The enablers are: the
pipeline formats (`4a31efc0`) and, in `3ceaee0c`, texture size and format
fixed at creation, the clip-space adjustment matrix
(`RenderContext::getClipSpaceAdjustment`) consumed by the painter in
`setSize`, the language-selection seam (`src/avg/src/shadersource.h`), the
`FORMAT_SRGB` removal, the submission and lifetime contract on
`RenderQueue::submit`, and dummy-backend tests for each. The compiler
component and the backend itself are not started.

## Migration

1. `ase`: add the stage and language enums, the backend's language list and
   byte-based shader creation (GL lists `Glsl330`; dummy accepts every value),
   `ShaderInterface`, layout computation (std140), `UniformBlock<T>`,
   `UniformValues`, the ring buffer, the declaration-aware `UniformSet`,
   `NamedVertexSpec` resolution and `makeProgram` taking the declaration. GL
   applies and, in debug, reflects and compares; dummy stores. Unit tests for
   the layout computation (every std140 rule: scalars, vec3 padding, mat4,
   arrays of scalars with 16-byte stride) and for the language query.
2. `avg` painter: declare `MatrixBlock` and the `pos`/`color` attributes (plus
   the text pipeline's `uv` attribute and `atlas` sampler, which the font-atlas
   work added),
   select the shader variant through the language list, switch to the typed
   block and the ring buffer, drop `pushUpload` on resize and the 420pack
   qualifiers. Headless `renderingtest` gains assertions on the vertex spec and
   the uploaded matrix.
   The compiler component is not part of this migration; it arrives with the
   first second-language backend, and until then the declarations are written
   by hand.
3. Backdrop-effect plan, stages 4-6: pass uniform blocks are `UniformValues`
   filled by the scheduler; built-in effects may use typed blocks. Multi-sampler
   passes declare their units; no shader-side bindings needed.
4. Remove the old path: `Program::getAttribLocation`/`getUniformLocation`
   become backend-private validator helpers; `UniformBuffer::setData` goes with
   the per-block buffers.
5. Vulkan readiness enablers (see "Vulkan readiness"): the colour and depth
   formats on `PipelineDesc` with `makePipelineWithBlend` removed, texture
   size and format fixed at creation, the clip-space adjustment query
   consumed by the painter, the language-selection seam in `avg`, the
   `FORMAT_SRGB` removal, the submission and lifetime contract in the
   queue's documentation, and dummy tests for each. Done (`4a31efc0`,
   `3ceaee0c`); the compiler component stays deferred until Vulkan work
   starts.

Each step is its own PR. Steps 1 and 2 are in: the painter declares its
interface, keeps the projection in a typed block, writes it through a ring
once per frame and submits that upload ahead of the frame's draw buffer, and
its GLSL carries no binding qualifiers. The old per-block `UniformBuffer`
path and the introspection accessors stay available until step 4. Step 5 is
in, minus the deferred compiler component.

## Alternatives considered

**Keep introspection as the source of truth** (extend today's model: look up
block offsets with `GL_UNIFORM_OFFSET`, samplers with `glGetUniformLocation`).
Cheapest to build, but it is GL-only by construction, it cannot work on the
dummy backend without faking a GL program, and it inverts the dependency: the
shader text decides the layout and C++ discovers it at runtime, so a shader
edit silently changes what the caller must send.

**Slot-only API, no names** (option 1 in the discussion). A C++ struct per
block, integer slots everywhere, names never leave the shader. Simplest and
fastest, and it is what the portable backends want. Rejected on its own because
the effect plan needs parameters set by name from animated render-tree values,
and a struct per user-defined effect is not acceptable. It survives as the
typed-block half of this design.

**By-name dynamic API only** (option 2). Everything through `set(name, value)`.
Covers the effect case but makes the painter's hot path pay a lookup per
member per frame, or forces callers to cache offsets by hand, which is the
current hand-computed-bytes problem with extra steps. It survives as the
`UniformValues` half.

**Why both on one declaration.** The two uses are genuinely different: engine
shaders are few, fixed and performance-sensitive; effect parameters are many,
data-driven and small. Sharing the declaration and the layout computation means
they cannot drift apart, the validator covers both, and tests can cross-check
one against the other.

**A runtime "introspect for me" convenience** (declaration optional; when
absent, GL fills it in from the compiled program). Tempting for quick
experiments, rejected because it makes behaviour diverge by backend: GL would
run with a full interface while dummy, having nothing to introspect, would run
with none, which is precisely today's empty-headless-spec problem, and the
silent attribute drop would return through the back door. A declaration is
always required; a tool may write it, a backend never invents it.

**Cross-compilation inside a backend** (ship GLSL 450 or SPIR-V and let the
backend translate on the device to whatever it wants). Puts a compiler
toolchain inside `ase`, where the dummy backend and every test would carry it,
and makes the listed languages a lie: a backend would list one and quietly
accept another. Compilation happens in the component above `ase` instead,
which hands every backend bytes in a language it actually listed; the
painter's fixed shaders can still be compiled ahead of time with the outputs
committed, so they pay no startup cost and do not depend on the device's
translator.

**A finite precompiled effect set, or direct SPIR-V emission** (as the way to
get runtime-generated effect shaders onto a SPIR-V backend without a
compiler). The first gives up runtime effect generation; the second is a
second shader generator to keep in step with the GLSL one. The compiler
component keeps the one GLSL generator valid everywhere.

**A queryable convention struct** (origin, depth range, row order, exposed
for every `ase` consumer to branch on). Every scissor caller, every upload and
the per-pixel paths in effect shaders would carry a branch for a difference
the backend absorbs for free; only the projection matrix genuinely needs the
caller's cooperation, so only that is exposed. Moving `ase` itself to
top-left/[0,1] was rejected too: it touches the painter's projection, every
scissor caller and the GL backend for no gain.

**Lazy per-format pipeline variants in the backend** (a format-free
`PipelineDesc`; a Vulkan backend creates a pipeline object per attachment
format combination at first draw). A first-draw hitch per combination, a cache
keyed on state the caller knows up front, and a shape neither Metal nor WebGPU
shares. Declaring the formats costs the caller one field it already has.

**A submit thread as the contract** (every backend shaped like GL's
dispatcher). The dummy backend already breaks it and Vulkan has no use for it;
the contract only needs to say that execution may be deferred and that the
backend keeps what a submitted frame references alive until the frame
completes.

## Open questions

- **Layout rules beyond std140.** std140 suffices for GL 3.3 uniform blocks. A
  Metal or WGSL backend needs its own rules; the design makes the rule a
  property of the block, but the first implementation only ships std140.
- **Array-of-struct members.** std140 arrays of structs are well defined but
  verbose to declare. Not needed by the painter or the planned effects; left
  out until something needs them.
- **Validator reporting in release.** Debug builds assert. Whether release
  builds should log a one-line warning on mismatch, or stay silent, is open.
  Leaning towards a single warning per program at creation, never per draw.
- **Ring buffer sizing and frames in flight.** Tied to the frame model
  (`frameclock.md`): the number of ring buffers equals the number of frames
  the GPU may lag. Decided for now: a `UniformRing` constructor parameter,
  default 3. On GL the count is not a correctness matter, since each frame's
  upload is a fresh `glBufferData` and the driver orphans the storage a
  pending draw still reads; the count is for backends that map buffers
  persistently, where three covers the frame being recorded, the one the GPU
  executes and the one still presented, the common triple-buffering default.
  The ring does not wait on fences itself; an owner that needs fewer buffers
  waits on the frame fence the queue already keeps
  (`glrenderstate.cpp:276-286`) before `beginFrame`. The lifetime rule under
  "Vulkan readiness" (the backend keeps what a submitted frame references
  alive until that frame's fence) means the count only has to cover the
  `canAcquire` budget.
- **Attribute types beyond float vectors.** The painter uses floats only;
  integer and normalized attributes are declarable but untested until a user
  appears.
- **Compiler component details.** glslang plus SPIRV-Cross as subprojects of
  a component above `ase` is decided (see "Vulkan readiness"); WGSL would need
  Tint or Naga in the same place. Still open: whether the painter's fixed
  shaders ship as committed outputs or go through the component at startup,
  and whether the typed-block C++ structs it can emit are committed or
  generated at build time. Both wait for Vulkan work to start; until then the
  seam in `avg` is a GLSL passthrough and no build links a compiler.
- **Where the language enum lives.** One enum for all backends in `ase`'s public
  headers is the simplest; a backend-extensible registry is only worth it if
  out-of-tree backends appear.
