
incdir = include_directories('../../include', '../../include/reactive', '.')

libreactivesrc = [
    'widget/addwidgets.cpp',
    'widget/bin.cpp',
    'widget/button.cpp',
    'widget/clip.cpp',
    'widget/focusgroup.cpp',
    'widget/focuson.cpp',
    'widget/frame.cpp',
    'widget/instance.cpp',
    'widget/label.cpp',
    'widget/margin.cpp',
    'widget/onpointerdown.cpp',
    'widget/onpointermove.cpp',
    'widget/onpointerup.cpp',
    'widget/onclick.cpp',
    'widget/onhover.cpp',
    'widget/onkeyevent.cpp',
    'widget/ontextevent.cpp',
    'widget/setsizehint.cpp',
    'widget/scrollbar.cpp',
    'widget/scrollview.cpp',
    'widget/textedit.cpp',
    'widget/theme.cpp',
    'widget/trackfocus.cpp',
    'widget/widget.cpp',
    'widget/widgetobject.cpp',
    'debug/drawkeyboardinputs.cpp',
    'shapes.cpp',
    'uniformgrid.cpp',
    'layout.cpp',
    'box.cpp',
    'vbox.cpp',
    'hbox.cpp',
    'stack.cpp',
    'keyboardinput.cpp',
    'inputarea.cpp',
    'widget.cpp',
    'window.cpp',
    'app.cpp',
    'connection.cpp',
    'annotation.cpp',
    'clickevent.cpp',
    'pointerbuttonevent.cpp',
    'pointermoveevent.cpp',
    'hoverevent.cpp',
    'simplesizehint.cpp',
    'stacksizehint.cpp',
    'growsizehint.cpp',
    'sizehint.cpp',
]

reactivedep = [libbtldep, libasedep, libavgdep]

libreactive = library(
        'reactive',
        [libreactivesrc],
        include_directories: incdir,
        dependencies: reactivedep,
        cpp_args: ['-DREACTIVE_EXPORT_SYMBOLS'],
        gnu_symbol_visibility: 'hidden'
        )

libreactivedep = declare_dependency(
        link_with: libreactive,
        dependencies: reactivedep,
        include_directories: include_directories('../../include')
        )

