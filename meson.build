project('comcom64', 'c', version: '0.4')

tg = dependency('thunk_gen')
TGMK = tg.get_variable('makeinc')
TG = tg.get_variable('binary')
tgbin = find_program(['thunk_gen', TG])
# working around https://github.com/mesonbuild/meson/issues/15035
if tgbin.full_path().startswith('/usr/') or tgbin.full_path().startswith(get_option('prefix'))
  tgbin = []
  message('Using standalone thunk_gen')
endif

mod = import('unstable-external_project')

mod.add_project('configure',
  verbose: true,
  depends: tgbin,
)

# Use this line to build:
# meson devenv -C build meson compile --verbose
