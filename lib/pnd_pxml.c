
#include <stdio.h> /* for FILE */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for string ops */

#include <sys/types.h> /* for stat */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat */

#include "pnd_pxml.h"
#include "pnd_pathiter.h"
#include "pnd_tinyxml.h"

pnd_pxml_handle pnd_pxml_fetch ( char *fullpath ) {

  pnd_pxml_t *p = malloc ( sizeof(pnd_pxml_t) );

  memset ( p, '\0', sizeof(pnd_pxml_t) );

  if ( ! pnd_pxml_load ( fullpath, p ) ) {
    return ( 0 );
  }

  return ( p );
}

pnd_pxml_handle pnd_pxml_fetch_buffer ( char *filename, char *buffer ) {

  pnd_pxml_t *p = malloc ( sizeof(pnd_pxml_t) );

  memset ( p, '\0', sizeof(pnd_pxml_t) );

  if ( ! pnd_pxml_parse ( filename, buffer, strlen ( buffer ), p ) ) {
    return ( 0 );
  }

  return ( p );
}

void pnd_pxml_delete ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;

  if ( p -> title_en ) {
    free ( p -> title_en );
  }
  if ( p -> title_de ) {
    free ( p -> title_de );
  }
  if ( p -> title_it ) {
    free ( p -> title_it );
  }
  if ( p -> title_fr ) {
    free ( p -> title_fr );
  }
  if ( p -> unique_id ) {
    free ( p -> unique_id );
  }
  if ( p -> standalone ) {
    free ( p -> standalone );
  }
  if ( p -> icon ) {
    free ( p -> icon );
  }
  if ( p -> description_en ) {
    free ( p -> description_en );
  }
  if ( p -> description_de ) {
    free ( p -> description_de );
  }
  if ( p -> description_it ) {
    free ( p -> description_it );
  }
  if ( p -> description_fr ) {
    free ( p -> description_fr );
  }
  if ( p -> previewpic1 ) {
    free ( p -> previewpic1 );
  }
  if ( p -> previewpic2 ) {
    free ( p -> previewpic2 );
  }
  if ( p -> author_name ) {
    free ( p -> author_name );
  }
  if ( p -> author_website ) {
    free ( p -> author_website );
  }
  if ( p -> version_major ) {
    free ( p -> version_major );
  }
  if ( p -> version_minor ) {
    free ( p -> version_minor );
  }
  if ( p -> version_release ) {
    free ( p -> version_release );
  }
  if ( p -> version_build ) {
    free ( p -> version_build );
  }
  if ( p -> exec ) {
    free ( p -> exec );
  }
  if ( p -> main_category ) {
    free ( p -> main_category );
  }
  if ( p -> subcategory1 ) {
    free ( p -> subcategory1 );
  }
  if ( p -> subcategory2 ) {
    free ( p -> subcategory2 );
  }
  if ( p -> altcategory ) {
    free ( p -> altcategory );
  }
  if ( p -> altsubcategory1 ) {
    free ( p -> altsubcategory1 );
  }
  if ( p -> altsubcategory2 ) {
    free ( p -> altsubcategory2 );
  }
  if ( p -> osversion_major ) {
    free ( p -> osversion_major );
  }
  if ( p -> osversion_minor ) {
    free ( p -> osversion_minor );
  }
  if ( p -> osversion_release ) {
    free ( p -> osversion_release );
  }
  if ( p -> osversion_build ) {
    free ( p -> osversion_build );
  }
  if ( p -> associationitem1_name ) {
    free ( p -> associationitem1_name );
  }
  if ( p -> associationitem1_filetype ) {
    free ( p -> associationitem1_filetype );
  }
  if ( p -> associationitem1_parameter ) {
    free ( p -> associationitem1_parameter );
  }
  if ( p -> associationitem2_name ) {
    free ( p -> associationitem2_name );
  }
  if ( p -> associationitem2_filetype ) {
    free ( p -> associationitem2_filetype );
  }
  if ( p -> associationitem2_parameter ) {
    free ( p -> associationitem2_parameter );
  }
  if ( p -> associationitem3_name ) {
    free ( p -> associationitem3_name );
  }
  if ( p -> associationitem1_filetype ) {
    free ( p -> associationitem3_filetype );
  }
  if ( p -> associationitem1_parameter ) {
    free ( p -> associationitem3_parameter );
  }
  if ( p -> clockspeed ) {
    free ( p -> clockspeed );
  }
  if ( p -> background ) {
    free ( p -> background );
  }
  if ( p -> startdir ) {
    free ( p -> startdir );
  }

  return;
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

