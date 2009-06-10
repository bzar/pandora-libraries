
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for memset */
#include <unistd.h> /* for fork/exec */

#include <sys/types.h> /* for wait */
#include <sys/wait.h> /* for wait */

#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_apps.h"

unsigned char pnd_apps_exec ( char *pndrun, char *fullpath, char *unique_id,
			      char *rel_exec, char *rel_startdir,
			      unsigned int clockspeed, unsigned int options )
{
  char *argv [ 20 ];
  int f;

  //printf ( "Entering pnd_apps_exec\n" );

  if ( ! pndrun ) {
    return ( 0 );
  }

  if ( ! fullpath ) {
    return ( 0 );
  }

  if ( ! unique_id ) {
    return ( 0 );
  }

  if ( ! rel_exec ) {
    return ( 0 );
  }

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
  if ( rel_startdir ) {
    argv [ f++ ] = "-s";
    argv [ f++ ] = rel_startdir;
  }
  argv [ f++ ] = "-b";
  argv [ f++ ] = unique_id;

  // skip -a (arguments) for now

  if ( options & PND_EXEC_OPTION_NOUNION ) {
    argv [ f++ ] = "-n"; // no union for now
  }

  if ( options & PND_EXEC_OPTION_NOX11 ) {
    argv [ f++ ] = "-x"; // no union for now
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

  // by definition, either error occurred or we are the original application.

  // do we wish to wait until the child process completes? (we don't
  // care if it crashed, was killed, was suspended, whatever.)
  if ( options & PND_EXEC_OPTION_BLOCK ) {
    int status = 0;
    //waitpid ( f, &status. 0 /* no options */ );
    wait ( &status );
  }

  // printf ( "Exiting pnd_apps_exec\n" );

  return ( 1 );
}
