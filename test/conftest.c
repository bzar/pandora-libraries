
#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */
#include <string.h> /* for strlen */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"

int main ( int argc, char *argv[] ) {

  // if an argument specified, try to load that one instead
  if ( argc > 1 ) {
    pnd_conf_handle h;
    h = pnd_conf_fetch_by_path ( argv [ 1 ] );
    char *i = pnd_box_get_head ( h );
    printf ( "%s -> %s [%p:%d]\n", pnd_box_get_key ( i ), i, i, strlen ( i ) );
    while ( ( i = pnd_box_get_next ( i ) ) ) {
      printf ( "%s -> %s [%p:%d]\n", pnd_box_get_key ( i ), i, i, strlen ( i ) );
    }

    char *poop = pnd_conf_get_as_char ( h, "info.viewer_args" );
    printf ( "info.viewer_args test: %s [%p:%d]\n", poop, poop, strlen ( poop ) );

    exit ( 0 );
  }

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
