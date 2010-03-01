
#include <stdio.h> /* for FILE etc */
#include <string.h>
#include <unistd.h> /* for unlink */
#include <stdlib.h> /* for free */

#include "pnd_apps.h"
#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_discovery.h"
#include "pnd_pndfiles.h"
#include "pnd_conf.h"
#include "pnd_desktop.h"
#include "pnd_logger.h"

unsigned char pnd_emit_dotdesktop ( char *targetpath, char *pndrun, pnd_disco_t *p ) {
  char filename [ FILENAME_MAX ];
  char buffer [ 1024 ];
  FILE *f;

  // specification
  // http://standards.freedesktop.org/desktop-entry-spec

  // validation

  if ( ! p -> unique_id ) {
    pnd_log ( PND_LOG_DEFAULT, "Can't emit dotdesktop for %s, missing unique-id\n", targetpath );
    return ( 0 );
  }

  if ( ! p -> exec ) {
    pnd_log ( PND_LOG_DEFAULT, "Can't emit dotdesktop for %s, missing exec\n", targetpath );
    return ( 0 );
  }

  if ( ! targetpath ) {
    pnd_log ( PND_LOG_DEFAULT, "Can't emit dotdesktop for %s, missing target path\n", targetpath );
    return ( 0 );
  }

  if ( ! pndrun ) {
    pnd_log ( PND_LOG_DEFAULT, "Can't emit dotdesktop for %s, missing pnd_run.sh\n", targetpath );
    return ( 0 );
  }

  // set up

  sprintf ( filename, "%s/%s#%u.desktop", targetpath, p -> unique_id, p -> subapp_number );

  // emit

  //printf ( "EMIT DOTDESKTOP '%s'\n", filename );

  f = fopen ( filename, "w" );

  if ( ! f ) {
    return ( 0 );
  }

  fprintf ( f, "%s\n", PND_DOTDESKTOP_HEADER );

  if ( p -> title_en ) {
    snprintf ( buffer, 1020, "Name=%s\n", p -> title_en );
    fprintf ( f, "%s", buffer );
  }

  fprintf ( f, "Type=Application\n" );
  fprintf ( f, "Version=1.0\n" );

  if ( p -> icon ) {
    snprintf ( buffer, 1020, "Icon=%s\n", p -> icon );
    fprintf ( f, "%s", buffer );
  }

  if ( p -> unique_id ) {
    fprintf ( f, "X-Pandora-UID=%s\n", p -> unique_id );
  }

#if 1
  if ( p -> desc_en && p -> desc_en [ 0 ] ) {
    snprintf ( buffer, 1020, "Comment=%s\n", p -> desc_en ); // no [en] needed I suppose, yet
    fprintf ( f, "%s", buffer );
  }
#endif

#if 0 // we let pnd_run.sh handle this
  if ( p -> startdir ) {
    snprintf ( buffer, 1020, "Path=%s\n", p -> startdir );
    fprintf ( f, "%s", buffer );
  } else {
    fprintf ( f, "Path=%s\n", PND_DEFAULT_WORKDIR );
  }
#endif

  if ( p -> exec ) {
    char *nohup;

    if ( p -> option_no_x11 ) {
      nohup = "/usr/bin/nohup ";
    } else {
      nohup = "";
    }

    // basics
    if ( p -> object_type == pnd_object_type_directory ) {
      snprintf ( buffer, 1020, "Exec=%s%s -p %s -e %s -b %s",
		 nohup, pndrun, p -> object_path, p -> exec, p -> unique_id );
    } else if ( p -> object_type == pnd_object_type_pnd ) {
      snprintf ( buffer, 1020, "Exec=%s%s -p %s/%s -e %s -b %s",
		 nohup, pndrun, p -> object_path, p -> object_filename, p -> exec, p -> unique_id );
    }

    // start dir
    if ( p -> startdir ) {
      strncat ( buffer, " -s ", 1020 );
      strncat ( buffer, p -> startdir, 1020 );
    }

    // clockspeed
    if ( p -> clockspeed && atoi ( p -> clockspeed ) != 0 ) {
      strncat ( buffer, " -c ", 1020 );
      strncat ( buffer, p -> clockspeed, 1020 );
    }

    // exec options
    if ( pnd_pxml_is_affirmative ( p -> option_no_x11 ) ) {
      strncat ( buffer, " -x ", 1020 );
    }

    // newline
    strncat ( buffer, "\n", 1020 );

    // emit
    fprintf ( f, "%s", buffer );
  }

  // categories
  {
    char cats [ 512 ] = "";
    int n;
    pnd_conf_handle c;
    char *confpath;

    // uuuuh, defaults?
    // "Application" used to be in the standard and is commonly supported still
    // Utility and Network should ensure the app is visible 'somewhere' :/
    char *defaults = PND_DOTDESKTOP_DEFAULT_CATEGORY;

    // determine searchpath (for conf, not for apps)
    confpath = pnd_conf_query_searchpath();

    // inhale the conf file
    c = pnd_conf_fetch_by_id ( pnd_conf_categories, confpath );

    // if we can find a default category set, pull it in; otherwise assume
    // the hardcoded one
    if ( pnd_conf_get_as_char ( c, "default" ) ) {
      defaults = pnd_conf_get_as_char ( c, "default" );
    }

    // ditch the confpath
    free ( confpath );

    // attempt mapping
    if ( c ) {

      n = pnd_map_dotdesktop_categories ( c, cats, 511, p );

      if ( n ) {
	fprintf ( f, "Categories=%s\n", cats );
      } else {
	fprintf ( f, "Categories=%s\n", defaults );
      }

    } else {
      fprintf ( f, "Categories=%s\n", defaults );
    }

  }

  fprintf ( f, "%s\n", PND_DOTDESKTOP_SOURCE ); // should we need to know 'who' created the file during trimming

  fclose ( f );

  return ( 1 );
}

