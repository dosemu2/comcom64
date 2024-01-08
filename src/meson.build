MA = meson.current_source_dir() / 'parsers/mkasmdefs.sh'
PD = meson.current_source_dir() / 'parsers/parse_decls.sh'
M4 = meson.current_source_dir() / 'parsers/thunks.m4'
tg = subproject('thunk_gen')
TG = tg.get_variable('TG')
TFLAGS = ['-a', '4', '-p', '4']
TARGET = 'comcom64'

CFILES = [
    'cmdbuf.c',
    'command.c',
    'thunks_a.c',
    'thunks_c.c',
    'version.c']

gad = custom_target('glob_asmdefs.h',
  output: 'glob_asmdefs.h',
  input: 'glob_asm.h',
  command: [MA, '@INPUT@'],
  capture: true)

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
cpp = meson.get_compiler('c')
sfiles = cpp.preprocess(['plt.S'],
  output: '@BASENAME@.s',
  dependencies: declare_dependency(
    compile_args: ['-D__ASSEMBLER__']))

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

libdj = dependency('dj64')
libcc64 = shared_library('comcom64', [CFILES],
  sources: [gad, tc],
  dependencies: libdj,
  link_args: ['-Wl,-Bsymbolic'])

strip = find_program('strip')
cc64 = custom_target(TARGET + '.so',
  output: TARGET + '.so',
  input: libcc64,
  command: [strip, '-o', '@OUTPUT0@', '@INPUT@'])

ASFILES = [ 'int23.S', 'int0.S', 'asm.S', sfiles, plt_inc ]
lib = static_library('dummy', ASFILES,
  build_by_default: false)

nasm_ld = find_program(['i686-linux-gnu-ld', 'i386-elf-ld', 'x86_64-linux-gnu-ld', 'ld'])
libdjs = dependency('dj64static', static: true)
elf = custom_target(TARGET + '.elf',
  output: [TARGET + '.elf', TARGET + '.map'],
  input: [lib.extract_all_objects(recursive: true),
    '/usr/i386-pc-dj64/lib/libc.a'],
  command: [nasm_ld, '-melf_i386', '-static',
    '-Map=@OUTPUT1@', '-o', '@OUTPUT0@', '@INPUT@'])
elf_s = custom_target(TARGET + '.elf.stripped',
  output: TARGET + '.elf.stripped',
  input: elf[0],
  command: [strip, '-o', '@OUTPUT0@', '@INPUT@'])

stubify = find_program('djstubify')
ccexe = custom_target(TARGET + '.exe',
  output: TARGET + '.exe',
  input: [elf_s, cc64],
  command: [stubify, '-l', cc64.full_path(), '-o', '@OUTPUT0@',
    elf_s.full_path()],
  install: true,
  install_dir: get_option('datadir') / TARGET,
  install_mode: 'rw-r--r--')

ln = find_program('ln')
custom_target('command.com',
  output: 'command.com',
  input: ccexe,
  command: [ln, '-s', '@PLAINNAME@', '@OUTPUT0@'],
  install: true,
  install_dir: get_option('datadir') / TARGET,
  install_mode: 'rw-r--r--')
#  follow_symlinks: false)
