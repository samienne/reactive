libbquisrc = [
  'src/remote/introspectionjson.cpp',
  'src/remote/remotedriver.cpp',
  'src/remote/streamtransport.cpp',
  'src/remote/pipetransport.cpp',
  'src/remote/pipelistener.cpp',
  'src/remote/sockettransport.cpp',
  'src/remote/socketlistener.cpp',
  'src/remote/tcptransport.cpp',
  'src/remote/tcplistener.cpp',
  'src/remote/transportdispatch.cpp',
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
  'src/windowimpl.cpp',
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

nlohmanndep = dependency('nlohmann_json', include_type: 'system',
    default_options: subproject_warnings)

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
  dependencies: bquidep + [nlohmanndep],
  cpp_args: ['-DBQUI_EXPORT_SYMBOLS'],
  link_args: bqui_link_args,
  gnu_symbol_visibility: 'hidden'
  )

libbquidep = declare_dependency(
  link_with: libbqui,
  dependencies: bquidep,
  link_args: bqui_link_args,
  include_directories: include_directories('include')
  )

bquitestsrc = [
  'test/apptest.cpp',
  'test/collectiontest.cpp',
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

# apptest now drives App::run against an explicit dummy platform
# (App::platform(makeDummyPlatform())), so it runs everywhere -- it no longer
# needs the default platform to be the headless dummy (which was macOS-only).

# The remote tests speak framed JSON-RPC and exercise the introspection->json
# adapter directly, so the test binary sees nlohmann and the library's private
# src headers (the adapter is a private header, not part of the public surface).
bquitest =  executable('bquitest',
  bquitestsrc,
  dependencies: [gtestdep, libbquidep, nlohmanndep],
  include_directories: include_directories('src'),
  )

# App::headless(true) renders offscreen with the real GL backend, which needs a
# GPU the CI runners lack (the same reason apptest is gated above). This target
# is built everywhere so it keeps compiling, but is deliberately not registered
# as a test(); run it by hand on a machine with a real GL context.
executable('bquiheadlesstest',
  'test/headlessrendertest.cpp',
  dependencies: [libbquidep],
  )

test('bqui', bquitest, timeout: 1200,
  args: ['--gtest_output=xml:' + meson.current_build_dir() / 'test_detail.xml'],
  workdir: meson.project_source_root())
