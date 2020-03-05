# xine-client 

Version 1.1, February 2020

## What is this?

`xine-client` is a simple command-line utility that uses 
the `xine-server` C API to control the `xine-server` audio player.
It isn't really intended to be a functional application in its
own right, although it makes an adequate command-line audio
player. Rather, it is a demonstration how to use the 
`xine-server` API. 

The API files (there are only one C source and one header)
are in the `api` directory. These files can simply be
dropped into the source tree of a C application. 

## Usage

    xine-client [options] {command} [arguments]

## Command-line arguments

`-h,--host={hostname}`

Hostname or IP number of the `xine-server` host. The default is `localhost`.

`-p,--port={number}`

TCP port of the `xine-server` server. Default is 30001

## Commands

`add {streams...}`

Add the specified streams to the existing playlist. The playlist
position is not changed, and playback is not started if it has not
started already.

`clear`

Clear the playlist and stop playback

`meta-info`

Reports meta-info about the item currently being played --
title, artist, etc. This information comes from Xine, not from
examination of the local file, and is not always accurate.

`next`

Play the next item in the playlist, if there is one

`pause`

Pauses playback. Use `play` (with no argument) to resume.

`play-now {streams...}`

Clear playlist, add the specified files or streams to the playlist, and
then play from the start of the playlist.

`play [N]`

With no arguments, does the opposite of `resume` -- resumes paused
playback. With an argument, starts playback at the specified playlist
entry (starting at zero).

`playlist`

Prints the playlist

`prev`

Play the previous item in the playlist, if there is one

`seek {sec}`

Moves the playback position in the current stream to `sec`. If
this position is larger than the length of the stream, playback
stops. If there is another item in the playlist, playback resumes
with that item. In short, it is possible to fast-forward from
one playback item to the next. 

`shutdown`

Shut down the server.

`status`

Reports the server's playback status, including the current
stream, and the playback position in the stream.

`stop`

Stops playback, and resets the playlist index to 'nothing'. 

`volume [V]`

With an argument, sets the volume, in range 0..100. Without an argument,
reports the current volume setting. Note that this value will not
be reliable until the server has played some audio. This is a limitation
of Xine.

## Author and legal

`xine-client` is copyright (c)2020 Kevin Boone, and distributed under the
terms of the GNU Public Licence, version 3.0. Essentially that means
you may use the software however you wish, so long as the source
code continues to be made available, and the original author is
acknowledged. There is no warranty of any kind.





