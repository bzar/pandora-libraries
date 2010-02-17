
#include <stdio.h>          // for stdio, NULL
#include <stdlib.h>         // for malloc, etc
#include <unistd.h>         // for close
#include <time.h>           // for time()
#include <sys/types.h>      // for kill
#include <signal.h>         // for kill signal
#include <string.h>         // for bzero
#include <poll.h>           // for poll()

#include "pnd_dbusnotify.h"
#include "pnd_logger.h"

/* HACK HACK HACK
 * Not yet doing a real libdbus implementation.
 * First cut is using dbus-monitor; I'll work on a real dbus listener soon.
 * HACK HACK HACK
 */

typedef struct {
  pid_t child_pid;         // pid for dbus-monitor
  int fd [ 2 ];            // pipe fd pair
} pnd_dbusnotify_t;

#define MONITOR "/usr/bin/dbus-monitor"
#define MONARG1 "--system"
#define MONARG2 "interface=org.freedesktop.Hal.Device"
#define MONGREP "volume.is_mounted"

pnd_dbusnotify_handle pnd_dbusnotify_init ( void ) {
  pnd_dbusnotify_t *p;

  // get thee a handle
  // and I will brandish it for Zo Kath Ra

  p = malloc ( sizeof(pnd_dbusnotify_t) );

  if ( ! p ) {
    return ( NULL ); // uhh..
  }

  bzero ( p, sizeof(pnd_dbusnotify_t) );

  // can we make a pipe?
  if ( pipe ( p -> fd ) == -1 ) {
    free ( p );
    return ( NULL );
  }

  // spawn child process
  p -> child_pid = fork();

  if ( p -> child_pid == -1 ) {
    free ( p );
    return ( NULL ); // borked

  } else if ( p -> child_pid == 0 ) {
    // child

    close ( p -> fd [ 0 ] ); // ditch stdin on child
    dup2 ( p -> fd [ 1 ], 1 ); // assign write-end of pipe to stdout
    close ( p -> fd [ 1 ] ); // we don't need this anymore anyway
    execlp ( MONITOR, MONITOR, MONARG1, MONARG2, NULL );

    exit ( 1 ); // damnit

  } else {
    // parent

    close ( p -> fd [ 1 ] ); // ditch write end, we're a grepper

  } // child or parent?

  return ( p );
}

void pnd_dbusnotify_shutdown ( pnd_dbusnotify_handle h ) {
  pnd_dbusnotify_t *p = (pnd_dbusnotify_t*) h;

  // free up
  close ( p -> fd [ 0 ] ); // kill reader end of pipe

  // destroy child process
  kill ( p -> child_pid, SIGKILL );

  free ( p );

  return;
}

unsigned char pnd_dbusnotify_rediscover_p ( pnd_dbusnotify_handle h ) {
  pnd_dbusnotify_t *p = (pnd_dbusnotify_t*) h;
  int r;
  struct pollfd fds [ 1 ];

  fds [ 0 ].fd = p -> fd [ 0 ]; // read side of pipe
  fds [ 0 ].events = POLLIN | POLLPRI /* | POLLRDHUP */;
  fds [ 0 ].revents = 0;

  r = poll ( fds, 1, 0 /* timeout*/ );

  if ( r < 0 ) {
    // error
    // should rebuild a new child process
    return ( 0 );

  } else if ( r == 0 ) {
    // no input on the pipe
    return ( 0 );

  } // activity

  // something on the pipe, lets check it

  if ( fds [ 0 ].revents & ( POLLERR|POLLHUP/*|POLLRDHUP*/ ) ) {
    // bad
    // should rebuild a new child process
    return ( 0 );
  }

  // something useful on the pipe
  char buf [ 4096 ];
  int readcount;

  readcount = read ( fds [ 0 ].fd, buf, 4000 );

  if ( readcount < 0 ) {
    // error
    return ( 0 );
  }

  // terminate the string
  *( buf + readcount + 1 ) = '\0';

  if ( strstr ( buf, MONGREP ) != NULL ) {
    return ( 1 );
  }

  return ( 0 );
}
