# xine-server API

## What is this?

This is a simple client library for controlling `xine-server`. It consists
of a single C source file and a single header, that can be dropped into
an application without any special built considerations. It is intended
to be built using GCC, and uses GNU-specific C extensions.

This README should be read alongside that of `xineserver` itself -- there
is a more-or-less direct mapping between API calls and client-server
protocol operations.
 

## Overview

The API is very simple -- each interaction with the server is complete
and self-contained: there is no specific 'make connection' step or anything
similar. Each operation either succeeds or fails. If it fails, it returns
an error code and a text (English) message.

Some of the API calls return opaque data structures. By 'opaque' I mean
that the data elements are not intended to be manipulated by users, and
there is a specific API call to dispose of the structure and reclaim
any memory used.

The API functions are defined in `xine-server-api.h` and documented,
where it isn't self-explanatory, therein. All the basic API functions
take at least the host and port of the server process, and a place
to store the error code and the error message. All these functions
return a `BOOL` value, where `TRUE` indicates success. If the 
return is `FALSE` then the `error` argument will have been allocated
with an error message. The caller is responsible for freeing the message 
(just an ordinary `free()`) after use. If the return is `TRUE` then
the function will not have allocated any memory that needs to be
freed, expect where the information in the header file indicates otherwise. 

## Notes

`xineserver` maintains a playlist although, when playing radio streams,
the playlist commonly contains only one item. The playlist is indexed
from zero, but the index may be -1 if nothing is playing. 

The API does not check the validity of any file or stream passed
to the server -- although the server might, either when it is added
to the playlist or when it is played. If the stream turns out to be
invalid when it is played, the client application will never know,
because it will no longer be listening. 

The API does not provide any capabilities the `xineserver` itself does
not. So, for example, it can't handle any types of file or stream
that the server cannot.

Note that the `xineserver_volume()` just sends a volume change to the
server, which can interpret it as it sees fit. In practice, Xine sets
the system master volume, at least when using the ALSA driver, which 
can be a little disconcerting.

Missing information (such as a stream that does not report a genre or
artist) is returned as "-" (a single dash) in all methods that return
that kind of data. Methods that return information about a file or
stream usually return "-" if nothing is playing. Bitrates are reported
as 0 bits per second if nothing is playing, or nothing is reported. 

