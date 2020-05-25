# libc_exec_replace
tool to match and replace exec* syscalls at runtime without modifying any binary

This project is a small library that redefines glibc functions execv() and execve() so that the user can replace some exec matching a pattern with something else.
If you have a program that is a blackbox and at some point it runs binary X and you want it run run binary Y than this might be what you need.
Other exec functions like execl are not implemented yet...

To use this thing you need to preload the .so library
and set the environment variables

- MYEXEC_MATCH

- MYEXEC_REPLACE

inside MYEXEC_REPLACE path must be absolute.

## Example:

```C
[r.pancheri@PC myexec]$ export MYEXEC_MATCH="cat"
[r.pancheri@PC myexec]$ export MYEXEC_REPLACE="/bin/echo cat replaced with echo: "
[r.pancheri@PC myexec]$ LD_PRELOAD=myexec.so bash
[r.pancheri@PC myexec]$ cat 123
cat replaced with echo: 123
[r.pancheri@PC myexec]$ 
```

if you preload instead `myexec_verbose.so` log debug stuff will be written in `$HOME/myexec.log`

Log is done with this library:
https://github.com/pymumu/tinylog

## Build Dependencies:

pthread, libld, glibc headers, wget

## How to compile:

make

## License:

MIT License
