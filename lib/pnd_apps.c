
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for memset */
#include <unistd.h> /* for fork/exec */
#include <limits.h>

#include <sys/types.h> /* for wait */
#include <sys/wait.h> /* for wait */

#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_apps.h"
#include "pnd_logger.h"

static char apps_exec_runline [ 1024 ];

char *pnd_apps_exec_runline ( void ) {
  return ( apps_exec_runline );
}

unsigned char pnd_apps_exec_disco ( char *pndrun, pnd_disco_t *app,
				    unsigned int options, void *reserved )
{
  char *argv [ 60 ];
  char fullpath [ PATH_MAX ] = "";
  int f;

  //printf ( "Entering pnd_apps_exec\n" );

  if ( ! pndrun ) {
    return ( 0 );
  }

  if ( ! app -> unique_id ) {
    return ( 0 );
  }

  if ( ! app -> exec ) {
    return ( 0 );
  }

  if ( options & PND_EXEC_OPTION_INFO && ! reserved ) {
    return ( 0 );
  }

  // determine path to pnd-file
  sprintf ( fullpath, "%s/%s", app -> object_path, app -> object_filename );

  // nail down argv for the app
  //
  memset ( argv, '\0', sizeof(char*) * 20 );

  f = 0;
  argv [ f++ ] = pndrun;
  argv [ f++ ] = "-p";
  argv [ f++ ] = fullpath;
  argv [ f++ ] = "-e";
  if ( options & PND_EXEC_OPTION_INFO ) {
    argv [ f++ ] = ((pnd_apps_exec_info_t*)reserved) -> viewer;
  } else {
    argv [ f++ ] = app -> exec;
  }
  if ( app -> startdir ) {
    argv [ f++ ] = "-s";
    argv [ f++ ] = app -> startdir;
  }
  if ( options & PND_EXEC_OPTION_INFO ) {
    if ( ((pnd_apps_exec_info_t*)reserved) -> args ) {
      argv [ f++ ] = "-a";
      argv [ f++ ] = ((pnd_apps_exec_info_t*)reserved) -> args;
    }
  } else {
    if ( app -> execargs ) {
      argv [ f++ ] = "-a";
      argv [ f++ ] = app -> execargs;
    }
  }
  if ( app -> appdata_dirname ) {
    argv [ f++ ] = "-b";
    argv [ f++ ] = app -> appdata_dirname;
  } else {
    argv [ f++ ] = "-b";
    argv [ f++ ] = app -> unique_id;
  }

  if ( options & PND_EXEC_OPTION_INFO ) {
    // we don't need to overclock for showing info :) do we? crazy active .js pages maybe?
  } else {
    if ( app -> clockspeed ) {
      argv [ f++ ] = "-c";
      argv [ f++ ] = app -> clockspeed;
    }
  }

  // skip -a (arguments) for now

  if ( options & PND_EXEC_OPTION_NOUNION ) {
    argv [ f++ ] = "-n"; // no union for now
  }

  if ( options & PND_EXEC_OPTION_NOX11 ) {
    argv [ f++ ] = "-x"; // shut down X!
  }

  // finish
  argv [ f++ ] = NULL; // for execv

  // stop here?
  if ( options & PND_EXEC_OPTION_NORUN ) {
    unsigned char i;
    bzero ( apps_exec_runline, 1024 );
    //pnd_log ( PND_LOG_DEFAULT, "Norun %u\n", f );
    unsigned char quotenext = 0;
    for ( i = 0; i < ( f - 1 ); i++ ) {
      //pnd_log ( PND_LOG_DEFAULT, "Norun %u: %s\n", i, argv [ i ] );

      // add spacing between args
      if ( i > 0 ) {
	strncat ( apps_exec_runline, " ", 1000 );
      }

      // quoting
      if ( quotenext ) {
	strncat ( apps_exec_runline, "\"", 1000 );
      }

      // arg
      strncat ( apps_exec_runline, argv [ i ], 1000 );

      // unquoting
      if ( quotenext ) {
	strncat ( apps_exec_runline, "\"", 1000 );
      }

      // clear quoting
      if ( quotenext ) {
	quotenext = 0;
      } else {
	// deprecated; need to handle spaces in some additional args
	//   if ( strcmp ( argv [ i ], "-a" ) == 0 ) {
	// if this is for -a, we need to wrap with quotes
	// ivanovic:
	// to allow spaces in filenames we have to add quotes around most terms!
	// terms with quotes:
	// -a additional arguments
	// -p fullpath to pnd
	// -e name of execuatable inside the pnd
	// -s startdir
	// -b name for the appdir

	if ( ( strcmp ( argv [ i ], "-a" ) == 0 ) ||
	     ( strcmp ( argv [ i ], "-p" ) == 0 ) ||
	     ( strcmp ( argv [ i ], "-e" ) == 0 ) ||
	     ( strcmp ( argv [ i ], "-s" ) == 0 ) ||
	     ( strcmp ( argv [ i ], "-b" ) == 0 ) )
	{
	  quotenext = 1;
	}

      }

    } // for
    return ( 1 );
  }

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
    waitpid ( f, &status, 0 /* no options */ );
    //wait ( &status );
  }

  // printf ( "Exiting pnd_apps_exec\n" );

  return ( 1 );
}

