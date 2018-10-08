project('ghosd', 'c',
        default_options: ['c_std=c11'])
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')
cc = meson.get_compiler('c')
sdl_dep = dependency('SDL2')
ttf_dep = dependency('SDL2_ttf')
rt_dep = cc.find_library('rt')
fc_dep = cc.find_library('fontconfig')
executable('ghosd', 'ghosd.c', dependencies: [sdl_dep, ttf_dep, rt_dep, fc_dep])
