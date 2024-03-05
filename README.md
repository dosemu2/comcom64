# comcom64

comcom64 is a command.com-alike 64bit shell for DOS.<br/>
This repository also contains the build recipe for 32bit version.

## building

Install the needed build tools.<br/>
You can see the list of them
[for ubuntu](https://github.com/stsp/comcom64/blob/master/debian/control#L7-L14)
or
[for fedora](https://github.com/stsp/comcom64/blob/master/comcom64.spec.rpkg#L17-L24)

Then just run `make CROSS_PREFIX=i686-linux-gnu-` on ubuntu, or
`make CROSS_PREFIX=x86_64-linux-gnu-` on fedora.

To build the 32bit version, install
[djgpp](https://www.delorie.com/djgpp/)
and run `make 32`.

## installing

Running `sudo make install` installs the 64bit version
for the use with [dosemu2](https://github.com/dosemu2/dosemu2).

## running

Just run `dosemu` and it should boot the installed comcom64.
