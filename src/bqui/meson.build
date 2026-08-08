libbquisrc = [
  'src/agent/introspectionjson.cpp',
  'src/agent/session.cpp',
  'src/agent/streamtransport.cpp',
  'src/agent/pipetransport.cpp',
  'src/agent/pipelistener.cpp',
  'src/agent/sockettransport.cpp',
  'src/agent/socketlistener.cpp',
  'src/agent/tcptransport.cpp',
  'src/agent/tcplistener.cpp',
  'src/agent/transportdispatch.cpp',
  'src/modifier/addwidgets.cpp',
  'src/modifier/background.cpp',
  'src/modifier/clip.cpp',
  'src/modifier/drawkeyboardinputs.cpp',
  'src/modifier/focusgroup.cpp',
  'src/modifier/focuson.cpp',
  'src/modifier/foreground.cpp',
  'src/modifier/frame.cpp',
  'src/modifier/margin.cpp',
  'src/modifier/onpointerdown.cpp',
  'src/modifier/onpointermove.cpp',
  'src/modifier/onpointerup.cpp',
  'src/modifier/onclick.cpp',
  'src/modifier/onhover.cpp',
  'src/modifier/onkeyevent.cpp',
  'src/modifier/ontextevent.cpp',
  'src/modifier/setgravity.cpp',
  'src/modifier/setid.cpp',
  'src/modifier/setmaximumsize.cpp',
  'src/modifier/setminimumsize.cpp',
  'src/modifier/setsize.cpp',
  'src/modifier/setsizehint.cpp',
  'src/modifier/setwidgetintrospection.cpp',
  'src/modifier/trackfocus.cpp',
  'src/modifier/transform.cpp',

  'src/widget/bin.cpp',
  'src/widget/button.cpp',
  'src/widget/box.cpp',
  'src/widget/constraintbox.cpp',
  'src/widget/constraintlayout.cpp',
  'src/widget/datavalue.cpp',
  'src/widget/hbox.cpp',
  'src/widget/instance.cpp',
  'src/widget/introspection.cpp',
  'src/widget/label.cpp',
  'src/widget/layout.cpp',
  'src/widget/scrollbar.cpp',
  'src/widget/scrollview.cpp',
  'src/widget/stack.cpp',
  'src/widget/textedit.cpp',
  'src/widget/uniformgrid.cpp',
  'src/widget/vbox.cpp',
  'src/widget/widget.cpp',

  'src/shape/circle.cpp',
  'src/shape/ellipse.cpp',
  'src/shape/rectangle.cpp',

  'src/widgetobject.cpp',
  'src/theme.cpp',
  'src/shapes.cpp',
  'src/keyboardinput.cpp',
  'src/inputarea.cpp',
  'src/window.cpp',
  'src/app.cpp',
  'src/connection.cpp',
  'src/annotation.cpp',
  'src/clickevent.cpp',
  'src/pointerbuttonevent.cpp',
  'src/pointermoveevent.cpp',
  'src/hoverevent.cpp',
  'src/simplesizehint.cpp',
  'src/stacksizehint.cpp',
  'src/growsizehint.cpp',
  'src/sizehint.cpp',
  'src/withanimation.cpp',
]

bquidep = [libbtldep, libbqdep, libasedep, libavgdep]

# nlohmann/json owns the agent layer's JSON-RPC envelope and serialisation. It
# is header-only and confined to the agent layer's own .cpp files (json.hpp is
# never pulled into a public header), so it is a dependency of the bqui library
# only — deliberately *not* re-exported through libbquidep, keeping its compile
# cost from spreading to dependents.
nlohmanndep = dependency('nlohmann_json', include_type: 'system',
    default_options: subproject_warnings)

# arrange is a vendored Cassowary linear-arithmetic constraint solver — UI-
# agnostic layout math (an "Eigen, not a bq"). Its variables and expressions
# appear in bqui's public layout interface (a Builder carries the box variables
# that name it to the solver), so unlike nlohmann it is re-exported through
# libbquidep and propagates to dependents — the one solver-math dependency
# permitted in bqui's public surface, alongside avg.
arrangedep = subproject('arrange',
    default_options: subproject_warnings).get_variable('arrange_dep').as_system()

# The TCP transport uses Winsock, which needs an explicit link to ws2_32 on
# Windows; POSIX sockets need no extra link.
bqui_link_args = []
if target_machine.system() == 'windows'
  bqui_link_args += ['ws2_32.lib']
endif

libbqui = library(
  'bqui',
  [libbquisrc],
  include_directories: include_directories('include', 'src'),
  dependencies: bquidep + [nlohmanndep, arrangedep],
  cpp_args: ['-DBQUI_EXPORT_SYMBOLS'],
  link_args: bqui_link_args,
  gnu_symbol_visibility: 'hidden'
  )

libbquidep = declare_dependency(
  link_with: libbqui,
  dependencies: bquidep + [arrangedep],
  link_args: bqui_link_args,
  include_directories: include_directories('include')
  )

bquitestsrc = [
  'test/apptest.cpp',
  'test/collectiontest.cpp',
  'test/constraintlayouttest.cpp',
  'test/databindtest.cpp',
  'test/headlessapptest.cpp',
  'test/introspectionjsontest.cpp',
  'test/introspectiontest.cpp',
  'test/layouttest.cpp',
  'test/sessiontest.cpp',
  'test/shapetest.cpp',
  'test/transporttest.cpp',
  'test/widgetinstancemodifiertest.cpp',
  'test/widgettest.cpp',
  'test/windowtest.cpp',
  ]

# The agent tests speak framed JSON-RPC and exercise the introspection->json
# adapter directly, so the test binary sees nlohmann and the library's private
# src headers (the adapter is a private header, not part of the public surface).
bquitest =  executable('bquitest',
  bquitestsrc,
  dependencies: [gtestdep, libbquidep, nlohmanndep, arrangedep],
  include_directories: include_directories('src'),
  )

test('bqui', bquitest, timeout: 1200,
  args: ['--gtest_output=xml:' + meson.current_build_dir() / 'test_detail.xml'],
  workdir: meson.project_source_root())
