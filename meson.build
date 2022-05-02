project(
	'fade-layer',
	'c',
	version: '0.1.0',
	license: 'MIT',
	meson_version: '>=0.58.1',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments(
    [
        '-Wno-unused-parameter'
    ],
    language: 'c',
)

wlroots = dependency('wlroots')
wayland_egl = dependency('wayland-egl')
wayland_cursor = dependency('wayland-cursor')
wayland_client = dependency('wayland-client')
wayland_scanner = dependency('wayland-scanner')
egl = dependency('egl', required: false, disabler: true)
glesv2 = dependency('glesv2', required: false, disabler: true)
libdrm = dependency('libdrm').partial_dependency(compile_args: true, includes: true)
wayland_protos = dependency('wayland-protocols', version: '>=1.14')

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')
wayland_scanner_prog = find_program(wayland_scanner.get_variable('wayland_scanner'))

wayland_scanner_code = generator(
    wayland_scanner_prog,
    output: '@BASENAME@-protocol.c',
    arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
    wayland_scanner_prog,
    output: '@BASENAME@-client-protocol.h',
    arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

client_protocols = [
    wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
    'wlr-layer-shell-unstable-v1.xml',
]

client_protos_src = []
client_protos_headers = []

foreach filename : client_protocols
    client_protos_src += wayland_scanner_code.process(filename)
    client_protos_headers += wayland_scanner_client.process(filename)
endforeach

lib_client_protos = static_library(
    'client_protos',
    client_protos_src + client_protos_headers,
    dependencies: [wayland_client]
)

client_protos = declare_dependency(
    link_with: lib_client_protos,
    sources: client_protos_headers,
)

dependencies = [
    client_protos,
    wayland_egl,
    wayland_cursor,
    egl,
    glesv2
]

sources = [
    'egl_common.c',
    'fade-layer.c'
]

executable('fade-layer',
	sources,
	include_directories: [],
	dependencies: dependencies
)
