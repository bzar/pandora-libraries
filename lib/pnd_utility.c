
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#define __USE_GNU /* for strcasestr */
#include <string.h> /* for making ftw.h happy */
#include <unistd.h> /* for fork exec */

#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_utility.h"
#include "pnd_pndfiles.h"
#include "pnd_discovery.h"

// a generalized variable-substitution routine might be nice; for now we need a quick tilde one,
// so here goes. Brute force ftw!
char *pnd_expand_tilde ( char *freeable_buffer ) {
  char *p;
  char *s = freeable_buffer;
  char *home = getenv ( "HOME" );

  if ( ! home ) {
    return ( s ); // can't succeed
  }

  while ( ( p = strchr ( s, '~' ) ) ) {
    char *temp = malloc ( strlen ( s ) + strlen ( home ) + 1 );
    memset ( temp, '\0', strlen ( s ) + strlen ( home ) + 1 );
    // copy in stuff prior to ~
    strncpy ( temp, s, p - s );
    // copy tilde in
    strcat ( temp, home );
    // copy stuff after tilde in
    strcat ( temp, p + 1 );
    // swap ptrs
    free ( s );
    s = temp;
  } // while finding matches

  return ( s );
}

void pnd_exec_no_wait_1 ( char *fullpath, char *arg1 ) {
  int i;

  if ( ( i = fork() ) < 0 ) {
    printf ( "ERROR: Couldn't fork()\n" );
    return;
  }

  if ( i ) {
    return; // parent process, don't care
  }

  // child process, do something
  if ( arg1 ) {
    execl ( fullpath, fullpath, arg1, (char*) NULL );
  } else {
    execl ( fullpath, fullpath, (char*) NULL );
  }

  // getting here is an error
  //printf ( "Error attempting to run %s\n", fullpath );

  return;
}

pnd_pxml_handle pnd_pxml_get_by_path ( char *fullpath ) {
  unsigned char valid = pnd_object_type_unknown;
  pnd_pxml_handle pxmlh = 0;

  // WARN: this is way too close to callback in pnd_disco .. should be refactored!

  if ( strcasestr ( fullpath, PXML_FILENAME ) ) {
    valid = pnd_object_type_directory;
  } else if ( strcasestr ( fullpath, PND_PACKAGE_FILEEXT "\0" ) ) {
    valid = pnd_object_type_pnd;
  }

  // if not a file of interest, just keep looking until we run out
  if ( ! valid ) {
    return ( 0 );
  }

  // potentially a valid application
  if ( valid == pnd_object_type_directory ) {
    pxmlh = pnd_pxml_fetch ( (char*) fullpath );

  } else if ( valid == pnd_object_type_pnd ) {
    FILE *f;
    char pxmlbuf [ 32 * 1024 ]; // TBD: assuming 32k pxml accrual buffer is a little lame

    // open it up..
    f = fopen ( fullpath, "r" );

    // try to locate the PXML portion
    if ( ! pnd_pnd_seek_pxml ( f ) ) {
      fclose ( f );
      return ( 0 ); // pnd or not, but not to spec. Pwn'd the pnd?
    }

    // accrue it into a buffer
    if ( ! pnd_pnd_accrue_pxml ( f, pxmlbuf, 32 * 1024 ) ) {
      fclose ( f );
      return ( 0 );
    }

    // by now, we have <PXML> .. </PXML>, try to parse..
    pxmlh = pnd_pxml_fetch_buffer ( (char*) fullpath, pxmlbuf );

    // done with file
    fclose ( f );

  }

  // ..

  return ( pxmlh );
}
