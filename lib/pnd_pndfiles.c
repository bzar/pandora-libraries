
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for making ftw.h happy */

#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_apps.h"
#include "pnd_pndfiles.h"

unsigned char pnd_pnd_seek_pxml ( FILE *f ) {
  char *b;
  unsigned int len;

  b = malloc ( PND_PXML_WINDOW_SIZE );

  if ( ! b ) {
    return ( 0 );
  }

  memset ( b, '\0', PND_PXML_WINDOW_SIZE );

  // determine length of file
  fseek ( f, 0, SEEK_END );

  len = ftell ( f );

  fseek ( f, 0, SEEK_SET );

  /* ready to scan through the file, backwards
   */

  //strcasestr ( b, PXML_TAGHEAD );



  // exeunt, with alarums

  free ( b );

  return ( 1 );
}
