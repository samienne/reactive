
incdir = include_directories('include')

bqsrc = [
    'src/signal/datacontext.cpp',
  ]

libbq = library(
  'bq',
  [bqsrc],
  include_directories: include_directories('include/bq', 'src'),
  dependencies: [libbtldep],
  cpp_args: ['-DBQ_EXPORT_SYMBOLS'],
  )

libbqdep = declare_dependency(
  link_with: libbq,
  dependencies: [libbtldep],
  include_directories: include_directories('include'),
  )

bqtest = executable('bqtest',
  'test/signaltest.cpp',
  'test/streamtest.cpp',
  dependencies: [libbqdep, gtestdep],
  )

test('bq', bqtest, timeout : 1200, args: ['--gtest_output=xml'])

