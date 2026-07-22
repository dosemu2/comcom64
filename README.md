# comcom64

comcom64 is a command.com-alike 64bit shell for DOS.<br/>
This repository also contains the build recipe for 32bit version.

## building

Install the needed build tools.<br/>
You can see the list of them
[for ubuntu](https://github.com/dosemu2/comcom64/blob/master/debian/control#L7-L14)
or
[for fedora](https://github.com/dosemu2/comcom64/blob/master/comcom64.spec.rpkg#L17-L24)

Then just run `make`. It will build `src/comcom64.exe` and `src/32/comcom32.exe`.
`comcom32.exe` can work under any emulator, while `comcom64.exe` is currently
specific to [dosemu2](https://github.com/dosemu2/dosemu2).

Alternatively you can run `make fetch` that will download the pre-built
executables to the same aforementioned directories.

## installing

Running `sudo make install` installs both executables
for the use with [dosemu2](https://github.com/dosemu2/dosemu2).
`dosemu2` chooses 64bit version at boot if both versions are installed.

## running

Just run `dosemu` and it should boot the installed comcom64.

Alternatively
[download comcom32](https://dosemu2.github.io/comcom64/files/comcom32.zip),
unzip it and run with `dosbox-staging ./comcom32.exe`.

You can also
[download comcom64](https://dosemu2.github.io/comcom64/files/comcom64.zip)
in case you have problems building it. Put it into `~/.dosemu/drive_c`
after unzipping, and symlink as `command.com` - dosemu2 will then happily boot it.

## mouse control

You can navigate the command history with mouse wheel.

Each button have 2 functions: one activates when you click on a text
area outside of a cursor row, and another activates when you click
inside the cursor row.

Left button:
1. if Ctrl pressed: type the clicked char; otherwise do nothing
2. moves the cursor to the clicked location

Middle button:
1. Enter
2. BackSpace

Right button:
1. Tab completion
2. truncate or clear line

There is a `mouseopt` command that controls mouse behavior.
It has the following switches:

 - /M - initialize mouse (if /M was not passed to comcom on start)
 - /E[1|0] - enable/disable mouse
 - /C[0|1] - enable/disable external control

External control allows to control other programs with mouse.
For example you can execute `mouseopt /c`, then run freecom and
control it with mouse similar to comcom64, even though freecom
is mouse-unaware by itself.

## terminal emulation support

Terminal emulation is supported, and under dosemu2 you can run `unix vim`
and get `vim` properly rendered. Under other emulators you may need to use
`djterm /e1` to enable terminal support, but this is rarely needed because
djterm is not `ansi`-compatible, and `ansi` is the only DOS-relevant terminal
protocol unless running host programs via `unix.com`.
