
#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_pxml.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"

int main ( int argc, char *argv[] ) {
  char *configpath;
  char *appspath;
  char *overridespath;
  unsigned char i;
  unsigned char do_exec = 0;
  unsigned char do_icon = 0;
  unsigned char do_dotdesktop = 0;

  for ( i = 1; i < argc; i++ ) {

    if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'e' ) {
      printf ( "Will attempt a random exec.\n" );
      do_exec = 1;
    } else if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'i' ) {
      printf ( "Will attempt to extract icons.\n" );
      do_icon = 1;
    } else if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'd' ) {
      printf ( "Will attempt to extract dotdesktop.\n" );
      do_dotdesktop = 1;
    } else {
      printf ( "%s [-e] [-i] [-d]\n", argv [ 0 ] );
      printf ( "-e\tOptional. Attempt to exec a random app.\n" );
      printf ( "-i\tOptional. Attempt to dump icon files from the end of pnd's to /tmp.\n" );
      printf ( "-d\tOptional. Attempt to dump dotdesktop files from the end of pnd's to /tmp.\n" );
      exit ( 0 );
    }

  }

  /* attempt to sort out the config file madness
   */

  // attempt to fetch a sensible default searchpath for configs
  configpath = pnd_conf_query_searchpath();

  // attempt to fetch the apps config to pick up a searchpath
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

  printf ( "Apps searchpath is '%s'\n", appspath );
  printf ( "Apps overrides searchpath is '%s'\n", overridespath );

  /* find pnd runscript
   */
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

  if ( run_searchpath ) printf ( "Locating pnd run in %s\n", run_searchpath );
  if ( run_script ) printf ( "Locating pnd runscript as %s\n", run_script );
  if ( pndrun ) printf ( "Default pndrun is %s\n", pndrun );

  /* attempt to discover apps in the path
   */
  pnd_box_handle applist;

  applist = pnd_disco_search ( appspath, overridespath );

  // list the found apps (if any)

  if ( applist ) {
    pnd_disco_t *d = pnd_box_get_head ( applist );

    while ( d ) {

      // display the app 'as is'

      printf ( "App: %s (type %u)\n", pnd_box_get_key ( d ), d -> object_type );

      printf ( "  Base path: %s\n", d -> path_to_object );

      if ( d -> title_en ) {
	printf ( "  Name: %s\n", d -> title_en );
      }
      if ( d -> icon ) {
	printf ( "  Icon: %s\n", d -> icon );
      }
      if ( d -> pnd_icon_pos ) {
	printf ( "  Icon in pnd might be at: %u\n", d -> pnd_icon_pos );
      }
      if ( d -> unique_id ) {
	printf ( "  Unique ID: %s\n", d -> unique_id );
      }
      if ( d -> main_category ) {
	printf ( "  Category: %s\n", d -> main_category );
      }
      if ( d -> exec ) {
	printf ( "  Executable: %s\n", d -> exec );
      }
      if ( d -> startdir ) {
	printf ( "  Start dir: %s\n", d -> startdir );
      }
      if ( d -> clockspeed ) {
	printf ( "  Clockspeed: %s\n", d -> clockspeed );
      }

      if ( do_dotdesktop ) {
	pnd_emit_dotdesktop ( "/tmp", pndrun, d );
      }

      if ( do_icon ) {
	pnd_emit_icon ( "/tmp", d );
      }

      // next!
      d = pnd_box_get_next ( d );

    } // while applist

  } else {
    printf ( "No applications found in search path\n" );
  }

  // lets toy with executing an application
  if ( do_exec ) {

    if ( ! pndrun ) {
      printf ( "*** Couldn't locate a pnd runscript.\n" );
    } else {
      printf ( "Found a pnd runscript of %s\n", pndrun );

      pnd_disco_t *d = pnd_box_get_head ( applist );
      if ( d ) {
	d = pnd_box_get_next ( d );

	if ( d ) {
	  pnd_apps_exec ( pndrun, d -> path_to_object, d -> unique_id, d -> exec, d -> startdir, atoi ( d -> clockspeed ) );
	}
      }

    }

  } // do_exec?

  // exeunt with alarums
  free ( configpath );
  if ( apph ) {
    pnd_box_delete ( apph );
  }

  return ( 0 );
}
