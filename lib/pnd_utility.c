
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#define __USE_GNU /* for strcasestr */
#include <string.h> /* for making ftw.h happy */
#include <unistd.h> /* for fork exec */

#include <utmp.h> /* for expand-tilde below; see commentary for this about-turn */
#include <sys/types.h> /* ditto */
#include <pwd.h> /* ditto */
#include <sys/stat.h> /* for fstat */
#include <dirent.h> /* for opendir */

#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_utility.h"
#include "pnd_pndfiles.h"
#include "pnd_discovery.h"

unsigned char pnd_check_login ( char *r_username, unsigned int maxlen ) {
  FILE *f;
  struct utmp b;
  struct passwd *pw;

  f = fopen ( "/var/run/utmp", "r" );

  if ( f ) {

    // spin until a non-root user comes along
    while ( fread ( &b, sizeof(struct utmp), 1, f ) == 1 ) {

      if ( ( b.ut_type == USER_PROCESS ) &&
	   ( strcmp ( b.ut_user, "root" ) != 0 ) )
      {

	// ut_user contains the username ..
	// now we need to find the path to that account.
	while ( ( pw = getpwent() ) ) {

	  if ( strcmp ( pw -> pw_name, b.ut_user ) == 0 ) {

	    if ( r_username ) {
	      strncpy ( r_username, b.ut_user, maxlen );
	    }

	    fclose ( f );
	    return ( 1 );
	  } // passwd entry matches the utmp entry

	} // while iteratin across passwd entries
	endpwent();

      } // utmp entry is for a user login

    } // while

    fclose ( f );
  } // opened?

  return ( 0 );
}

// a generalized variable-substitution routine might be nice; for now we need a quick tilde one,
// so here goes. Brute force ftw!
char *pnd_expand_tilde ( char *freeable_buffer ) {
  char *p;
  char *s = freeable_buffer;
  char *home = getenv ( "HOME" );

  //printf ( "DEBUG: expand tilde IN: '%s'\n", freeable_buffer );
  //printf ( "DEBUG:  $HOME was %s\n", home );

  // well, as pndnotifyd (etc) may be running as _root_, while the user is logged in
  // as 'pandora' or god knows what, this could be problematic. Other parts of the lib
  // use wordexp() for shell-like expansion, but this funciton is (at least) used by
  // pndnotifyd for determination of the .desktop emit path, so we need ~ to expand
  // to the _actual user_ rather than root's homedir. Rather than run 'users' or 'who'
  // or the like, we'll just cut to the chase..
  ///////
  {
    FILE *f;
    struct utmp b;
    static char *florp = NULL;
    struct passwd *pw;

    f = fopen ( "/var/run/utmp", "r" );

    if ( f ) {

      while ( fread ( &b, sizeof(struct utmp), 1, f ) == 1 ) {

	if ( b.ut_type == USER_PROCESS ) {

	  // ut_user contains the username ..
	  // now we need to find the path to that account.
	  while ( ( pw = getpwent() ) ) {

	    if ( strcmp ( pw -> pw_name, b.ut_user ) == 0 ) {

	      // aight, we've got a logged in user and have matched it to
	      // passwd entry, so can construct the appropriate path

	      if ( florp ) {
		free ( florp );
	      }
	      florp = strdup ( pw -> pw_dir );

	      home = florp;
	      //printf ( "  DEBUG: home (for %s) is %s (from %u)\n", b.ut_user, home, b.ut_type );

	    } // passwd entry matches the utmp entry

	  } // while iteratin across passwd entries
	  endpwent();

	} // utmp entry is for a user login

      } // while
      fclose ( f );
    } // opened?

  }
  ///////

  if ( ! home ) {
    return ( s ); // can't succeed
  }

  //printf ( "DEBUG: entering while (%s) with home (%s)\n", s, home );

  while ( ( p = strchr ( s, '~' ) ) ) {
    //printf ( "DEBUG: within while (%s)\n", s );
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

  //printf ( "DEBUG: expand tilde OUT: '%s'\n", s );

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

unsigned char pnd_determine_mountpoint ( char *fullpath, char *r_mountpoint, unsigned char mountpoint_len ) {

  // just cheap it, and call df like an idiot.

  // Filesystem           1K-blocks      Used Available Use% Mounted on

  char cmd [ PATH_MAX ];
  FILE *p;
  char inbuf [ PATH_MAX ];

  sprintf ( cmd, "/bin/df %s 2>/dev/null", fullpath );

  if ( ( p = popen ( cmd, "r" ) ) ) {

    // ignore title line; we really shoudl analyze it to figure out which column, but we make assumptions..
    fgets ( inbuf, PATH_MAX, p );

    if ( ! fgets ( inbuf, PATH_MAX, p ) ) {
      pclose ( p );
      return ( 0 );
    }

    pclose ( p );

    // by now, good
    char crap [ PATH_MAX ];
    char mount [ PATH_MAX ];
    if ( sscanf ( inbuf, "%s %s %s %s %s %s", crap, crap, crap, crap, crap, mount ) != 6 ) {
      return ( 0 );
    }

    if ( strlen ( mount ) < mountpoint_len ) {
      strcpy ( r_mountpoint, mount );
      return ( 1 );
    }

  } // if popen

  return ( 0 );

#if 0
  struct stat fooby;

  // can we even stat this file?
  if ( stat ( fullpath, &fooby ) == 0 ) {
    //dev_t     st_dev;     /* ID of device containing file */
    //dev_t     st_rdev;    /* device ID (if special file) */

    dev_t mount = fooby.st_dev;

    DIR *d = opendir ( "/dev" );

    if ( d ) {
      struct dirent *de;
      char path [ FILENAME_MAX ];

      while ( de = readdir ( d ) ) {
	sprintf ( path, "/dev/%s", de -> d_name );

	if ( stat ( path, &fooby ) == 0 ) {

	  // finally, if we find the same major/minor pair in /dev, as we found for the target file, it means we found the right device
	  if ( fooby.st_rdev == mount ) {
	    printf ( "Device: %s\n", path );
	  }

	} // if

      } // while

    } // opened /dev?

  } // stat
#endif

  return ( 0 );
}
