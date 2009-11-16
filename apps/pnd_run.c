
#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */
#include <string.h> /* for strdup */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"
#include "pnd_pndfiles.h"
#include "pnd_pxml.h"

static void usage ( char *argv[] ) {
  printf ( "%s [-r runscript] [-n] path-to-pndfile\n", argv [ 0 ] );
  printf ( "-r\tOptional. If not specified, will attempt to suss from configs.\n" );
  printf ( "-n\tOptional. If present, instruct runscript to kill/restart X11 around app.\n" );
  printf ( "pndfile\tRequired. Full path to the pnd-file to execute.\n" );
  return;
}

int main ( int argc, char *argv[] ) {
  char *pnd_run = NULL;
  char *pndfile = NULL;
  unsigned char no_x11 = 0;
  unsigned char i;

  for ( i = 1; i < argc; i++ ) {

    if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'r' ) {
      pnd_run = argv [ i + 1 ];
      i++;
      if ( ! pnd_run ) {
	printf ( "-r specified, but no argument provided.\n" );
	exit ( 0 );
      }
    } else if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'n' ) {
      no_x11 = 1;
    } else {

      if ( argv [ i ][ 0 ] == '-' ) {
	usage ( argv );
	exit ( 0 );
      } else if ( pndfile ) {
	printf ( "Only one pndfile may be specified.\n" );
      } else {
	pndfile = argv [ i ];
      }

    }

  } // for args

  // if runscript was not specified on cmdline, attempt to pick it up from config
  // ---> cribbed right out of discotest :/ copypaste ftw!
  if ( ! pnd_run ) {
    char *configpath;
    char *appspath;
    char *overridespath;

    // attempt to fetch a sensible default searchpath for configs
    configpath = pnd_conf_query_searchpath();

    // attempt to fetch the apps config. since it finds us the runscript
    pnd_conf_handle apph;

    apph = pnd_conf_fetch_by_id ( pnd_conf_apps, configpath );

    if ( apph ) {
      appspath = pnd_conf_get_as_char ( apph, PND_APPS_KEY );

      if ( ! appspath ) {
	appspath = PND_APPS_SEARCHPATH;
      }

      overridespath = pnd_conf_get_as_char ( apph, PND_PXML_OVERRIDE_KEY );

      if ( ! overridespath ) {
	overridespath = PND_PXML_OVERRIDE_SEARCHPATH;
      }

    } else {
      // couldn't find a useful app search path so use the default
      appspath = PND_APPS_SEARCHPATH;
      overridespath = PND_PXML_OVERRIDE_SEARCHPATH;
    }

    // given app-config, try to locate a runscript
    char *run_searchpath;
    char *run_script;
    char *pndrun;

    if ( apph ) {
      run_searchpath = pnd_conf_get_as_char ( apph, PND_PNDRUN_SEARCHPATH_KEY );
      run_script = pnd_conf_get_as_char ( apph, PND_PNDRUN_KEY );
      pndrun = NULL;

      if ( ! run_searchpath ) {
	run_searchpath = PND_APPS_SEARCHPATH;
	run_script = PND_PNDRUN_FILENAME;
      }

    } else {
      run_searchpath = NULL;
      run_script = NULL;
      pndrun = PND_PNDRUN_DEFAULT;
    }

    if ( ! pndrun ) {
      pndrun = pnd_locate_filename ( run_searchpath, run_script );
    }

    // hand back to main proggy
    pnd_run = pndrun; // lame, fix this

  } // try to locate runscript

  if ( ! pnd_run ) {
    printf ( "Runscript could not be determined. Fail.\n" );
    exit ( 0 );
  }

  if ( ! pndfile ) {
    usage ( argv );
    exit ( 0 );
  }

  // summary
  printf ( "Runscript\t%s\n", pnd_run );
  printf ( "Pndfile\t%s\n", pndfile );
  printf ( "Kill X11\t%s\n", no_x11 ? "true" : "false" );

  // sadly, to launch a pnd-file we need to know what the executable is in there
  unsigned int pxmlbuflen = 96 * 1024; // lame, need to calculate it
  char *pxmlbuf = malloc ( pxmlbuflen );
  if ( ! pxmlbuf ) {
    printf ( "ERROR: RAM exhausted!\n" );
    exit ( 0 );
  }
  memset ( pxmlbuf, '\0', pxmlbuflen );

  FILE *f = fopen ( pndfile, "r" );
  if ( ! f ) {
    printf ( "ERROR: Couldn't open pndfile %s!\n", pndfile );
    exit ( 0 );
  }

  pnd_pxml_handle h = NULL;
  if ( pnd_pnd_seek_pxml ( f ) ) {
    if ( pnd_pnd_accrue_pxml ( f, pxmlbuf, pxmlbuflen ) ) {
      h = pnd_pxml_fetch_buffer ( "pnd_run", pxmlbuf );
    }
  }

  fclose ( f );

  if ( ! h ) {
    printf ( "ERROR: Couldn't pull PXML.xml from the pndfile.\n" );
    exit ( 0 );
  }

  // attempt to invoke
  unsigned int options = 0;
  if ( no_x11 ) {
    options |= PND_EXEC_OPTION_NOX11;
  }

  unsigned int clock = 200;
  if ( pnd_pxml_get_clockspeed ( h ) ) {
    clock = atoi ( pnd_pxml_get_clockspeed ( h ) );
  }

  pnd_apps_exec ( pnd_run, pndfile,
		  pnd_pxml_get_unique_id ( h ),
		  pnd_pxml_get_exec ( h ),
		  pnd_pxml_get_startdir ( h ),
		  clock,
		  options );

  return ( 0 );
} // main
