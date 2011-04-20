
#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */
#include <string.h> /* for strdup */
#include <ctype.h> /* for isdigit */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"
#include "pnd_pndfiles.h"
#include "pnd_pxml.h"

static void usage ( char *argv[] ) {
  printf ( "%s [-r runscript] [-n] [-X] path-to-pndfile\n", argv [ 0 ] );
  printf ( "-r\tOptional. If not specified, will attempt to suss from configs.\n" );
  printf ( "-X\tOptional. If present, run sub-app number 'X'; ex: -0 for first, -1 for second, etc.\n" );
  printf ( "pndfile\tRequired. Full path to the pnd-file to execute.\n" );
  return;
}

int main ( int argc, char *argv[] ) {
  char *pnd_run = NULL;
  char *pndfile = NULL;
  unsigned char i;
  unsigned char subapp = 0;

  for ( i = 1; i < argc; i++ ) {

    if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'r' ) {
      pnd_run = argv [ i + 1 ];
      i++;
      if ( ! pnd_run ) {
	printf ( "-r specified, but no argument provided.\n" );
	exit ( 0 );
      }
    } else if ( argv [ i ][ 0 ] == '-' && isdigit(argv [ i ][ 1 ]) ) {
      subapp = atoi (argv[i] + 1);
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
    char *overridespath;

    // attempt to fetch a sensible default searchpath for configs
    configpath = pnd_conf_query_searchpath();

    // attempt to fetch the apps config. since it finds us the runscript
    pnd_conf_handle apph;

    apph = pnd_conf_fetch_by_id ( pnd_conf_apps, configpath );

    if ( apph ) {

      overridespath = pnd_conf_get_as_char ( apph, PND_PXML_OVERRIDE_KEY );

      if ( ! overridespath ) {
	overridespath = PND_PXML_OVERRIDE_SEARCHPATH;
      }

    } else {
      // couldn't find a useful app search path so use the default
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
  printf ( "Pndfile\t\t%s\n", pndfile );
  printf ( "Subapp Number\t%u\n", subapp );

  // figure out path and filename
  char *path, *filename;

  if ( strchr ( pndfile, '/' ) ) {
    char *foo = rindex ( pndfile, '/' );
    *foo = '\0';
    path = pndfile;
    filename = foo + 1;
  } else {
    path = "./";
    filename = pndfile;
  }

  // run it
  pnd_box_handle h = pnd_disco_file ( path, filename );

  if ( h ) {
    pnd_disco_t *d = pnd_box_get_head ( h );
    while ( subapp && d ) {
      if ( d -> title_en ) {
	printf ( "Skipping: '%s'\n", d -> title_en );
      }
      d = pnd_box_get_next ( d );
      subapp--;
    }
    if ( ! d ) {
      printf ( "No more applications in pnd-file.\n" );
      exit ( 0 );
    }
    if ( d -> title_en ) {
      printf ( "Invoking: '%s'\n", d -> title_en );
    }
    printf ( "--\n" );
    pnd_apps_exec_disco ( pnd_run, d, PND_EXEC_OPTION_BLOCK, NULL );
  }

  return ( 0 );
} // main
