/*============================================================================

  xine-server
  xine-server-api.h
  Copyright (c)2020 Kevin Boone, GPL v3.0

  Definitions of the functions in the xineserver API.

============================================================================*/

#pragma once

// Boolean -- define these if nobody else has

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE 
#define FALSE 0
#endif

#ifndef BOOL
typedef int BOOL;
#endif

// Default server port
#define XINESERVER_DEF_PORT 30001

// Error codes

// No error
#define XINESERVER_ERR_OK             0
// Syntax error in client command 
#define XINESERVER_ERR_SYNTAX         1 
// General playback problem
#define XINESERVER_ERR_PLAYBACK       2
// Tried to play a non-existent file 
#define XINESERVER_ERR_NOFILE         3
// Client sent an unrecognized command 
#define XINESERVER_ERR_BADCOMMAND     4
// Client sent a defective command argument
#define XINESERVER_ERR_BADARG         5
// Playlist index is out of range 
#define XINESERVER_ERR_PLAYLIST_INDEX 6
// Playlist is empty 
#define XINESERVER_ERR_PLAYLIST_EMPTY 7
// Attempt to move past end of playlist 
#define XINESERVER_ERR_PLAYLIST_END   8
// Attempt to move before start of playlist 
#define XINESERVER_ERR_PLAYLIST_START 9
// Communication error with server 
#define XINESERVER_ERR_COMM           10 
// Unexpected response from server 
#define XINESERVER_ERR_RESPONSE       11 

// Limits

#define XINESERVER_MAX_VOLUME         100
#define XINESERVER_MIN_VOLUME         0 
#define XINESERVER_MAX_EQ             100
#define XINESERVER_MIN_EQ             -100


// Names of commands

#define XINESERVER_CMD_ADD       "add"
#define XINESERVER_CMD_PLAYLIST  "playlist"
#define XINESERVER_CMD_STOP      "stop"
#define XINESERVER_CMD_PLAY      "play"
#define XINESERVER_CMD_STATUS    "status"
#define XINESERVER_CMD_SHUTDOWN  "shutdown"
#define XINESERVER_CMD_PAUSE     "pause"
#define XINESERVER_CMD_CLEAR     "clear"
#define XINESERVER_CMD_NEXT      "next"
#define XINESERVER_CMD_PREV      "prev"
#define XINESERVER_CMD_VOLUME    "volume"
#define XINESERVER_CMD_META_INFO "meta-info"
#define XINESERVER_CMD_SEEK      "seek"
#define XINESERVER_CMD_EQ        "eq"

// XSPlaylist is an opaque structure, used with the
//  xsplaylist_xxx funtions

typedef struct _XSPlaylist XSPlaylist;

// XSMetaInfo is an opaque structure, used with the
//  xsmetainfo_xxx funtions

typedef struct _XSMetaInfo XSMetaInfo;

// XSStatus is an opaque structure, used with the
//  xsstatus_xxx funtions

typedef struct _XSStatus XSStatus;

typedef enum _XSTransportStatus
  {
  XINESERVER_TRANSPORT_STOPPED = 0,
  XINESERVER_TRANSPORT_PLAYING = 1,
  XINESERVER_TRANSPORT_PAUSED = 2,
  XINESERVER_TRANSPORT_BUFFERING = 3
  } XSTransportStatus;

typedef enum _XSNotifyClass
  {
  XSNOTIFY_CLASS_SERVER = 1, // Server events
  XSNOTIFY_CLASS_TRANSPORT = 2, // Transport events (play, stop...)
  XSNOTIFY_CLASS_PLAYLIST = 3, // Playlist changes 
  XSNOTIFY_CLASS_AUDIO = 3, // Audio changes -- volume, eq... 
  } XSNotifyClass;

