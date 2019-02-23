export PATH="$PATH:/usr/local/cross/bin"
if ! which i586-pc-msdosdjgpp-gcc 2>/dev/null ; then
    cp tmp/comcom32.exe .
    exit 0
fi
make $@
