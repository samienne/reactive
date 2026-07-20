project('reactive', 'cpp', default_options: ['cpp_std=c++17', 'warning_level=3'])

if target_machine.system() == 'windows'
    add_global_arguments('/wd4251', '/bigobj', '/UNICODE', language : 'cpp')
endif

# Third-party headers come in as system includes so their diagnostics stay out
# of our build log without also blinding us to our own code, and the subprojects
# compile at warning level 0 rather than ours.
subproject_warnings = ['warning_level=0']

libeigendep = dependency('eigen3', include_type: 'system')

freetype = subproject('freetype2',
    default_options: [
      'png=disabled',
      'harfbuzz=disabled',
      'zlib=disabled',
      'tests=disabled',
      'brotli=disabled',
      ] + subproject_warnings)
libfreetypedep = freetype.get_variable('freetype_dep').as_system()

mapbox = subproject('mapbox', default_options: subproject_warnings)
mapboxdep = mapbox.get_variable('mapbox_dep').as_system()

gtest = subproject('gtest', default_options: subproject_warnings)
gtestdep = gtest.get_variable('gtest_main_dep').as_system()

tracydep = dependency('tracy', include_type: 'system',
    default_options: subproject_warnings)

subdir('src')

