# xsradio

Version 1.0a, February 2020

## What is this?

`xsradio` is a simple console-based Internet radio stream selector, 
for use with the `xine-server` audio server, particularly on an
embedded Linux system. It uses the `ncurses` library to provide
a simple user interface. 

## Prerequisites

`xine-server` must be running, on a host to which the `xsradio`
application has network access. If `xsradio` and `xine-server` are
on different hosts, `xsradio` must be configured to listen on 
a physical network interface, not `localhost`.

`xsradio` needs a file containing radio streams from which to
select. A sample is provided, but probably most of the stations
will be non-functioning within a week of its being generated --
more on this subject below.

## Usage

The operation of `xsradio` should be reasonably self-explanatory.
Select a station from the list and press 'enter'. Press 'h'
for a list of other key bindings.
 
## Internet radio in general

There is no reliable way to know for sure how many Internet radio
stations there are -- tens of thousands at least. 
My experience is that, even of those stations which I have
known to work in the past, at least half do not work at any particular
time. In the various lists of stations to be found on Web sites,
probably 80% do not work, at leaast from my region. 

`xsradio` is supplied with a sample list of stations, but I would
expect that most of them -- with the possible exception of the
state-run stations -- will not be working. Later in this README
I give an example of how to obtain a community-supported station
list that is at least up-to-date, but even that doesn't
guarantee success.

There are various way in which `xsradio` can fail to play a station.
First, the URI might not resolve at all -- this is the simplest case,
because the program will display an error message immediately.
Second, a connection might be established, and then be closed
immediately from the server end. This appears in `xsradio` as if,
essentially, nothing happened -- the user interface will continue
to display "stopped". Third, the station might start to deliver
audio, but continually be interrupted by buffering. This 
behaviour results in `xsradio` showing the status as 'buffering'
either frequently, or extended periods. In general, if a 
station doesn't start delivering audio after a few seconds, it 
probably isn't going to.

The point I wish to make is that, although `xsradio` might have
bugs, problems with it are at least as likely to be due to
the unreliable nature of Internet radio stations themselves.

## Stream file format

`xsradio` reads a list of streams from a simple text file, 
ideally in UTF8 encoding. Each line of the file denotes a 
stream, and has the following format:

    URI____name

That is, the stream URI is separated from the name by four underscores.
The `name` entry can be any text -- it is displayed
in the user interface, but has no other function.

By default, `xsradio` reads the stream file from

    /usr/share/xsradio/streams.list 

but a different file can be specified using the `--streams` command-line
switch.

## Sample streams 

A good, open-access list of Internet Radio streams can be found
at the Community Radio Station Index (CRSI) website

http://www.radio-browser.info/gui/#!/

which is maintained by Alex Segler and others. 

The `xsradio` source bundle contains a Perl script called 
`get_and_filter_stations.pl`. This script generates the file
`streams.list.sample` by downloading the full station list from
CRSI and filtering it. In the script, the filter selects
English-languge or English-location radio stations with "news" 
in their tags, and which are marked as tested. It should be relatively
easy to modify this script to match other criteria. The full list
from CRSI -- even if limited to stations that are known to be
working -- will be far too large to feed into `xsradio`. 


## Command-line options 

`-h,--host={hostname}`

Hostname or IP number of the `xine-server` host. The default is `localhost`.

`-p,--port={number}`

TCP port of the `xine-server` server. Default is 30001

`s,--streams={filename}

Filename of the streams list. Default is `/usr/share/xsradio/streams.list`.

## RC files 

Any of the options that can be set on the command line can also be set
in either of the RC files: `/etc/xsradio.rc` and `$HOME/.xsradio.rc`.
The command line options take precedence over the RC files.


## Notes

xxx



## Author and legal

`xsradio` is copyright (c)2020 Kevin Boone, and distributed under the
terms of the GNU Public Licence, version 3.0. Essentially that means
you may use the software however you wish, so long as the source
code continues to be made available, and the original author is
acknowledged. There is no warranty of any kind.




gpm -m /dev/input/mice -t imps2



