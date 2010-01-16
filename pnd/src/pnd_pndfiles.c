
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <ctype.h> /* for isprint */
#include <unistd.h> /* for fork/exec */
#include <sys/types.h> /* for wait */
#include <sys/wait.h> /* for wait */

#define __USE_GNU
#include <string.h> /* for making strcasestr happy */

#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_apps.h"
#include "pnd_pndfiles.h"

unsigned char pnd_pnd_seek_pxml ( FILE *f ) {
  char *b;                // buffer
  char *match;            // match within buffer
  unsigned int len;       // length of file (not suporting 'big files' 2GB..)
  unsigned int pos;       // current tape head
  unsigned int readable;  // amount to read (for small files/chunks)

  b = malloc ( PND_PXML_WINDOW_SIZE + 1 );

  if ( ! b ) {
    return ( 0 );
  }

  memset ( b, '\0', PND_PXML_WINDOW_SIZE + 1 );

  // determine length of file
  fseek ( f, 0, SEEK_END );
  len = ftell ( f );
  fseek ( f, 0, SEEK_SET );

  /* ready to scan through the file, backwards
   */

  // easy case or no?
  if ( len <= PND_PXML_WINDOW_SIZE ) {
    pos = 0;
    readable = len;
  } else {
    pos = len - PND_PXML_WINDOW_SIZE;
    readable = PND_PXML_WINDOW_SIZE;
  }

  while ( 1 ) {

    //printf ( "find pxml; pos %u readable %u\n", pos, readable );

    // seek into this blocks position
    fseek ( f, pos, SEEK_SET );

    // read
    fread ( b, 1, readable, f );

    // find the head tag here? now, there are serious heavy duty algorithyms for locating
    // strings within an arbitrary buffer. Thats needs to be done. This is the worst
    // performing brute force strategy here.
    if ( ( match = pnd_match_binbuf ( b, readable, PXML_TAGHEAD ) ) ) {
      fseek ( f, pos + ( match - b ), SEEK_SET );
      //printf ( "  match found at %u\n", pos + ( match - b ) );
      free ( b );
      return ( 1 );
    }

    // no match, so we need to back up another chunk; if we can't
    // back up that much, we're on our last check. if we can't back
    // up at all, we've already been here and time to fail
    if ( pos == 0 ) {
      break; // done, FAIL
    } else if ( pos > PND_PXML_WINDOW_FRACTIONAL ) {
      pos -= PND_PXML_WINDOW_FRACTIONAL;
      readable = PND_PXML_WINDOW_SIZE;
    } else {
      readable = PND_PXML_WINDOW_SIZE - pos;
      memset ( b + pos, '\0', PND_PXML_WINDOW_SIZE - pos );
      pos = 0;
    }

  } // while

  // exeunt, with alarums
  free ( b );
  return ( 0 );
}

unsigned char pnd_pnd_accrue_pxml ( FILE *f, char *target, unsigned int maxlen ) {
  char inbuf [ 1024 ]; // TBD: love all these assumptions? noob!
  char *insert = target;
  unsigned int l;

  while ( fgets ( inbuf, 1023, f ) ) {

    // copy inbuf onto end of target
#if 1
    strncpy ( insert, inbuf, maxlen );
    // reduce counter for max size so we can avoid buffer overruns
    l = strlen ( inbuf );
    maxlen -= l; // reduce
    insert += l; // increment
#else
    strncat ( target, inbuf, maxlen );
#endif

    if ( strcasestr ( inbuf, PXML_TAGFOOT ) ) {
      return ( 1 );
    }

  } // while

  return ( 0 );
}

char *pnd_match_binbuf ( char *haystack, unsigned int maxlen, char *needle ) {
  char *end = haystack + maxlen - strlen ( needle );
  unsigned int needlelen = strlen ( needle );

  while ( haystack < end ) {

    if ( isprint(*haystack) ) {

      if ( strncasecmp ( haystack, needle, needlelen ) == 0 ) {
	//printf ( "haystack %s\n", haystack );
	return ( haystack );
      }

    }

    haystack += 1;
  }

  return ( NULL );
}

static unsigned char pnd_pnd_mountie ( char *pndrun, char *fullpath, char *unique_id, unsigned char do_mount ) {
  char *argv [ 20 ];
  int f;

  if ( ! pndrun ) {
    return ( 0 );
  }

  if ( ! fullpath ) {
    return ( 0 );
  }

  if ( ! unique_id ) {
    return ( 0 );
  }

#if 0
  printf ( "  runscript: %s\n", pndrun );
  printf ( "  path: %s\n", fullpath );
  printf ( "  id: %s\n", unique_id );
#endif

  memset ( argv, '\0', sizeof(char*) * 20 );

  // normal stuff
  f = 0;
  argv [ f++ ] = pndrun;
  argv [ f++ ] = "-p";
  argv [ f++ ] = fullpath;
  argv [ f++ ] = "-b";
  argv [ f++ ] = unique_id;

  // mount-only
  if ( do_mount ) {
    argv [ f++ ] = "-m";
  } else {
    argv [ f++ ] = "-u";
  }

  // finish
  argv [ f++ ] = NULL; // for execv

  // debug
#if 0
  int i;
  for ( i = 0; i < f; i++ ) {
    printf ( "exec's argv %u [ %s ]\n", i, argv [ i ] );
  }
#endif

  // invoke it!

  if ( ( f = fork() ) < 0 ) {
    // error forking
  } else if ( f > 0 ) {
    // parent
  } else {
    // child, do it
    execv ( pndrun, argv );
  } 

  // wait until mountscript actually completes
  int status = 0;
  wait ( &status );

  return ( 1 );
}

unsigned char pnd_pnd_mount ( char *pndrun, char *fullpath, char *unique_id ) {
  return ( pnd_pnd_mountie ( pndrun, fullpath, unique_id, 1 ) );
}

unsigned char pnd_pnd_unmount ( char *pndrun, char *fullpath, char *unique_id ) {
  return ( pnd_pnd_mountie ( pndrun, fullpath, unique_id, 0 ) );
}
