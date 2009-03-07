
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for making ftw.h happy */
#include <unistd.h> /* for fork exec */

#include "pnd_utility.h"

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