typedef enum XSNotifyEvent 
  {
  XSNOTIFY_EVENT_STARTUP = 0, // Server started up 
  XSNOTIFY_EVENT_SHUTDOWN = 1, // Server shut down
  XSNOTIFY_EVENT_PLAYBACK_STOPPED = 2, // Playback stopped 
  XSNOTIFY_EVENT_PLAYBACK_PAUSED = 3, // Playback paused 
  XSNOTIFY_EVENT_CHANGED_PL_POSITION = 4, // Changed position in playlist 
  XSNOTIFY_EVENT_PL_CHANGED = 5, // Playlist contents changed
  XSNOTIFY_EVENT_VOLUME_CHANGED = 6, // Audio volume changed
  XSNOTIFY_EVENT_SEEK = 7, // Playback position changed in stream
  XSNOTIFY_EVENT_NEW_STREAM = 8, // Started to play a new stream
  XSNOTIFY_EVENT_STREAM_FINISHED = 9, // Finished playing stream 
  XSNOTIFY_EVENT_PL_FINISHED = 10, // Finished whole playlist 
  XSNOTIFY_EVENT_PROGRESS = 11, // General progress update 

  } XSNotifyEvent;


#ifdef __cplusplus
exetern "C" { 
#endif

// Basic API functions

// Add the specific streams (or local files) to the playlist. They won't
//  be played, and the existing playlist contents are retained.
// Clients are advised to use this method to add multiple items, rather
//  than add_single, because only one notification will be sent for
//  the playlist change
BOOL   xineserver_add      (const char *host, int port, int nstreams, 
                            const char *const *streams, 
                            int *error_code, char **error);

// Add the specific single stream (or local file) to the playlist. It won't
//  be played, and the existing playlist contents are retained.
BOOL   xineserver_add_single (const char *host, int port, 
                            const char *stream, 
                            int *error_code, char **error);

// Get the contents of the playlist, as a XSPlist opaque structure.
// See the methods below for access the playlist. If the method
//   returns TRUE a structure has been stored, and the caller should
//   use xsplaylist_destroy() to free it after use.
BOOL   xineserver_playlist (const char *host, int port, XSPlaylist **playlist, 
                            int *error_code, char **error);

// Stop playback, and set the playlist index to -1 (ie., nowhere).
BOOL   xineserver_stop     (const char *host, int port, 
                            int *error_code, char **error);

// Request the server to shut down cleanly. 
BOOL   xineserver_shutdown (const char *host, int port, 
                            int *error_code, char **error);

// Pause playback. It is not considered an error to call pause() 
//   when nothing is playing although, of course, this has no 
//   effect. There might be errors contacting the server, though,
//   so this method should be assumed to be capable of failing
BOOL   xineserver_pause    (const char *host, int port, 
                            int *error_code, char **error);

// Resume playback after a pause. It is not considered an error
//   to resume when something is already playing.
BOOL   xineserver_resume   (const char *host, int port, 
                            int *error_code, char **error);

// Play the specified item in the playlist. This function can fail
//   in many ways -- index out of range, playlist empty, item
//   not playable, etc. 
BOOL   xineserver_play     (const char *host, int port, int index, 
                            int *error_code, char **error);

// Clear the playlist. This function also stops playback, because
//   xineserver regards everything it plays as being in a playlist.
BOOL   xineserver_clear    (const char *host, int port, 
                            int *error_code, char **error);

// Play the next item in the playlist. An error is returned if
//   the item being played is already the last in the playlist 
BOOL   xineserver_next     (const char *host, int port, 
                            int *error_code, char **error);

// Play the previous item in the playlist. An error is returned if
//   the item being played is already the first in the playlist 
BOOL   xineserver_prev     (const char *host, int port,
                            int *error_code, char **error);

// Set the volume (0..100). Note that xineserver sets the _system_ volume, not
//  a local volume level for itsel, when using the 'alsa' driver. 
//  Setting the volume here could potentially
//  affect all audio playback 
BOOL   xineserver_set_volume   (const char *host, int port, int volume,
                            int *error_code, char **error);
BOOL   xineserver_get_volume   (const char *host, int port, int *volume,
                            int *error_code, char **error);

// Get and set the equalizer level, in ten bands, with values between
//  -100 and 100. All zero is a flat response
BOOL   xineserver_set_eq (const char *host, int port, int eq[10],
                            int *error_code, char **error);
BOOL   xineserver_get_eq (const char *host, int port, int eq[10],
                            int *error_code, char **error);

// A convenience function for converting the time valumes in msec
//   that Xine uses, to more manageable hour, minute, second values
void   xineserver_msec_to_hms (int msec, int *h, int *m, int *s);

// Get the current playback status, in an XSStatus structure. If the
//   function succeeds (returns TRUE), then the caller must free
//   the structure by calling xsstatus_destroy() after use. See
//   the xsstatus_xxx methods for details of the information returned.
BOOL   xineserver_status   (const char *host, int port, XSStatus **status, 
                            int *error_code, char **error);

// Get meta-info for the currently-playing item, in an XSMetaInfo structure.
// If the function succeeds (returns TRUE), then the caller must free the
//   structure by calling xsmetainfo_destroy() after use. See the 
//   xsmetainfo_xxx methods for details of the information returned.
BOOL   xineserver_meta_info (const char *host, int port, XSMetaInfo **mi, 
                            int *error_code, char **error);
// Seek to a specific position in the current stream, in milliseconds.
// If msec is negative, treat it as zero. If msec is longer than the
//   length of the stream, treat playback of that stream as finished,
//   and advance to the next playlist item, if there is one
BOOL   xineserver_seek (const char *host, int port, int msec, 
                            int *error_code, char **error);

// Operations on opaque data structures 

// Destroy the XSPlaylist structure allocated by xineserver__playlist()
void         xsplaylist_destroy  (XSPlaylist *self);
// Get the number of entries in the playlist. This could be zero.
int          xsplaylist_get_nentries (const XSPlaylist *self);
// Get the playlist items as an array of const char * values
char **const xsplaylist_get_entries (const XSPlaylist *self);

// Destroy the XSStatus structure alloation by xineserver_status()
void         xsstatus_destroy (XSStatus *self);
// Get the playback position, in msec. This could be any non-negative value
int          xsstatus_get_position (const XSStatus *self);
// Get the stream length, in msec. This could be any non-negative value,
//   because the length might not even be known (e.g., a shoutcast stream)
int          xsstatus_get_length (const XSStatus *self);
// Get the playlist length. This could be zero
int          xsstatus_get_playlist_length (const XSStatus *self);
// Get the current playlist item. The index is zero-based. The index
//   could be -1 if the playlist is empty, or xineserver_stop() has
//   been called
int          xsstatus_get_playlist_index (const XSStatus *self);
// Convenience function to get the playback position in a displayable
//  HH:MM:SS value. The buffer should have room for at least 9 bytes
void         xsstatus_get_position_hms (const XSStatus *self, char *buff,
               int bufflen);
// Convenience function to get the stream length in a displayable
//  HH:MM:SS value. The buffer should have room for at least 9 bytes
void         xsstatus_get_length_hms (const XSStatus *self, char *buff,
               int bufflen);
// Get the file or URL currently playing. If nothing is playing, the
//  stream is "-" (that is, a dash).
const char  *xsstatus_get_stream (const XSStatus *self);
XSTransportStatus xsstatus_get_transport_status (const XSStatus *self);

// Destroy the XSMetaInfo structure alloation by xineserver_meta_info()
void        xsmetainfo_destroy (XSMetaInfo *self);
// Bitrate is in bits per second
int         xsmetainfo_get_bitrate (const XSMetaInfo *self);
const char *xsmetainfo_get_composer (const XSMetaInfo *self);
const char *xsmetainfo_get_album (const XSMetaInfo *self);
const char *xsmetainfo_get_genre (const XSMetaInfo *self);
const char *xsmetainfo_get_artist (const XSMetaInfo *self);
const char *xsmetainfo_get_title (const XSMetaInfo *self);
BOOL        xsmetainfo_is_seekable (const XSMetaInfo *self);



#ifdef __cplusplus
} 
#endif


