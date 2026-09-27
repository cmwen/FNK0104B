# Scripts

`use_standalone_gdb.py` selects PlatformIO's pinned standalone Xtensa GDB for
the `hello-debug` environment. The release firmware environments do not use it.

`99-fnk0104b-jtag.rules` grants this host's `plugdev` group access to the
board's observed USB JTAG ID. See `docs/development.md` for installation and
verification; it is not applied by a normal build.
