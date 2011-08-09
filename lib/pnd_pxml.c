
#include <stdio.h> /* for FILE */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for string ops */

#include <sys/types.h> /* for stat */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat */

#include "pnd_pxml.h"
#include "pnd_pathiter.h"
#include "pnd_tinyxml.h"
#include "pnd_logger.h"

pnd_pxml_handle *pnd_pxml_fetch ( char *fullpath ) {
  FILE *f;
  char *b;
  unsigned int len;

  f = fopen ( fullpath, "r" );

  if ( ! f ) {
    return ( 0 );
  }

  fseek ( f, 0, SEEK_END );

  len = ftell ( f );

  fseek ( f, 0, SEEK_SET );

  if ( ! len ) {
    return ( NULL );
  }

  b = (char*) malloc ( len + 1 );

  if ( ! b ) {
    fclose ( f );
    return ( 0 );
  }

  fread ( b, 1, len, f );

  fclose ( f );

  return ( pnd_pxml_fetch_buffer ( fullpath, b ) );
}

pnd_pxml_handle *pnd_pxml_fetch_buffer ( char *filename, char *buffer ) {

  pnd_pxml_t **p = malloc ( sizeof(pnd_pxml_t*) * PXML_MAXAPPS );
  memset ( p, '\0', sizeof(pnd_pxml_t*) * PXML_MAXAPPS );

  if ( ! pnd_pxml_parse ( filename, buffer, strlen ( buffer ), p ) ) {
    return ( 0 );
  }

  return ( (pnd_pxml_handle*) p );
}

void pnd_pxml_delete ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;

  int i;
  if (p->titles) {
    for (i = 0; i < p->titles_c; i++)
    {
      free(p->titles[i].language);
      free(p->titles[i].string);
    }
    free(p->titles);
  }

  if (p->descriptions) {
    for (i = 0; i < p->descriptions_c; i++)
    {
      free(p->descriptions[i].language);
      free(p->descriptions[i].string);
    }
    free(p->descriptions);
  }

  if ( p -> standalone ) {
    free ( p -> standalone );
  }
  if ( p -> icon ) {
    free ( p -> icon );
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
  if ( p -> appdata_dirname ) {
    free ( p -> appdata_dirname );
  }

  free(p); /*very important!*/

  return;
}

void pnd_pxml_set_app_name ( pnd_pxml_handle h, char *v ) {
  /* 
   * Please do not use this function if it can be avoided; it is only here for compatibility.
   * The function might fail on low memory, and there's no way for the user to know when this happens.
   */
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  char has_en_field = 0;
  int i;

  if (!v) return; /*TODO: add error information? Make it possible to set the string to NULL?*/

  for (i = 0; i < p->titles_c; i++)
  {
    if (strncmp("en", p->titles[i].language, 2) == 0) /*strict comparison; match "en_US", "en_GB" etc... All these are set.*/
    {
      free(p->titles[i].string);
      p->titles[i].string = strdup(v);
      has_en_field = 1;
    }
  }

  if (!has_en_field)
  {
    p->titles_c++;
    if (p->titles_c > p->titles_alloc_c) //we don't have enough strings allocated
    {
      p->titles_alloc_c <<= 1;
      p->titles = (pnd_localized_string_t *)realloc((void*)p->titles, p->titles_alloc_c);
      if (!p->titles) return; //errno = ENOMEM
    }
    p->titles[p->titles_c - 1].language = "en_US";
    p->titles[p->titles_c - 1].string = strdup(v);
  }

  return;
}

unsigned char pnd_is_pxml_valid_app ( pnd_pxml_handle h ) {
  //pnd_pxml_t *p = (pnd_pxml_t*) h; //unused atm

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

#if 0 // TODO: Unfinished entirely now
  pnd_pxml_handle mergeh;

  if ( ! pnd_pxml_get_unique_id ( h ) ) {
    return ( -1 ); // no unique-id present, so can't use it to name potential override files
  }

  SEARCHPATH_PRE
  {

    // do it
    strncat ( buffer, "/", FILENAME_MAX );
    strncat ( buffer, pnd_pxml_get_unique_id ( h ), FILENAME_MAX );
    strncat ( buffer, ".xml", FILENAME_MAX );
    //printf ( "  Path to seek merges: '%s'\n", buffer );

    // TODO: handle multiple subapps!
    mergeh = pnd_pxml_fetch ( buffer );

    if ( mergeh ) {

      // TODO: handle all the various data bits
      if ( pnd_pxml_get_app_name_en ( mergeh ) ) {
	pnd_pxml_set_app_name ( h, pnd_pxml_get_app_name_en ( mergeh ) );
      }

      pnd_pxml_delete ( mergeh );
    }

  }
  SEARCHPATH_POST
#endif

  return ( retval );
}

