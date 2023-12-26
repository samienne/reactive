incdir = include_directories('../include')
gtest = subproject('gtest')
gtestdep = gtest.get_variable('gtest_main_dep')

config_file = configure_file(
  input: 'config.h.in',
  output: 'config.h',
  configuration: { 'DATA_PATH': join_paths(meson.project_source_root(), 'data') }
  )

devtest = executable('devtest',
        'filetest.cpp',
        include_directories : incdir,
        link_with : [],
        dependencies : [gtestdep]#, libasedep, libavgdep, libreactivedep]
        )

unittest = executable('unittest',
        'asynctest.cpp',
        'animatedtest.cpp',
        'widgetinstancemodifiertest.cpp',
        'grouptest.cpp',
        'signalwraptest.cpp',
        'signalresulttest.cpp',
        'iteratetest.cpp',
        'pmrtest.cpp',
        'databindtest.cpp',
        'btlreducetest.cpp',
        'btlcollectiontest.cpp',
        'collectiontest.cpp',
        'tweensignaltest.cpp',
        'clonetest.cpp',
        'widgettest.cpp',
        'signaltest.cpp',
        'resulttest.cpp',
        'tupletest.cpp',
        'utf8test.cpp',
        'test1.cpp',
        'pathtest.cpp',
        'avgtransformtest.cpp',
        'fonttest.cpp',
        'blipsignaltest.cpp',
        'functionaltest.cpp',
        'joinsignaltest.cpp',
        'foldpsignaltest.cpp',
        'inputsignaltest.cpp',
        'mapsignaltest.cpp',
        'countsignaltest.cpp',
        'timesignaltest.cpp',
        'signaltypereductiontest.cpp',
        'streamtest.cpp',
        'signalthreadedtest.cpp',
        'droprepeatssignaltest.cpp',
        'hashtest.cpp',
        'sharedtest.cpp',
        'uniquetest.cpp',
        'brushtest.cpp',
        'colortest.cpp',
        'pentest.cpp',
        'typelisttest.cpp',
        include_directories : incdir,
        link_with : [],
        dependencies : [gtestdep, libasedep, libavgdep, libreactivedep])

test('unittest', unittest, timeout : 1200, args: ['--gtest_output=xml'])
test('devtest', devtest, timeout : 1200, args: ['--gtest_output=xml'])

