# xine-server

A network-controllable audio player with a C API

Version 1.0b, February 2020

Please be aware that this is a work in progress, and may not be of
production quality.

## What is this?

`xine-server` is an audio player that can be controlled by a simple
line-by-line textual network protocol. It is based on the Xine library, and can
play most audio files and streams in widespread use. `xine-server` is intended
for use as a back-end audio player in an embedded Linux system, but may have
more general applicability -- multi-room audio, for example.  `xineserver`
builds cleanly on a Raspberry Pi, and has minimal dependencies, beyond those
used by Xine itself. 

## Contents

This repository has four parts:

`server`

This is the main `xine-server` process.

`api`

The C API which, for simplicitly, is implemented in a single C source and
a single header file.

`xine-client`

A command-line audio player that demonstrates the use of the C API.

`xsradio`

A simple, ncurses-based radio stream selector, that uses the C API.

# Where next?

Please see the README.md file in the `server` directory for more information.


## Author and legal

`xine-server` is copyright (c)2020 Kevin Boone, and distributed under the
terms of the GNU Public Licence, version 3.0. Essentially that means
you may use the software however you wish, so long as the source
code continues to be made available, and the original author is
acknowledged. There is no warranty of any kind.