unsigned char pnd_emit_icon ( char *targetpath, pnd_disco_t *p ) {
  //#define BITLEN (8*1024)
#define BITLEN (64*1024)
  char buffer [ FILENAME_MAX ]; // target filename
  char from [ FILENAME_MAX ];   // source filename
  char bits [ BITLEN ];
  unsigned int bitlen;
  FILE *pnd, *target;

  // prelim .. if a pnd file, and no offset found, discovery code didn't locate icon.. so bail.
  if ( ( p -> object_type == pnd_object_type_pnd ) &&
       ( ! p -> pnd_icon_pos ) )
  {
    return ( 0 ); // discover code didn't find it, so FAIL
  }

  // determine filename for target
  sprintf ( buffer, "%s/%s.png", targetpath, p -> unique_id /*, p -> subapp_number*/ ); // target

  /* first.. open the source file, by type of application:
   * are we looking through a pnd file or a dir?
   */
  if ( p -> object_type == pnd_object_type_directory ) {
    sprintf ( from, "%s/%s", p -> object_path, p -> icon );
  } else if ( p -> object_type == pnd_object_type_pnd ) {
    sprintf ( from, "%s/%s", p -> object_path, p -> object_filename );
  }

  pnd = fopen ( from, "r" );

  if ( ! pnd ) {
    return ( 0 );
  }

  unsigned int len;

  target = fopen ( buffer, "wb" );

  if ( ! target ) {
    fclose ( pnd );
    return ( 0 );
  }

  fseek ( pnd, 0, SEEK_END );
  len = ftell ( pnd );
  //fseek ( pnd, 0, SEEK_SET );

  fseek ( pnd, p -> pnd_icon_pos, SEEK_SET );

  len -= p -> pnd_icon_pos;

  while ( len ) {

    if ( len > (BITLEN) ) {
      bitlen = (BITLEN);
    } else {
      bitlen = len;
    }

    if ( fread ( bits, bitlen, 1, pnd ) != 1 ) {
      fclose ( pnd );
      fclose ( target );
      unlink ( buffer );
      return ( 0 );
    }

    if ( fwrite ( bits, bitlen, 1, target ) != 1 ) {
      fclose ( pnd );
      fclose ( target );
      unlink ( buffer );
      return ( 0 );
    }

    len -= bitlen;
  } // while

  fclose ( pnd );
  fclose ( target );

  return ( 1 );
}

#if 1 // we switched direction to freedesktop standard categories
// if no categories herein, return 0; otherwise, if some category-like-text, return 1
int pnd_map_dotdesktop_categories ( pnd_conf_handle c, char *target_buffer, unsigned short int len, pnd_disco_t *d ) {
  char *t;
  char *match;

  // clear target so we can easily append
  memset ( target_buffer, '\0', len );

  // for each main-cat and sub-cat, including alternates, just append them all together
  // we'll try mapping them, since the categories file is there, but we'll default to
  // copying over; this lets the conf file do merging or renaming of cagtegories, which
  // could still be useful, but we can leave the conf file empty to effect a pure
  // trusted-PXML-copying

  // it would be sort of cumbersome to copy all the freedesktop.org defined categories (as
  // there are hundreds), and would also mean new ones and peoples custom ones would
  // flop

  /* attempt primary category chain
   */
  #define MAPCAT(field)				            \
    if ( ( t = d -> field ) ) {                             \
      match = pnd_map_dotdesktop_category ( c, t );         \
      strncat ( target_buffer, match ? match : t, len );    \
      strncat ( target_buffer, ";", len );                  \
    }

  MAPCAT(main_category);
  MAPCAT(main_category1);
  MAPCAT(main_category2);
  MAPCAT(alt_category);
  MAPCAT(alt_category1);
  MAPCAT(alt_category2);

  if ( target_buffer [ 0 ] ) {
    return ( 1 ); // I guess its 'good'?
  }

#if 0
  if ( ( t = d -> main_category ) ) {
    match = pnd_map_dotdesktop_category ( c, t );
    strncat ( target_buffer, match ? match : t, len );
    strncat ( target_buffer, ";", len );
  }
#endif

  return ( 0 );
}
#endif

