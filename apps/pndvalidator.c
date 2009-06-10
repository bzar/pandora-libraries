
/* pndvalidator - a really dumb little tool to check a given PXML-app or pnd-app for 'compliance'.
 * The "PXML spec" is not hard-defined so hopefull yover time this tool will prove useful, if it is
 * updated to be 'tough enough' on PXML/pnd's and people actually use it
 */

#include <stdio.h>     // for stdio
#include <string.h>    // for strcmp
#include "pnd_conf.h"
#include "pnd_apps.h"
#include "pnd_pxml.h"
#include "pnd_utility.h"

int main ( int argc, char *argv[] ) {
  char *fullpath = NULL;
  unsigned char do_override = 0;
  unsigned char do_assets = 0;

  if ( argc <= 1 ) {
    printf ( "usage: %s [options] path-to-file\n", argv [ 0 ] );
    printf ( "\n" );
    printf ( "path-to-file\tCan refer to a PXML.xml in a subdir, or can refer to a .pnd-style application bundle\n" );
    printf ( "\n" );
    printf ( "Options:\n" );
    printf ( "\t-a\tattempt to mount and verify assets (executable, icon, screenshots, etc) are present\n" );
    printf ( "\t-m\tattempt to merge in PXML-overrides and validate against the result\n" );
    printf ( "\n" );
    printf ( "By default, the validator will only pick up the PXML and perform checks on field content.\n" );
    return ( 0 );
  }

  unsigned char i = 1;

  while ( i < argc ) {

    if ( strncmp ( argv [ i ], "-m", 2 ) == 0 ) {
      do_override = 1;
    } else if ( strncmp ( argv [ i ], "-a", 2 ) == 0 ) {
      do_assets = 1;
    } else {
      fullpath = argv [ i ];
    }

    i++;

  } // while

  // summarize

  if ( do_assets ) {
    printf ( "Note: Will attempt to examine application assets\n" );
  }

  if ( do_override ) {
    printf ( "Note: Will merge PXML-overrides if found (not an error if not found.)\n" );
  }

  if ( ! fullpath ) {
    printf ( "ERROR: No path provided.\n" );
    return ( 0 );
  }

  printf ( "Path to examine: %s\n", fullpath );

  printf ( "\n" );

  //
  // actually do useful work
  //

  pnd_pxml_handle pxmlh;

  pxmlh = pnd_pxml_get_by_path ( fullpath );

  if ( ! pxmlh ) {
    printf ( "ERROR: PXML could not be extracted meaningfully.\n" );
    return ( 0 );
  }

  printf ( "Got back a meaningful PXML structure.\n" );

  /* check the content
   */

  // check for required fields

  // exec-path?

  // app name?

  // unique ID

  // package-name (shortname)

  /* done!
   */
  pnd_pxml_delete ( pxmlh );

  return ( 0 );
}
