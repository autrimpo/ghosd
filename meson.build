project('ghosd', 'c',
        default_options: ['c_std=c11'])
cc = meson.get_compiler('c')
sdl_dep = dependency('SDL2')
rt_dep = cc.find_library('rt')
executable('ghosd', 'ghosd.c', dependencies: [sdl_dep, rt_dep])
