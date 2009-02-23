
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for memset */
#include <unistd.h> /* for fork/exec */

#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_apps.h"

unsigned char pnd_apps_exec ( char *pndrun, char *fullpath, char *unique_id, char *rel_exec, char *rel_startdir, unsigned int clockspeed ) {
  char *argv [ 20 ];
  int f, i;

  printf ( "Entering pnd_apps_exec\n" );
#if 0
  printf ( "  runscript: %s\n", pndrun );
  printf ( "  path: %s\n", fullpath );
  printf ( "  id: %s\n", unique_id );
  printf ( "  exec: %s\n", rel_exec );
  printf ( "  cwd: %s\n", rel_startdir );
  printf ( "  clock: %u\n", clockspeed );
#endif

  memset ( argv, '\0', sizeof(char*) * 20 );

  f = 0;
  argv [ f++ ] = pndrun;
  argv [ f++ ] = "-p";
  argv [ f++ ] = fullpath;
  argv [ f++ ] = "-e";
  argv [ f++ ] = rel_exec;
  // skip -a (arguments) for now

  //argv [ f++ ] = "-b";
  //argv [ f++ ] = baename;

  argv [ f++ ] = "-u"; // no union for now
  argv [ f++ ] = NULL; // for execv

  // debug
#if 0
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

  printf ( "Exiting pnd_apps_exec\n" );

  return ( 1 );
}
