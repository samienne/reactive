libbquisrc = [
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
  'src/windowlist.cpp',
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

libbqui = library(
  'bqui',
  [libbquisrc],
  include_directories: include_directories('include', 'src'),
  dependencies: bquidep,
  cpp_args: ['-DBQUI_EXPORT_SYMBOLS'],
  gnu_symbol_visibility: 'hidden'
  )

libbquidep = declare_dependency(
  link_with: libbqui,
  dependencies: bquidep,
  include_directories: include_directories('include')
  )

bquitestsrc = [
  'test/collectiontest.cpp',
  'test/databindtest.cpp',
  'test/introspectiontest.cpp',
  'test/layouttest.cpp',
  'test/shapetest.cpp',
  'test/widgetinstancemodifiertest.cpp',
  'test/widgettest.cpp',
  'test/windowtest.cpp',
  ]

# App::run opens real windows, so it can only be exercised where ase builds its
# headless backend. This gate is the minimal thing needed to run apptest.cpp
# against ase's dummy backend; the proper headless platform (env-selected and
# frame-stepped) arrives in a follow-up (PR #96), and this should be revisited
# to use it once that lands.
if ase_is_headless
  bquitestsrc += 'test/apptest.cpp'
endif

bquitest =  executable('bquitest',
  bquitestsrc,
  dependencies: [gtestdep, libbquidep],
  )

test('bqui', bquitest, timeout: 1200,
  args: ['--gtest_output=xml:' + meson.current_build_dir() / 'test_detail.xml'],
  workdir: meson.project_source_root())
