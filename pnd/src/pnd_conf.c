
#include <stdio.h> /* for NULL, printf, FILE, etc */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for strdup */
#include <ctype.h> /* for isspace */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_pathiter.h"

pnd_conf_filename_t pnd_conf_filenames[] = {
  { pnd_conf_conf,         PND_CONF_FILE },
  { pnd_conf_apps,         "apps" },
  { pnd_conf_startup,      "startup" },
  { pnd_conf_desktop,      "desktop" },
  { pnd_conf_categories,   "categories" },
  { pnd_conf_evmap,        "eventmap" },
  { pnd_conf_nil,          NULL },
};

char *pnd_conf_query_searchpath ( void ) {
  pnd_conf_handle ch;

  // fetch base config
  ch = pnd_conf_fetch_by_id ( pnd_conf_conf, PND_CONF_SEARCHPATH );

  if ( ! ch ) {
    //printf ( "Couldn't locate base conf file '%s'\n", PND_CONF_FILE );
    return ( strdup ( PND_CONF_SEARCHPATH ) );
  }

  // can we find a user-specified config path? if so, use it.. if not, fall back!
  char *searchpath;
  char *temp;

  temp = pnd_conf_get_as_char ( ch, PND_CONF_KEY );

  if ( searchpath ) {
    searchpath = strdup ( temp );
  } else {
    searchpath = strdup ( PND_CONF_SEARCHPATH );
  }

  return ( searchpath );
}

pnd_conf_handle pnd_conf_fetch_by_id ( pnd_conf_filename_e id, char *searchpath ) {
  pnd_conf_filename_t *p = pnd_conf_filenames;

  while ( p -> filename ) {

    /* found the filename associated to the id? */
    if ( p -> id == id ) {
      return ( pnd_conf_fetch_by_name ( p -> filename, searchpath ) );
    }

    /* next! */
    p++;
  } /* while */

  return ( NULL );
}

pnd_conf_handle pnd_conf_fetch_by_name ( char *filename, char *searchpath ) {

  /* the fun part here is that we get to cheat; while we have to search through all the directories
   * listed in the search path, we can stop at the first matching file. Nothign really fancy going on, and
   * no need for comprehensive directory crawling. yay!
   */
  pnd_conf_handle conf;

  //printf ( "Search path: '%s'\n", searchpath );

  SEARCHPATH_PRE
  {

    strncat ( buffer, "/", FILENAME_MAX - 1 );
    strncat ( buffer, filename, FILENAME_MAX - 1 );
    conf = pnd_conf_fetch_by_path ( buffer );

    if ( conf ) {
      return ( conf );
    }

  }
  SEARCHPATH_POST

  return ( NULL );
}

pnd_conf_handle pnd_conf_fetch_by_path ( char *fullpath ) {
  FILE *f;
  char section [ 256 ] = "";
  char buffer [ FILENAME_MAX ];
  char inbuffer [ FILENAME_MAX ];
  char *c, *head, *tail, *mid;

  //printf ( "Attempt to load config from fullpath '%s'\n", fullpath );

  /* WARN:
   * No check yet to verify the directory is actually mounted and readable; either way
   * this should not block or take up much time, though SD cards might be slow to open over
   * and over again .. perhaps need some smarts or caching of results or somesuch, since this
   * call gets spammed over and over...
   */
  f = fopen ( fullpath, "r" );

  if ( ! f ) {
    return ( NULL );
  }

  // damn, we actually found a file, so need to try to parse it. Shucks. Give back a box-handle
  // so the consumer has some lists to look at
  pnd_box_handle h;
  h = pnd_box_new ( fullpath );

  // inhale file
  while ( fgets ( inbuffer, FILENAME_MAX, f ) ) {

    // strip line-endings and DOSisms
    if ( ( c = strchr ( inbuffer, '\r' ) ) ) {
      *c = '\0';
    }

    if ( ( c = strchr ( inbuffer, '\n' ) ) ) {
      *c = '\0';
    }

    //printf ( "config line: '%s'\n", inbuffer );

    // strip comments
    if ( ( c = strchr ( inbuffer, '#' ) ) ) {
      *c = '\0';
    }

    // strip leading and trailing spaces
    head = inbuffer;
    while ( *head && isspace ( *head ) ) {
      head++;
    }

    if ( inbuffer [ 0 ] == '\0' ) {
      //printf ( "  -> discard\n" );
      continue; // skip, the line was pure comment or blank
    }

    tail = strchr ( inbuffer, '\0' ) - 1;
    while ( *tail && isspace ( *tail ) ) {
      *tail = '\0';
      tail--;
    }

    if ( inbuffer [ 0 ] == '\0' ) {
      //printf ( "  -> discard\n" );
      continue; // skip, the line was pure comment or blank
    }

    // decorated, ie: a section?
    if ( *head == '[' && *tail == ']' ) {
      // note: handle the nil-section

      memset ( section, '\0', 256 );

      if ( tail == head + 1 ) {
        section [ 0 ] = '\0';
      } else {
        strncpy ( section, head + 1, tail - head - 1 );
      }

      //printf ( " -> section '%s'\n", section );

    } else {

      // must be a key (and likely a value) .. find the division
      mid = head;
      while ( *mid && ! isspace ( *mid ) ) {
        mid++;
      }
      *mid = '\0';
      mid++;

      //printf ( "key head: '%s'\n", head );
      //printf ( "key mid: '%s'\n", mid );

      // is thjis a key/value pair, or just a key?
      if ( mid [ 0 ] ) {
        // key/value pairing
        char *v;

        // form the actual new key
        if ( section [ 0 ] ) {
          snprintf ( buffer, FILENAME_MAX - 1, "%s.%s", section, head );
        } else {
          strncpy ( buffer, head, FILENAME_MAX - 1 );
        }

        //printf ( "Found key '%s' in config file\n", buffer );

        // alloc node into the box
        v = pnd_box_allocinsert ( h, buffer, strlen ( mid ) + 1 ); // allow for trailing null

        if ( v ) {
          strcpy ( v, mid );
        } else {
          return ( NULL ); // OOM while reading conf is either sad, or really scary conf (also sad.)
        }

      } else {
        // key/value pairing
        char *v;

        // form the actual new key
        if ( section [ 0 ] ) {
          snprintf ( buffer, FILENAME_MAX - 1, "%s.%s", section, head );
        } else {
          strncpy ( buffer, head, FILENAME_MAX - 1 );
        }

        //printf ( "Found key with no value '%s' in config file\n", buffer );

        // alloc node into the box
        v = pnd_box_allocinsert ( h, buffer, 0 ); // zero b/c of no payload

        if ( ! v ) {
          return ( NULL ); // OOM while reading conf is either sad, or really scary conf (also sad.)
        }

      } // key or key/value?

    } // section or key/value line?

  } // while

  // clean up a trifle
  fclose ( f );

  return ( h );
}

char *pnd_conf_get_as_char ( pnd_conf_handle c, char *key ) {
  return ( pnd_box_find_by_key ( c, key ) );
}

int pnd_conf_get_as_int ( pnd_conf_handle c, char *key ) {
  char *t = pnd_box_find_by_key ( c, key );

  if ( ! t ) {
    return ( PND_CONF_BADNUM ); // non-existant
  }

  int i = atoi ( t );

  return ( i );
}
