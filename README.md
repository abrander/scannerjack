Scannerjack
===========

Scannerjack is a simple GTK+ application to edit channel settings on classic
Uniden scanners.

![screenshot-2](https://user-images.githubusercontent.com/3726205/29249365-26f3b44e-802e-11e7-9580-4add4ab7926c.png)

Compatibility
=============

Scannerjack is tested with an Uniden UBC 780 XLT, but it should work
with at least the following scanners:

- BC245XLT / UBC245XLT
- BC250D / UBC250D
- BC780XLT / UBC780XLT
- BC785XLT / UBC785XLT
- BC895XLT / UBC895XLT

Scannerjack is *not* compatible with modern (GPS-enabled) Uniden scanners.

Compiling and developing
========================

For compiling Scannerjack GTK+ v3, gcc and make is required. On Ubuntu Linux
this can be installed from the Ubuntu repositories with
`sudo apt-get install libgtk-3-dev gcc make`.

Compiling the application should be as simple as typing `make`.

When developing it can be useful to open the GTK+ inspector like this:

    make && GTK_DEBUG=interactive ./scannerjack

Running
=======

Scannerjack is hardcoded to use `/dev/ttyUSB0` at 19.200 baud. If this doesn't
match your setup, you will need to modify the source code before Scannerjack
will be usable.

Please note that your user will need access to the serial device. Usually this
can be accomplished by adding your user to the `dialout` group or using `sudo`
to start the binary. You can verify acess rights in the shell by piping nothing
to the device `>/dev/ttyUSB0`. If you miss the needed rights, your shell will
output an error like "bash: /dev/ttyUSB0: Permission denied".

License
=======

The user is allowed to do anything with the Scannerjack. Should the user
of the product meet the author and consider the software useful, he is
encouraged to buy the author a beer.
