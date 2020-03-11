# xsradio

Version 1.0c, February 2020

## What is this?

`xsradio` is a simple console-based Internet radio stream selector, for use
with the `xine-server` audio server, particularly on an embedded Linux system.
It uses the `ncurses` library to provide a simple user interface.  `xsradio` is
a test client for `xine-server`, but also a useable -- if simplistic --
way to play Internet radio stations. 

`xsradio` is designed to be able to run in a Linux kernel console, that is,
without any kind of graphical user interface installed. Of course it also runs
in a terminal emulator on a Linux desktop but, if X/Wayland is available, the
GUI equivalent `gxsradio` might be preferable. There are many Internet radio
applications for Linux, but both `xsradio` and `gsxradio` are specifically
designed for use on minimal and embedded Linux systems. Since version 1.0c,
`xsradio` and `gsxradio` use the same station list file format, which is
described below.

In a kernel console, there is at least limited mouse support (see below),
but `xsradio` is really designed for keyboard operation.

## Prerequisites

`xine-server` must be running, on a host to which the `xsradio` application has
network access. If `xsradio` and `xine-server` are on different hosts,
`xsradio` must be configured to listen on a physical network interface, not
`localhost` (which is the default).

`xsradio` needs a file containing radio streams from which to select. Some
sample files are provided, but probably most of the stations will be
non-functioning within a week of its being generated -- more on this subject
below.

## Usage

The operation of `xsradio` should be reasonably self-explanatory.
Select a station from the list and press 'enter'. Press 'h'
for a list of other key bindings. See below for other command-line
options.
 
## Internet radio in general

There is no reliable way to know for sure how many Internet radio stations
there are -- tens of thousands at least.  My experience is that, even of those
stations which I have known to work in the past, at least half do not work at
any particular time. In the various lists of stations to be found on Web sites,
probably 80% do not work, at least from my region. 

Commercial vendors of Internet radio devices maintain curated lists of stations
-- this is a large part of what you pay for when you buy a commercial device.
To be best of my knowledge, these station lists are proprietary, and there is
no legal way to distribute them. In the public domain, I think the next best
thing to a commercial station list is the community-maintained list at
radio-browser.info. This list is reasonably reliable, to the extent that the
station URLs are at least checked to ensure that a connection can be made.
Nevertheless, being community-supported, the data is of variable quality, and
you can't expect every station to work. 

`xsradio` is supplied with a few sample station files, and the Perl script that
extracts them from radio-browser.info -- see below for how to use this.

There are various way in which `xsradio` -- or, more accurately, xine-server
-- can fail to play a station.  First, the URI might not resolve at all -- this
is the simplest case, because the program will display an error message
immediately.  Second, the URI might resolve, but no connection be made. Again,
this is reported immediately.  Third, a connection might be established, and
then be closed immediately from the server end. This might happen if the server
is too busy, or doesn't like the player for some reason.  This appears in
`xsradio` as if, essentially, nothing happened -- the user interface will
continue to display "stopped". Fourth, the station might start to deliver
audio, but continually be interrupted by buffering. This behaviour results in
`xsradio` showing the status as 'buffering' either frequently, or for extended
periods. In general, if a station doesn't start delivering audio after a few
seconds, it probably isn't going to. However, I do know of a few stations that
have a 30-second buffering period, and then play perfectly well.

The point I wish to make here is that, although `xsradio` or `xine-server`
might have bugs, problems in the use of this software are at least as likely to
be due to the unreliable nature of Internet radio stations themselves.


## Station list file format

`xsradio` reads a list of streams from a simple text file, 
ideally in UTF8 encoding. Each line of the file denotes a 
stream, and has the following format:

    name {tab} country_code {tab} URI {tab} [unused number] 

The last `unused number` field is "0" or "1". This field is
not used by `xsradio`, but it _is_ used by `gxsradio` to save whether
a stream has been selected in the user interface. `xsradio` will
work if this final field is missing, but `gxsradio` will not,
so it's perhaps best to keep it in place.

By default, `xsradio` reads the stream file from

    /usr/share/xsradio/news_and_drama.gxsradio

but a different file can be specified using the `--streams` command-line
switch.

## Sample streams 

A good, open-access list of Internet Radio streams can be found
at the Community Radio Station Index (CRSI) website

    http://www.radio-browser.info/gui/#!/

which is maintained by Alex Segler and others. 

The `xsradio` source bundle contains a Perl script called
`gxsradio_get_stations.pl`.  This script queries the `radio-browser.info`
database and generates a number of station list files for use with `xsradio`
and `gxsradio`. The script is intended to be customized to suit a user's
preferences -- by default it extracts English or English-language stations that
have various tags. `make install` puts the Perl script, and sample output
files, in `/usr/local/xsradio`. It's probably best not to rely on the sample
files to be up-to-date, but to modify the script according to your own
listening preferences and run it again.

## Command-line options 

`-h,--host={hostname}`

Hostname or IP number of the `xine-server` host. The default is `localhost`.

`-p,--port={number}`

TCP port of the `xine-server` server. Default is 30001

`s,--streams={filename}

Filename of the streams list. 

## RC files 

Any of the options that can be set on the command line can also be set
in either of the RC files: `/etc/xsradio.rc` and `$HOME/.xsradio.rc`.
The command line options take precedence over the RC files.


## Mouse support

`xsradio` has very rudimentary mouse support. You can page through
the station list using the up and down markers (`v` and '^'), and
play a station by clicking on its name. Mouse support will
work -- after a fashion -- in a Linux kernel console (see below) 
if `gpm` is running. `gpm` needs to know about the specific
type of mouse attched. Most modern USB mice support the PS/2 protocol,
so you can run `gpm` like this:

    gpm -m /dev/input/mice -t imps2

When running `xsradio` in an X terminal emulator, `ncurses` should
emulate a mouse automatically.

## Linux kernel console support

`xsradio` is designed to be able to run in a kernel console,
that is, the console that is available on Linux in the absence
of any graphical desktop. There are two main limitations with
most of operation.

First, to get even rudimentary mouse support, you will
need to be running `gpm`, (as described above) 
and to have a version of the
`ncurses` library that is sufficiently up-to-date to be able
to auto-detect `gpm` (essentially, version 6.0 and later).

Second, the kernel console has by default only a limited range
of symbols in its font. Many consoles only support 
8-bit characters, anyway. So although `xradio` can be
made fully UTF-8 aware, if linked with the 'wide character`
version of `ncurses`, the console may not be able to 
display anything other than ASCII characters and box
graphics. 

These are limitations of the console, and cannot be overcome
in `xsradio`.  

## Author and legal

`xsradio` is copyright (c)2020 Kevin Boone, and distributed under the
terms of the GNU Public Licence, version 3.0. Essentially that means
you may use the software however you wish, so long as the source
code continues to be made available, and the original author is
acknowledged. There is no warranty of any kind.

## Revision history

1.0a Feb 2020<br/>
First working release

1.0b Feb 2020<br/>
Added preliminary mouse support

1.0c March 2020<br/>
Modified `xsradio` to read the same files as `gxsradio`.
Modified the station download script to apply multiple filters
and write multiple station files.


