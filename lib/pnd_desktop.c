
#include <stdio.h> /* for FILE etc */
#include <string.h>
#include <unistd.h> /* for unlink */

#include "pnd_apps.h"
#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_discovery.h"
#include "pnd_pndfiles.h"

unsigned char pnd_emit_dotdesktop ( char *targetpath, char *pndrun, pnd_disco_t *p ) {
  char filename [ FILENAME_MAX ];
  char buffer [ 1024 ];
  FILE *f;

  // specification
  // http://standards.freedesktop.org/desktop-entry-spec

  // validation

  if ( ! p -> unique_id ) {
    return ( 0 );
  }

  if ( ! p -> exec ) {
    return ( 0 );
  }

  if ( ! targetpath ) {
    return ( 0 );
  }

  if ( ! pndrun ) {
    return ( 0 );
  }

  // set up

  sprintf ( filename, "%s/%s.desktop", targetpath, p -> unique_id );

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

#if 0
  if ( p -> description_en ) {
    snprintf ( buffer, 1020, "Comment=%s\n", p -> description_en );
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

    // basics
    if ( p -> object_type == pnd_object_type_directory ) {
      snprintf ( buffer, 1020, "Exec=%s -p %s -e %s -n", pndrun, p -> object_path, p -> exec );
    } else if ( p -> object_type == pnd_object_type_pnd ) {
      snprintf ( buffer, 1020, "Exec=%s -p %s/%s -e %s -n", pndrun, p -> object_path, p -> object_filename, p -> exec );
    }

    // start dir
    if ( p -> startdir ) {
      strncat ( buffer, " -s ", 1020 );
      strncat ( buffer, p -> startdir, 1020 );
    }

    // newline
    strncat ( buffer, "\n", 1020 );

    // emit
    fprintf ( f, "%s", buffer );
  }

#if 1 // categories
  fprintf ( f, "%s\n", "Categories=Application;Network;" );
#endif

  fprintf ( f, "%s\n", PND_DOTDESKTOP_SOURCE ); // should we need to know 'who' created the file during trimming

  fclose ( f );

  return ( 1 );
}

unsigned char pnd_emit_icon ( char *targetpath, pnd_disco_t *p ) {
  char buffer [ FILENAME_MAX ]; // target filename
  char from [ FILENAME_MAX ];   // source filename
  char bits [ 8 * 1024 ];
  unsigned int bitlen;
  FILE *pnd, *target;

  // prelim .. if a pnd file, and no offset found, discovery code didn't locate icon.. so bail.
  if ( ( p -> object_type == pnd_object_type_pnd ) &&
       ( ! p -> pnd_icon_pos ) )
  {
    return ( 0 ); // discover code didn't find it, so FAIL
  }

  // determine filename for target
  sprintf ( buffer, "%s/%s.png", targetpath, p -> unique_id ); // target

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

    if ( len > (8*1024) ) {
      bitlen = (8*1024);
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
