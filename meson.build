project('reactive', 'cpp', default_options: ['cpp_std=c++17', 'warning_level=3'])

if target_machine.system() == 'windows'
    add_global_arguments('/wd4251', '/bigobj', '/UNICODE', language : 'cpp')
endif

eigen = subproject('eigen')
libeigendep = eigen.get_variable('eigen_dep')

freetype = subproject('freetype2', default_options: ['png=disabled', 'harfbuzz=disabled', 'zlib=disabled', 'tests=disabled', 'brotli=disabled'])
libfreetypedep = freetype.get_variable('freetype_dep')

mapbox = subproject('mapbox')
mapboxdep = mapbox.get_variable('mapbox_dep')

subdir('src')
subdir('test')

