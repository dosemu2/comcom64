tg = dependency('thunk_gen')
TG = tg.get_variable(pkgconfig: 'binary')
PD = tg.get_variable(pkgconfig: 'pdscript')
TFLAGS = ['-a', '4', '-p', '4']
TARGET = 'comcom64'
CMD = TARGET + '.exe'
DBG = 'dosemu_' + CMD + '.dbg'

CFILES = [
    'cmdbuf.c',
    'command.c',
    'ms.c',
    'env.c',
    'psp.c',
    'umb.c',
    'ae0x.c',
    'compl.c',
    'thunks_a.c',
    'thunks_c.c'
    ]

GEN_TMP = { '1':'thunk_calls.tmp' }

tct = custom_target('thunk_calls.tmp',
  output: 'thunk_calls.tmp',
  input: 'asm.h',
  command: [PD, '1', '@INPUT@'],
  capture: true)
plt_inc = custom_target('plt.inc',
  output: 'plt.inc',
  input: tct,
  command: [PD, '3', '@INPUT@'],
  capture: true)
libdj = dependency('dj64')
cpp = meson.get_compiler('c')
sfiles = cpp.preprocess(['plt.S'],
  output: '@BASENAME@.s',
  depends: plt_inc,
  dependencies: libdj,
  compile_args: ['-D__ASSEMBLER__'])

env = environment()
env.set('CPP', 'cpp')
env.set('srcdir', meson.current_source_dir() / 'parsers')
pd = find_program(PD)
gtgen = generator(pd,
  arguments: ['@EXTRA_ARGS@', '@INPUT@'],
  output: '@BASENAME@.tmp',
  capture: true)
gt = []
foreach n: GEN_TMP.keys()
  gt += gtgen.process('asm.h',
      extra_args: n,
      env: ['CPP=cpp', 'srcdir=' + meson.current_source_dir() / 'parsers'])
endforeach
GEN = { GEN_TMP['1']:gt[0] }

tc = custom_target('thunk_calls.h',
  output: 'thunk_calls.h',
  input: GEN['thunk_calls.tmp'],
  command: [TG, TFLAGS],
  feed: true,
  capture: true)

r = run_command(find_program('git'), 'describe', '--dirty=+', check: true)
REVISIONID = r.stdout().strip()
libcc64 = shared_library('comcom64', [CFILES],
  sources: [tc],
  c_args: ['-DREV_ID="' + REVISIONID + '"'],
  dependencies: libdj,
  link_args: ['-Wl,-Bsymbolic', '-Wl,-rpath=/usr/local/i386-pc-dj64/lib64',
    '-Wl,-rpath=/usr/i386-pc-dj64/lib64'],
#  build_rpath: ['/usr/local/i386-pc-dj64/lib64', '/usr/i386-pc-dj64/lib64']
)

objc = find_program('objcopy')
dbg = custom_target(DBG,
  output: DBG,
  input: libcc64,
  command: [objc, '--only-keep-debug', '@INPUT@', '@OUTPUT@'])

od = find_program('objdump')
# FIXME!
FLG = run_command('sh', '-c', od.full_path() + ' -T ' + libcc64.full_path() +
    ' | grep _shm_flags | sed -E "s/([^ ]+) .+/0x\\1/"',
  check: true,
  capture: true,
).stdout()

strip = find_program('strip')
cc64s = custom_target('lib.stripped',
  output: '@PLAINNAME@.stripped',
  input: libcc64,
  command: [strip, '-o', '@OUTPUT0@', '@INPUT@'])

cc64 = custom_target('lib.stripped.dl',
#  output: '@PLAINNAME0@.dl',
  output: 'lib' + libcc64.name() + '.stripped.dl',
  input: [cc64s, dbg],
  command: [objc, '--add-gnu-debuglink=@INPUT1@', '@INPUT0@', '@OUTPUT@'])

ASFILES = [ 'int23.S', 'int0.S', 'mouse.S', 'asm.S', sfiles ]
lib = static_library('dummy', ASFILES,
  build_by_default: false)

nasm_ld = find_program(['i686-linux-gnu-ld', 'i386-elf-ld', 'x86_64-linux-gnu-ld', 'ld'])
libdjs = dependency('dj64static', static: true)
elf = custom_target(TARGET + '.elf',
  output: [TARGET + '.elf', TARGET + '.map'],
  input: [lib.extract_all_objects(recursive: true),
#    libdjs.args(link_args: true)],
    '/usr/local/i386-pc-dj64/lib/libc.a'],
  command: [nasm_ld, '-melf_i386', '-static', '--whole-archive',
    '-Map=@OUTPUT1@', '-o', '@OUTPUT0@', '@INPUT@'])
elf_s = custom_target('elf.stripped',
  output: '@PLAINNAME@.stripped',
  input: elf[0],
  command: [strip, '--strip-debug', '-o', '@OUTPUT0@', '@INPUT@'])

stubify = find_program('djstubify')
ccexe = custom_target(CMD,
  output: CMD,
  input: [elf_s, cc64, dbg],
  command: [stubify, '-l', cc64.full_path(), '-l', dbg.full_path(), '-n', CMD,
    '-f', FLG, '-o', '@OUTPUT0@', elf_s.full_path()],
  install: true,
  install_dir: get_option('datadir') / TARGET,
  install_mode: 'rw-r--r--')

fs_mod = import('fs')
install_symlink('command.com',
  pointing_to: fs_mod.name(ccexe.full_path()),
  install_dir: get_option('datadir') / TARGET)
