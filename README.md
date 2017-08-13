Scannerjack
===========

![screenshot-1](https://user-images.githubusercontent.com/3726205/29249039-86a1cf36-8027-11e7-82fb-5b248e7d04c8.png)

Scannerjack is a simple GTK+ application to edit channel settings on classic
Uniden scanners.

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

For compiling Scannerjack GTK+ v3, gtk and make is required. On Ubuntu Linux
this can be installed from the Ubuntu repositories with
`sudo apt-get install libgtk-3-dev gcc make`.

Compiling the application should be as simple as typing `make`.

When developing it can be useful to open the GTK+ inspector like this:

    make && GTK_DEBUG=interactive sudo -E ./scannerjack

License
=======

The user is allowed to do anything with the Scannerjack. Should the user
of the product meet the author and consider the software useful, he is
encouraged to buy the author a beer.
