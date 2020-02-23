# xine-server

Version 1.0a, February

## What is this?

`xine-server` is an audio player that can be controlled by a simple
line-by-line textual network protocol. It is based on the Xine library, and can
play most audio files and streams in widespread use. `xine-server` is intended
for use as a back-end audio player in an embedded Linux system, but may have
more general applicability -- multi-room audio, for example.  `xineserver`
builds cleanly on a Raspberry Pi, and has minimal dependencies, beyond those
used by Xine itself. 

A C library (consisting of a single source file) is available for
integrating the server with other C applications, as is a simple 
test client for testing the operation of the server.

`xine-server` might one day support video but, at present, Xine's
built-in video support is unsatisfactory on the kinds of platforms
`xine-server` is intended for. Although Xine _will_ play video on a Raspberry
Pi -- direct to the framebuffer, for example --  it won't benefit from
any of the optimizations available in the proprietary 
<code>omxplayer</code>. 

This version of `xine-server` has been tested primarily with Xine 1.29.

## Building

    $ make
    $ sudo make install

To run `xine-server` you will need the library <code>libxine.so</code>,
along with its rather extensive list of dependencies,
but you don't necessarily need any of the Xine front-ends (<code>gxine</code>,
for example). To build, you will need the development files as
well -- on a Raspbian system you might do

    $ sudo apt-get install libxine2-dev

On a Fedora system, you can do

    $ sudo dnf install xine-lib-devel

If you're building on an embedded system, you'll need to make your 
own arrangements to provide the Xine dependencies. 

## Operation

`xine-server` is intended to run quietly in the background, until it is
contacted by a client. Because each client interaction is self-contained
and should not take any significant time, the server will service
multiple concurrent clients (if necessary). Although this should not
be visible to clients, `xine-server` actually only services one client
request at a time -- there is no internal concurrency, to avoid the potential
for multi-threading problems that might otherwise develop.

When completely idle, `xine-server` uses no CPU. 
However, whether it can be left running from boot depends
on the general configuration of the system. Once `xine-server` has
opened an audio driver, it will not release it until it shuts down.
Depending on the audio driver in use, this may prevent other applications
playing audio. If you're just using `xine-server` to play audio
selected by another application, then that application could start
and stop `xine-server` as required. 

`xine-server` maintains a playlist and, once the playlist has been
populated by a client, it will play the items in the playlist one
after another. Clients can change the play order, pause playback,
stop playback, and manage the playlist. Clients can also 
retrieve information about the playlist and the item being played.
For more information on capabilities, see the file `README.protocol`. 

## Commnd line options

`-c,--config {path}`

Specify a location for the Xine engine configuration file. The default
is not to use one, as configuration defaults are workable on many
systems. You might need to specify a configuration file if you
have multiple audio devices, and need to force output to a specific one.

A sample configuration file is provided in the source code bundle.

`--debug`

Run in debug mode: do not detach from controlling terminal, and enable verbose
logging (see `--log-level`) to `stdout`.

`-d,--driver {name}`

Set the Xine audio driver. On a Linux system, suitable drivers are likely to
include `alsa`, `pulseaudio`, and `oss`. If no driver is specified, Xine will
auto-detect. This will usually result in the pulseaudio driver being used, if
the system supports it.

`-h,--host {IP}`

Set the IP number of the interface the server will bind to.
The default is 127.0.0.1, `localhost`. This default provides a
small modicum of security -- only processes on the same host
will be able to control the server.

You can bind to a specific IP number, or use `-h 0.0.0.0` to
bind to all IP interfaces. Both approaches need to be used
with caution, since `xine-server` has no security.

`-l,--log-level {N}

Set the logging level from 0-5. Levels higher than 3 will be extremely
verbose, and will only be comprehensible alongside the source code.

`--list-drivers`

Print a list of audio drivers, and then exit
 
`-p,--port {N}`

TCP port on which to listen for connections from clients.

## Logging

Other than when run in debug mode (``--debug``) logging is to
the system log daemon via the `syslog()` system call. Only
log messages of severity 0-2 are logged this way. In debug mode,
logging is to `stdout`, and `--log-level` can be used to set
verbose logging levels 3-4.

What happens to log messages in normal (non-debug) mode depends on
how system logging is configured.

## Audio driver issues

There is no general way to tell what audio drivers will be available
-- it depends on the platform and the way Xine was built. `alsa` is
a good choice on an embedded Linux system. On a desktop system 
with Pulse installed, Pulse will be used by default. You can get
a list of available drivers by running

    $ xine-server --list-drivers

Be aware that the `alsa` driver is likely to be exclusive, but this
also will depend on the system configuration. Even if it isn't
exclusive, using the `alsa` driver will probably have the effect that
setting the `xine-server` volume will set the overall system volume.
This isn't a problem with Pulse, which maintains per-application volume
settings.

## Protocol

See the file `README.protocol` in the source code bundle for
full details of the protocol.

## RC file

All the options that can be set on the command line can also be
set in the files `/etc/xine-server.rc` or `$HOME/.xine-server.rc`.
For example, to set the default IP port to 1234, add the line

    port=1234

These settings can be overridden on the command line. 

## Security

There isn't any. `xine-server` is not designed to be used in an
environment where security is a primary concern. By default, 
any process on the same host as `xine-server` can control the server and,
if the default host is changed from `localhost`, potentially anybody
can control it.

The `xine-server` process needs to run with sufficient user privileges
to allow the process to access the audio device. For Pulse, no
special privileges are needed. For ALSA, this depends on the
user/group ownership of the files in the `/dev/snd` directory.

## Limitations

_`xine-server` will try to 'play' image files_

The Xine engine supports the 'playback' of image files, even though they
contain no audio. If a client puts an image file, or a URL that loads
an image file, into the playlist, `xine-server` will get stuck. The
playback position will remain at 0 milliseconds, but the playback will
never end. I regard it as the client's job to filter out files that cannot be
played.

_Some messages are lost to stdout_

All internal logging in `xine-server` goes through a common logging
interface, allowing log verbosity to be controlled. The is particularly
relevant in debug mode. However, some of the libraries used by 
Xine, particularly `ffmpeg` log directly to `stdout`. This logging
cannot be controlled, or limited, by `xine-server`. This limitation
is generally only noticeable in debug mode, because in normal mode
`stdout` is suppressed anyway. 


## Author and legal

`xine-server` is copyright (c)2020 Kevin Boone, and distributed under the
terms of the GNU Public Licence, version 3.0. Essentially that means
you may use the software however you wish, so long as the source
code continues to be made available, and the original author is
acknowledged. There is no warranty of any kind.

