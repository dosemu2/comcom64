# comcom64

comcom64 is a command.com-alike 64bit shell for DOS.<br/>
This repository also contains the build recipe for 32bit version.

## building

Install the needed build tools.<br/>
You can see the list of them
[for ubuntu](https://github.com/dosemu2/comcom64/blob/master/debian/control#L7-L14)
or
[for fedora](https://github.com/dosemu2/comcom64/blob/master/comcom64.spec.rpkg#L17-L24)

Then just run `make`.

To build the 32bit version, install
[djgpp](https://www.delorie.com/djgpp/)
and run `make 32`.

## installing

Running `sudo make install` installs the 64bit version
for the use with [dosemu2](https://github.com/dosemu2/dosemu2).

## running

Just run `dosemu` and it should boot the installed comcom64.

## mouse control

You can navigate the command history with mouse wheel.

All buttons have 2 functions: one activates when you click on a text
area outside of a cursor row, and another activates when you click
inside the cursor row.

Left button:
  - if Ctrl pressed: type clicked char; otherwise do nothing
  - moves the cursor to the clicked location

Middle button:
  - Enter
  - BackSpace

Right button:
  - Tab completion
  - truncate or clear line

There is a `mouseopt` command that controls mouse behavior.
It has the following switches:

 - /M - initialize mouse (if /M was not passed to comcom on start)
 - /E[1|0] - enable/disable mouse
 - /C[0|1] - enable/disable external control

External control allows to control other programs with mouse.
For example you can execute `mouseopt /c`, then run freecom and
control it with mouse similar to comcom64, even though freecom
is mouse-unaware by itself.
