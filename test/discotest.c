
#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_pxml.h"
#include "pnd_discovery.h"

int main ( void) {
  char *configpath;
  char *appspath;
  char *overridespath;

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

  /* attempt to discover apps in the path
   */
  pnd_box_handle applist;

  applist = pnd_disco_search ( appspath, overridespath );

  // list the found apps (if any)

  if ( applist ) {
    pnd_disco_t *d = pnd_box_get_head ( applist );

    while ( d ) {

      // display the app 'as is'

      printf ( "App: %s\n", pnd_box_get_key ( d ) );

      printf ( "  Base path: %s\n", d -> path_to_object );

      if ( d -> title_en ) {
	printf ( "  Name: %s\n", d -> title_en );
      }
      if ( d -> icon ) {
	printf ( "  Icon: %s\n", d -> icon );
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

      //pnd_emit_dotdesktop ( "/tmp", d );

      // next!
      d = pnd_box_get_next ( d );

    } // while applist

  } else {
    printf ( "No applications found in search path\n" );
  }

  // exeunt with alarums
  free ( configpath );
  if ( apph ) {
    pnd_box_delete ( apph );
  }

  return ( 0 );
}