char *pnd_pxml_get_best_localized_string(pnd_localized_string_t strings[], int strings_c, char *iso_lang)
{
  int i;
  int similarity_weight = 0xffff; /*Set to something Really Bad in the beginning*/
  char *best_match = NULL;

  for(i = 0; i < strings_c; i++)
  {
    // factor in the length -- if we're given 'en' and have a string 'en_US', thats better than 'de_something'; if we don't
    // use length, then en_US and de_FO are same to 'en'.
    int maxcount = strlen ( strings[i].language ) < strlen ( iso_lang ) ? strlen ( strings[i].language ) : strlen ( iso_lang );
    int new_weight = abs(strncmp(strings[i].language, iso_lang, maxcount));
    //pnd_log ( PND_LOG_DEFAULT, "looking for lang %s, looking at lang %s (weight %d, old %d): %s\n",
    //          iso_lang, strings [ i ].language, new_weight, similarity_weight, strings [ i ].string );
    if (new_weight < similarity_weight)
    {
      similarity_weight = new_weight;
      best_match = strings[i].string;
    }
  }

  if ( best_match ) {
    //pnd_log ( PND_LOG_DEFAULT, "best match: %s\n", best_match );
    return strdup(best_match);
  }

  //pnd_log ( PND_LOG_DEFAULT, "best match: FAIL\n" );

  return ( NULL );
}

char *pnd_pxml_get_package_id ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> package_id );
}

char *pnd_pxml_get_app_name ( pnd_pxml_handle h, char *iso_lang ) {
  pnd_pxml_t *p = (pnd_pxml_t *) h;
  return pnd_pxml_get_best_localized_string(p->titles, p->titles_c, iso_lang);
}

char *pnd_pxml_get_app_name_en ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_name(h, "en");
}

char *pnd_pxml_get_app_name_de ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_name(h, "de");
}

char *pnd_pxml_get_app_name_it ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_name(h, "it");
}

char *pnd_pxml_get_app_name_fr ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_name(h, "fr");
}
char *pnd_pxml_get_unique_id ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> unique_id );
}

char *pnd_pxml_get_appdata_dirname ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> appdata_dirname );
}

char *pnd_pxml_get_standalone ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> standalone );
}

char *pnd_pxml_get_icon ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> icon );
}

// this guy's func name is 'out of sync' with the family of functions below; but since it
// exists, rather than just remove it and break someones code, will add in the appropriate
// function wrapper; the header only specifies the other guy (always did), so the header
// was already on the right path.
char *pnd_pxml_get_app_description ( pnd_pxml_handle h, char *iso_lang ) {
  pnd_pxml_t *p = (pnd_pxml_t *) h;
  return pnd_pxml_get_best_localized_string(p->descriptions, p->descriptions_c, iso_lang);
}

char *pnd_pxml_get_description_en ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_description(h, "en");
}

char *pnd_pxml_get_description_de ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_description(h, "de");
}

char *pnd_pxml_get_description_it ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_description(h, "it");
}

char *pnd_pxml_get_description_fr ( pnd_pxml_handle h ) {
  return pnd_pxml_get_app_description(h, "fr");
}

// wrapper added so family of function names is consistent; see comment for pnd_pxml_get_app_description() above
char *pnd_pxml_get_description ( pnd_pxml_handle h, char *iso_lang) {
  return ( pnd_pxml_get_app_description ( h, iso_lang ) );
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

char *pnd_pxml_get_execargs ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> execargs );
}

char *pnd_pxml_get_exec_option_no_x11 ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> exec_no_x11 );
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

char *pnd_pxml_get_mkdir ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> mkdir_sp );
}

unsigned char pnd_pxml_is_affirmative ( char *v ) {

  if ( ! v ) {
    return ( 0 );
  }

  if ( ( v [ 0 ] == 'Y' ) ||
       ( v [ 0 ] == 'y' ) ||
       ( v [ 0 ] == '1' ) )
  {
    return ( 0 );
  }

  return ( 0 );
}

pnd_pxml_x11_req_e pnd_pxml_get_x11 ( char *pxmlvalue ) {

  if ( ! pxmlvalue ) {
    return ( pnd_pxml_x11_ignored );
  } else if ( strcasecmp ( pxmlvalue, "req" ) == 0 ) {
    return ( pnd_pxml_x11_required );
  } else if ( strcasecmp ( pxmlvalue, "stop" ) == 0 ) {
    return ( pnd_pxml_x11_stop );
  } else if ( strcasecmp ( pxmlvalue, "ignore" ) == 0 ) {
    return ( pnd_pxml_x11_ignored );
  }

  return ( pnd_pxml_x11_ignored ); // default
}

char *pnd_pxml_get_info_name ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> info_name );
}

char *pnd_pxml_get_info_type ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> info_type );
}

char *pnd_pxml_get_info_src ( pnd_pxml_handle h ) {
  pnd_pxml_t *p = (pnd_pxml_t*) h;
  return ( p -> info_filename );
}