unsigned char pnd_is_pxml_valid_app ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;

  // for now, lets just verify the exec-path is valid

  //printf ( "exec is '%s'\n", p -> exec );

  return ( 1 );

  // even this is complicated by pnd_run.sh semantics .. can't check if it exists
  // during discovery, since it is not mounted yet..
#if 0
  struct stat buf;
  if ( stat ( p -> exec, &buf ) == 0 ) {
    return ( 1 ); // path is present
  }
#endif

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

      if ( pnd_pxml_get_app_name_en ( mergeh ) ) {
	pnd_pxml_set_app_name ( h, pnd_pxml_get_app_name_en ( mergeh ) );
      }

      pnd_pxml_delete ( mergeh );
    }

  }
  SEARCHPATH_POST

  return ( retval );
}

char *pnd_pxml_get_app_name_en ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> title_en );
}

char *pnd_pxml_get_app_name_de ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> title_de );
}

char *pnd_pxml_get_app_name_it ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> title_it );
}

char *pnd_pxml_get_app_name_fr ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> title_fr );
}

char *pnd_pxml_get_unique_id ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> unique_id );
}

char *pnd_pxml_get_standalone ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> standalone );
}

char *pnd_pxml_get_icon ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> icon );
}

char *pnd_pxml_get_description_en ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> description_en );
}

char *pnd_pxml_get_description_de ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> description_de );
}

char *pnd_pxml_get_description_it ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> description_it );
}

char *pnd_pxml_get_description_fr ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> description_fr );
}

char *pnd_pxml_get_previewpic1 ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> previewpic1 );
}

char *pnd_pxml_get_previewpic2 ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> previewpic2 );
}

char *pnd_pxml_get_author_name ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> author_name );
}

char *pnd_pxml_get_author_website ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> author_website );
}

char *pnd_pxml_get_version_major ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> version_major );
}

char *pnd_pxml_get_version_minor ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> version_minor );
}

char *pnd_pxml_get_version_release ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> version_release );
}

char *pnd_pxml_get_version_build ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> version_build );
}

char *pnd_pxml_get_exec ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> exec );
}

char *pnd_pxml_get_main_category ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> main_category );
}

char *pnd_pxml_get_subcategory1 ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> subcategory1 );
}

char *pnd_pxml_get_subcategory2 ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> subcategory2 );
}

char *pnd_pxml_get_altcategory ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> altcategory );
}

char *pnd_pxml_get_altsubcategory1 ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> altsubcategory1 );
}

char *pnd_pxml_get_altsubcategory2 ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> altsubcategory2 );
}

char *pnd_pxml_get_osversion_major ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> osversion_major );
}

char *pnd_pxml_get_osversion_minor ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> osversion_minor );
}

char *pnd_pxml_get_osversion_release ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> osversion_release );
}

char *pnd_pxml_get_osversion_build ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> osversion_build );
}

char *pnd_pxml_get_associationitem1_name ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem1_name );
}

char *pnd_pxml_get_associationitem1_filetype ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem1_filetype );
}

char *pnd_pxml_get_associationitem1_parameter ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem1_parameter );
}

char *pnd_pxml_get_associationitem2_name ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem2_name );
}

char *pnd_pxml_get_associationitem2_filetype ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem2_filetype );
}

char *pnd_pxml_get_associationitem2_parameter ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem2_parameter );
}

char *pnd_pxml_get_associationitem3_name ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem3_name );
}

char *pnd_pxml_get_associationitem3_filetype ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem3_filetype );
}

char *pnd_pxml_get_associationitem3_parameter ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> associationitem3_parameter );
}

char *pnd_pxml_get_clockspeed ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> clockspeed );
}

char *pnd_pxml_get_background ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> background );
}

char *pnd_pxml_get_startdir ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> startdir );
}