#if 0 // we switched direction
//int pnd_map_dotdesktop_categories ( pnd_conf_handle c, char *target_buffer, unsigned short int len, pnd_pxml_handle h ) {
int pnd_map_dotdesktop_categories ( pnd_conf_handle c, char *target_buffer, unsigned short int len, pnd_disco_t *d ) {
  unsigned short int n = 0; // no. matches
  char *t;
  char *match;

  // clear target so we can easily append
  memset ( target_buffer, '\0', len );

  /* attempt primary category chain
   */
  match = NULL;

  if ( ( t = d -> main_category ) ) {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = d -> main_category1 ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = d -> main_category2 ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }
  
  if ( match ) {
    strncat ( target_buffer, match, len );
    len -= strlen ( target_buffer );
    n += 1;
  }

  /* attempt secondary category chain
   */
  match = NULL;

  if ( ( t = d -> alt_category ) ) {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = d -> alt_category1 ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = d -> alt_category2 ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }
  
  if ( match ) {
    if ( target_buffer [ 0 ] != '\0' && len > 0 ) {
      strcat ( target_buffer, ";" );
      len -= 1;
    }
    strncat ( target_buffer, match, len );
    len -= strlen ( target_buffer );
    n += 1;
  }

#if 0 // doh, originally I was using pxml-t till I realized I pre-boned myself on that one
  match = NULL;

  if ( ( t = pnd_pxml_get_main_category ( h ) ) ) {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = pnd_pxml_get_subcategory1 ( h ) ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = pnd_pxml_get_subcategory2 ( h ) ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }
  
  if ( match ) {
    strncat ( target_buffer, match, len );
    len -= strlen ( target_buffer );
    n += 1;
  }

  /* attempt secondary category chain
   */
  match = NULL;

  if ( ( t = pnd_pxml_get_altcategory ( h ) ) ) {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = pnd_pxml_get_altsubcategory1 ( h ) ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }

  if ( ( ! match ) &&
       ( t = pnd_pxml_get_altsubcategory2 ( h ) ) )
  {
    match = pnd_map_dotdesktop_category ( c, t );
  }
  
  if ( match ) {
    if ( target_buffer [ 0 ] != '\0' && len > 0 ) {
      strcat ( target_buffer, ";" );
      len -= 1;
    }
    strncat ( target_buffer, match, len );
    len -= strlen ( target_buffer );
    n += 1;
  }
#endif

  if ( n && len ) {
    strcat ( target_buffer, ";" );
  }

  return ( n );
}
#endif

// given category 'foo', look it up in the provided config map. return the char* reference, or NULL
char *pnd_map_dotdesktop_category ( pnd_conf_handle c, char *single_category ) {
  char *key;
  char *ret;

  key = malloc ( strlen ( single_category ) + 4 + 1 );

  sprintf ( key, "map.%s", single_category );

  ret = pnd_conf_get_as_char ( c, key );

  free ( key );

  return ( ret );
}

unsigned char *pnd_emit_icon_to_buffer ( pnd_disco_t *p, unsigned int *r_buflen ) {
  // this is shamefully mostly a copy of emit_icon() above; really, need to refactor that to use this routine
  // with a fwrite at the end...
  char from [ FILENAME_MAX ];   // source filename
  char bits [ 8 * 1024 ];
  unsigned int bitlen;
  FILE *pnd = NULL;
  unsigned char *target = NULL, *targiter = NULL;

  // prelim .. if a pnd file, and no offset found, discovery code didn't locate icon.. so bail.
  if ( ( p -> object_type == pnd_object_type_pnd ) &&
       ( ! p -> pnd_icon_pos ) )
  {
    return ( NULL ); // discover code didn't find it, so FAIL
  }

  /* first.. open the source file, by type of application:
   * are we looking through a pnd file or a dir?
   */
  if ( p -> object_type == pnd_object_type_directory ) {
    sprintf ( from, "%s/%s", p -> object_path, p -> icon );
  } else if ( p -> object_type == pnd_object_type_pnd ) {
    sprintf ( from, "%s/%s", p -> object_path, p -> object_filename );
  }

  pnd = fopen ( from, "r" );

  if ( ! pnd ) {
    return ( NULL );
  }

  // determine length of file, then adjust by icon position to find begin of icon
  unsigned int len;

  fseek ( pnd, 0, SEEK_END );
  len = ftell ( pnd );
  //fseek ( pnd, 0, SEEK_SET );

  fseek ( pnd, p -> pnd_icon_pos, SEEK_SET );

  len -= p -> pnd_icon_pos;

  // create target buffer
  target = malloc ( len );

  if ( ! target ) {
    fclose ( pnd );
    return ( 0 );
  }

  targiter = target;

  if ( r_buflen ) {
    *r_buflen = len;
  }

  // copy over icon to target
  while ( len ) {

    if ( len > (8*1024) ) {
      bitlen = (8*1024);
    } else {
      bitlen = len;
    }

    if ( fread ( bits, bitlen, 1, pnd ) != 1 ) {
      fclose ( pnd );
      free ( target );
      return ( NULL );
    }

    memmove ( targiter, bits, bitlen );
    targiter += bitlen;

    len -= bitlen;
  } // while

  fclose ( pnd );

  return ( target );
}
