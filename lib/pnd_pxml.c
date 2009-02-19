
#include <stdio.h> /* for FILE */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for string ops */

#include <sys/types.h> /* for stat */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat */

#include "pnd_pxml.h"
#include "pnd_pathiter.h"

void pnd_pxml_load(const char* pFilename, pnd_pxml_t *app);

pnd_pxml_handle pnd_pxml_fetch ( char *fullpath ) {

  pnd_pxml_t *p = malloc ( sizeof(pnd_pxml_t) );

  memset ( p, '\0', sizeof(pnd_pxml_t) );

  pnd_pxml_load ( fullpath, p );

  return ( p );
}

void pnd_pxml_delete ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;

  if ( p -> title_en ) {
    free ( p -> title_en );
  }

  if ( p -> icon ) {
    free ( p -> icon );
  }

  if ( p -> exec ) {
    free ( p -> exec );
  }
  if ( p -> main_category ) {
    free ( p -> main_category );
  }
  if ( p -> unique_id ) {
    free ( p -> unique_id );
  }
  if ( p -> clockspeed ) {
    free ( p -> clockspeed );
  }

  return;
}

char *pnd_pxml_get_app_name ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> title_en );
}

char *pnd_pxml_get_icon_path ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> icon );
}

char *pnd_pxml_get_clockspeed ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> clockspeed );
}

void pnd_pxml_set_app_name ( pnd_pxml_handle h, char *v ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  if ( p -> title_en ) {
    free ( p -> title_en );
    p -> title_en = NULL;
  }

  if ( v ) {
    p -> title_en = strdup ( v );
  }

  return;
}

char *pnd_pxml_get_unique_id ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> unique_id );
}

char *pnd_pxml_get_primary_category ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> main_category );
}

char *pnd_pxml_get_exec_path ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> exec );
}

unsigned char pnd_is_pxml_valid_app ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;

  // for now, lets just verify the exec-path is valid
  //printf ( "exec is '%s'\n", p -> exec );

  struct stat buf;
  if ( stat ( p -> exec, &buf ) == 0 ) {
    return ( 1 ); // path is present
  }

  return ( 0 );
}

signed char pnd_pxml_merge_override ( pnd_pxml_handle h, char *searchpath ) {
  // the pxml includes a unique-id; use this value to attempt to find an
  // override in the given searchpath
  signed char retval = 0;
  pnd_pxml_handle mergeh;

  SEARCHPATH_PRE
  {

    // do it
    strncat ( buffer, "/", FILENAME_MAX );
    strncat ( buffer, pnd_pxml_get_unique_id ( h ), FILENAME_MAX );
    strncat ( buffer, ".xml", FILENAME_MAX );
    //printf ( "  Path to seek merges: '%s'\n", buffer );

    mergeh = pnd_pxml_fetch ( buffer );

    if ( mergeh ) {

      if ( pnd_pxml_get_app_name ( mergeh ) ) {
	pnd_pxml_set_app_name ( h, pnd_pxml_get_app_name ( mergeh ) );
      }

      pnd_pxml_delete ( mergeh );
    }

  }
  SEARCHPATH_POST

  return ( retval );
}