unsigned char pnd_apps_exec ( char *pndrun, char *fullpath, char *unique_id,
			      char *rel_exec, char *rel_startdir,
			      char *args,
			      unsigned int clockspeed, unsigned int options )
{
  pnd_disco_t d;
  bzero ( &d, sizeof(pnd_disco_t) );

  char cpuspeed [ 10 ];
  sprintf ( cpuspeed, "%u", clockspeed );

  char hackpath [ PATH_MAX ];
  strncpy ( hackpath, fullpath, PATH_MAX );
  char *c = strrchr ( hackpath, '/' );
  if ( c ) {
    *c = '\0';
    d.object_path = hackpath;
    d.object_filename = c + 1;
  } else {
    d.object_path = fullpath;
  }

  d.unique_id = unique_id;
  d.exec = rel_exec;
  d.startdir = rel_startdir;
  d.execargs = args;
  if ( clockspeed ) {
    d.clockspeed = cpuspeed;
  } else {
    d.clockspeed = NULL;
  }

  return ( pnd_apps_exec_disco ( pndrun, &d, options, NULL ) );
}

void pnd_get_ro_mountpoint ( char *fullpath, char *unique_id, char *r_mountpoint, unsigned int mountpoint_len ) {

  if ( ! r_mountpoint ) {
    return; // sillyness
  }

  snprintf ( r_mountpoint, mountpoint_len, "%s/%s/", PND_MOUNT_PATH, unique_id );

  return;
}

unsigned char pnd_get_appdata_path ( char *fullpath, char *unique_id, char *r_path, unsigned int path_len ) {
  // you know, determining the 'mount point' used is sort of a pain in the rear
  // use df <file>, skip header line, grab first token. Cheap and should work.
  // (Rather than just assume first two path chunks in path, say, which would be pretty darned
  // accurate, but whose to say they're not using NFS or crazy mountpoints?)
  FILE *df;
  char cmdbuf [ 1024 ];

  snprintf ( cmdbuf, 1023, "/bin/df %s", fullpath );

  df = popen ( cmdbuf, "r" );

  if ( ! df ) {
    return ( 0 ); // tunefs: you can tune a filesystem but you can't tune a fish
  }

  // fetch and discard header
  if ( ! fgets ( cmdbuf, 1023, df ) ) {
    pclose ( df );
    return ( 0 );
  }

  // grab df result line
  if ( ! fgets ( cmdbuf, 1023, df ) ) {
    pclose ( df );
    return ( 0 );
  }

  pclose ( df );

  // parse
  char *ws = strchr ( cmdbuf, ' ' );

  if ( ! ws ) {
    return ( 0 );
  }

  *ws = '\0';

  if ( r_path && path_len ) {
    snprintf ( r_path, path_len, "%s/pandora/appdata/%s/", cmdbuf, unique_id );
  }

  return ( 1 );
}
