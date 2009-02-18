
#include <sys/inotify.h>    // for INOTIFY duh
#include <stdio.h>          // for stdio, NULL
#include <stdlib.h>         // for malloc, etc
#include <unistd.h>         // for close

#include "pnd_notify.h"

typedef struct {
  int fd;              // notify API file descriptor
} pnd_notify_t;

static void pnd_notify_hookup ( int fd );

pnd_notify_handle pnd_notify_init ( void ) {
  int fd;
  pnd_notify_t *p;

  fd = inotify_init();

  if ( fd < 0 ) {
    return ( NULL );
  }

  p = malloc ( sizeof(pnd_notify_t) );

  if ( ! p ) {
    close ( fd );
    return ( NULL ); // uhh..
  }

  p -> fd = fd;

  // setup some default watches
  pnd_notify_hookup ( fd );

  return ( p );
}

void pnd_notify_shutdown ( pnd_notify_handle h ) {
  pnd_notify_t *p = (pnd_notify_t*) h;

  close ( p -> fd );

  return;
}

static void pnd_notify_hookup ( int fd ) {

  inotify_add_watch ( fd, "./testdata", IN_CREATE | IN_DELETE | IN_UNMOUNT );

  return;
}

unsigned char pnd_notify_rediscover_p ( pnd_notify_handle h ) {
  pnd_notify_t *p = (pnd_notify_t*) h;

  struct timeval t;
  fd_set rfds;
  int retcode;

  // don't block for long..
  t.tv_sec = 1;
  t.tv_usec = 0; //5000;

  // only for our useful fd
  FD_ZERO ( &rfds );
  FD_SET ( (p->fd), &rfds );

  // wait and test
  retcode = select ( (p->fd) + 1, &rfds, NULL, NULL, &t );

  if ( retcode < 0 ) {
    return ( 0 ); // hmm.. need a better error code handler
  } else if ( retcode == 0 ) {
    return ( 0 ); // timeout
  }

  if ( ! FD_ISSET ( (p->fd), &rfds ) ) {
    return ( 0 );
  }

  // by this point, something must have happened on our watch
#define BINBUFLEN ( 100 * ( sizeof(struct inotify_event) + 30 ) ) /* approx 100 events in a shot? */
  unsigned char binbuf [ BINBUFLEN ];
  int actuallen;

  actuallen = read ( (p->fd), binbuf, BINBUFLEN );

  if ( actuallen < 0 ) {
    return ( 0 ); // error
  } else if ( actuallen == 0 ) {
    return ( 0 ); // nothing, or overflow, or .. whatever.
  }

  unsigned int i;
  struct inotify_event *e;

  while ( i < actuallen ) {
    e = (struct inotify_event *) &binbuf [ i ];

    /* do it!
     */

    if ( e -> len ) {
      printf ( "Got event against '%s'\n", e -> name );
    }

    /* do it!
     */

    // next
    i += ( sizeof(struct inotify_event) + e -> len );
  } // while

  return ( 1 );
}
