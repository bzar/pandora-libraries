
#include <stdio.h> // for stdio
#include <unistd.h> // for exit()
#include <stdlib.h> // for exit()

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_locate.h"

int main ( int argc, char *argv[] ) {
  char *configpath;
  char *pndpath;

  /* attempt to sort out the config file madness
   */

  // attempt to fetch a sensible default searchpath for configs
  configpath = pnd_conf_query_searchpath();

  // attempt to fetch app config, or default the pnd_run.sh location
  pnd_conf_handle apph;

  apph = pnd_conf_fetch_by_id ( pnd_conf_apps, configpath );

  if ( apph ) {
    pndpath = pnd_conf_get_as_char ( apph, PND_PNDRUN_SEARCHPATH_KEY );

    printf ( "Found a path in apps config: '%s'\n", pndpath );

    if ( ! pndpath ) {
      pndpath = PND_PNDRUN_SEARCHPATH_KEY;
    }

  } else {
    // couldn't find a useful app search path so use the default
    pndpath = PND_PNDRUN_SEARCHPATH_KEY;
  }

  // given a searchpath (Default or configured), try to find pnd_run.sh; if not
  // found, then just use the default
  char *pndrun;

  printf ( "Searching in path '%s'\n", pndpath );

  pndrun = pnd_locate_filename ( pndpath, PND_PNDRUN_FILENAME );

  if ( ! pndrun ) {
    printf ( "Result is default, not a locate-find\n" );
    pndrun = PND_PNDRUN_DEFAULT;
  }

  printf ( "Locate found pnd_run.sh in '%s'\n", pndrun );

  return ( 0 );
}
