
incdir = include_directories('../../include', '../../include/reactive', '.')

libreactivesrc = [
    'widget/button.cpp',
    'widget/widgetobject.cpp',
    'widget/scrollbar.cpp',
    'widget/scrollview.cpp',
    'widget/bin.cpp',
    'widget/textedit.cpp',
    'widget/label.cpp',
    'widget/theme.cpp',
    'widget/focusgroup.cpp',
    'debug/drawkeyboardinputs.cpp',
    'shapes.cpp',
    'uniformgrid.cpp',
    'layout.cpp',
    'vbox.cpp',
    'hbox.cpp',
    'stack.cpp',
    'keyboardinput.cpp',
    'inputarea.cpp',
    'widget.cpp',
    'rendering.cpp',
    'window.cpp',
    'app.cpp',
    'glxapp.cpp',
    'connection.cpp',
    'annotation.cpp',
    'clickevent.cpp',
    'pointerbuttonevent.cpp',
    'pointermoveevent.cpp',
    'simplesizehint.cpp',
    'stacksizehint.cpp',
    'growsizehint.cpp',
    'sizehint.cpp',
]

reactivedep = [libbtldep, libasedep, libavgdep]

libreactive = library(
        'reactive',
        libreactivesrc,
        include_directories: incdir,
        dependencies: reactivedep,
        cpp_args: ['-Wno-unused-function', '-DREACTIVE_EXPORT_SYMBOLS'],
        gnu_symbol_visibility: 'hidden'
        )

libreactivedep = declare_dependency(
        link_with: libreactive,
        dependencies: reactivedep,
        include_directories: include_directories('../../include')
        )

