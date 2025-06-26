btl_args = []
if get_option('tracy_enable')
  btl_args = ['-DTRACY_ENABLE']
endif

libbtldep = declare_dependency(
        include_directories: include_directories('include'),
        dependencies: [],
        compile_args: btl_args,
        )

btltest = executable('btltest',
  'test/asynctest.cpp',
  'test/btlcollectiontest.cpp',
  'test/btlreducetest.cpp',
  'test/clonetest.cpp',
  'test/invoketest.cpp',
  'test/lrucachetest.cpp',
  'test/pmrtest.cpp',
  'test/resulttest.cpp',
  'test/sharedtest.cpp',
  'test/tupletest.cpp',
  'test/typelisttest.cpp',
  'test/uniquetest.cpp',
  'test/utf8test.cpp',
  dependencies: [libbtldep, gtestdep],
  )

test('btl', btltest, timeout : 1200, args: ['--gtest_output=xml'])
