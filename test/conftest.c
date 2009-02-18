
#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"

int main ( void ) {

  // attempt to fetch a sensible default searchpath for configs
  char *configpath = pnd_conf_query_searchpath();
  printf ( "Config searchpath is: '%s'\n", configpath );

  // attempt to fetch the 'apps' config
  pnd_conf_handle apph;

  apph = pnd_conf_fetch_by_id ( pnd_conf_apps, configpath );

  if ( ! apph ) {
    printf ( "Couldn't locate apps config!\n" );
    return ( -1 );
  }

  // dump the config file
  printf ( "Config file name is: '%s'\n", pnd_box_get_name ( apph ) );
  char *value = pnd_box_get_head ( apph );
  printf ( "Config has key '%s'\n", pnd_box_get_key ( value ) );
  printf ( "Config has value '%s'\n", value );
  while ( ( value = pnd_box_get_next ( value ) ) ) {
    printf ( "Config has key '%s'\n", pnd_box_get_key ( value ) );
    printf ( "Config has value '%s'\n", value );
  }

  // lets query the apps config
  char *binpath;

  binpath = pnd_conf_get_as_char ( apph, PND_APPS_KEY );

  if ( ! binpath ) {
    printf ( "Couldn't locate the app auto-discovery searchpath!\n" );
    return ( -2 );
  }

  printf ( "Located auto-discovery searchpath '%s'\n", binpath );

  // exeunt with alarums
  free ( configpath );
  pnd_box_delete ( apph );

  return ( 0 );
}
