
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#define __USE_GNU /* for strcasestr */
#include <string.h>
#include <sys/types.h> /* for stat(2) */
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "../lib/pnd_pathiter.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_conf.h"
#include "pnd_discovery.h"
#include "pnd_desktop.h"

#include "mmapps.h"
#include "mmui.h"

mm_app_t *apps_fullscan ( char *searchpath ) {
  mm_app_t *apphead = NULL;
  mm_app_t *n = NULL;

  SEARCHPATH_PRE
  {
    pnd_log ( pndn_debug, "Scanning path '%s'\n", buffer );

    DIR *d = opendir ( buffer );
    struct dirent *de;
    char *c;
    char fullpath [ PATH_MAX ];

    if ( d ) {

      while ( ( de = readdir ( d ) ) ) {
	pnd_log ( pndn_debug, "  Found file: '%s'\n", de -> d_name );

	// candidate?
	if ( ( c = strrchr ( de -> d_name, '.' ) ) &&
	     ( strcasecmp ( c, ".desktop" ) == 0 ) )
	{
	  pnd_log ( pndn_debug, "    ..filename suggests a .desktop\n" );

	  sprintf ( fullpath, "%s/%s", buffer, de -> d_name );

	  n = apps_fetch_from_dotdesktop ( fullpath );

	  if ( n ) {
	    // got an app, prepend to the applist
	    pnd_log ( pndn_rem, "Found application '%s': '%s'\n", n -> dispname, n -> exec );
	    if ( ! apphead ) {
	      apphead = n;
	      apphead -> next = NULL;
	    } else {
	      n -> next = apphead;
	      apphead = n;
	    }
	  } else {
	    pnd_log ( pndn_debug, "  No application found.\n" );
	  } // if got an app back

	} else {
	  pnd_log ( pndn_debug, "    ..filename suggests ignore\n" );
	}

      } // while

      closedir ( d );
    } else {
      pnd_log ( pndn_warning, "WARN: Couldn't open directory '%s', skipping\n" );
    }


  }
  SEARCHPATH_POST

  return ( apphead );
}

mm_app_t *apps_fetch_from_dotdesktop ( char *path ) {

  FILE *f = fopen ( path, "r" );
  char buffer [ 1024 ];
  mm_app_t *p = (mm_app_t *) malloc ( sizeof(mm_app_t) );
  char *c;

  if ( ! f ) {
    if ( p ) {
      free ( p );
    }
    return ( NULL );
  }

  if ( ! p ) {
    fclose ( f );
    return ( NULL );
  }

  bzero ( p, sizeof(mm_app_t) );

  unsigned char apptype = 0;
  unsigned char pndcreated = 0;

  while ( fgets ( buffer, 1000, f ) ) {
    char *equals;

    // chop
    if ( ( c = strchr ( buffer, '\n' ) ) ) {
      *c = '\0'; // truncate trailing newline
    }

    //pnd_log ( pndn_debug, ".desktop line: '%s'\n", buffer );

    if ( strcmp ( buffer, PND_DOTDESKTOP_SOURCE ) == 0 ) {
      pndcreated = 1;
    }

    if ( ( equals = strchr ( buffer, '=' ) ) ) {
      *equals = '\0';
    }

    if ( strcasecmp ( buffer, "type" ) == 0 &&
	 strcasecmp ( equals + 1, "application" ) == 0 )
    {
      apptype = 1;
    }

    if ( strcasecmp ( buffer, "name" ) == 0 ) {
      p -> dispname = strdup ( equals + 1 );
    }

    if ( strcasecmp ( buffer, "exec" ) == 0 ) {
      p -> exec = strdup ( equals + 1 );
    }

    if ( strcasecmp ( buffer, "icon" ) == 0 ) {
      p -> iconpath = strdup ( equals + 1 );
    }

  } // while

  fclose ( f );

  if ( ! apptype ) {
    pnd_log ( pndn_debug, ".desktop is not an application; ignoring.\n" );
    free ( p );
    return ( NULL ); // not an application
  }

  if ( ! pndcreated ) {
    pnd_log ( pndn_debug, ".desktop is not from libpnd; ignoring.\n" );
    free ( p );
    return ( NULL ); // not created by libpnd
  }

  return ( p );
}
