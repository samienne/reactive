gtest_dep = dependency(
  'gtest_main',
  fallback: ['gtest', 'gtest_main_dep'],
  required: true,
)

arrange_test = executable(
  'arrange_test',
  files(
    'constraint_test.cpp',
    'diff_test.cpp',
    'expression_test.cpp',
    'layout_example_test.cpp',
    'smoke_test.cpp',
    'solver_test.cpp',
    'strength_test.cpp',
    'variable_test.cpp',
  ),
  dependencies: [arrange_dep, gtest_dep],
)

test('arrange', arrange_test, protocol: 'gtest')
