
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "pnd_conf.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "../lib/pnd_pathiter.h"

#include "mmenu.h"
#include "mmcache.h"
#include "mmcat.h"

mm_category_t g_categories [ MAX_CATS ];
unsigned char g_categorycount = 0;

mm_catmap_t g_catmaps [ MAX_CATS ];
unsigned char g_catmapcount = 0;

extern pnd_conf_handle g_conf;

unsigned char category_push ( char *catname, pnd_disco_t *app, pnd_conf_handle ovrh ) {
  mm_category_t *c;

  // check category list; if found, append app to the end of it.
  // if not found, add it to the category list and plop the app in there.
  // app's are just app-refs, which contain links to the disco-t list -- thus removal is only in one place, and
  // an app can be in multiple categories if we like..
  //

  // find or create category
  //

  if ( ( c = category_query ( catname ) ) ) {
    // category was found..
  } else {
    // category wasn't found..
    pnd_log ( PND_LOG_DEFAULT, "New category '%s'\n", catname );
    g_categories [ g_categorycount ].catname = strdup ( catname );
    g_categories [ g_categorycount ].refs = NULL;
    c = &(g_categories [ g_categorycount ]);
    g_categorycount++;
  }

  if ( ! app ) {
    return ( 1 ); // create cat, but skip app
  }

  // alloc and populate appref
  //
  mm_appref_t *ar = malloc ( sizeof(mm_appref_t) );
  if ( ! ar ) {
    return ( 0 );
  }

  bzero ( ar, sizeof(mm_appref_t) );

  ar -> ref = app;
  ar -> ovrh = ovrh;

  // plug it into category
  //   and sort it on insert!
#if 0 // no sorting
  ar -> next = c -> refs;
  c -> refs = ar;
#else // with sorting
  // if no refs at all, or new guy has no title, just stick it in at head
  if ( c -> refs && ar -> ref -> title_en ) {
    mm_appref_t *iter = c -> refs;
    mm_appref_t *last = NULL;

    while ( iter ) {

      if ( iter -> ref -> title_en ) {
	if ( strcmp ( ar -> ref -> title_en, iter -> ref -> title_en ) < 0 ) {
	  // new guy is smaller than the current guy!
	  break;
	}
      } else {
	// since new guy must have a name by here, we're bigger than any guy who does not have a name
	// --> continue
      }

      last = iter;
      iter = iter -> next;
    }

    if ( iter ) {
      // smaller than the current guy, so stitch in
      if ( last ) {
	ar -> next = iter;
	last -> next = ar;
      } else {
	ar -> next = c -> refs;
	c -> refs = ar;
      }
    } else {
      // we're the biggest, just append to last
      last -> next = ar;
    }

  } else {
    ar -> next = c -> refs;
    c -> refs = ar;
  }
#endif
  c -> refcount++;

  return ( 1 );
}

mm_category_t *category_query ( char *catname ) {
  unsigned char i;

  for ( i = 0; i < g_categorycount; i++ ) {

    if ( strcasecmp ( g_categories [ i ].catname, catname ) == 0 ) {
      return ( &(g_categories [ i ]) );
    }

  }

  return ( NULL );
}

void category_dump ( void ) {
  unsigned int i;

  // WHY AREN'T I SORTING ON INSERT?

  // dump
  for ( i = 0; i < g_categorycount; i++ ) {
    pnd_log ( PND_LOG_DEFAULT, "Category %u: '%s' * %u\n", i, g_categories [ i ].catname, g_categories [ i ].refcount );
    mm_appref_t *ar = g_categories [ i ].refs;

    while ( ar ) {
      pnd_log ( PND_LOG_DEFAULT, "  Appref %s\n", IFNULL(ar -> ref -> title_en,"No Name") );
      ar = ar -> next;
    }

  } // for

  return;
}

void category_freeall ( void ) {
  unsigned int i;
  mm_appref_t *iter, *next;

  for ( i = 0; i < g_categorycount; i++ ) {

    iter = g_categories [ i ].refs;

    while ( iter ) {
      next = iter -> next;
      free ( iter );
      iter = next;
    }

    g_categories [ i ].refs = NULL;

  } // for

  g_categorycount = 0;

  return;
}

unsigned char category_map_setup ( void ) {

  char *searchpath = pnd_box_get_head ( g_conf );

  if ( ! searchpath ) {
    return ( 0 );
  }

  // look through conf for useful keys
  while ( searchpath ) {
    char *k = pnd_box_get_key ( searchpath );

    // does this key look like a category mapping key?
    if ( strncasecmp ( k, "categories.@", 12 ) == 0 ) {
      k += 12;

      // iterate across 'words' in v, assigning catmaps to them
      SEARCHCHUNK_PRE
      {
	//pnd_log ( pndn_debug, "target(%s) from(%s)\n", k, buffer );

	category_push ( k, NULL, 0 );
	g_catmaps [ g_catmapcount ].target = category_query ( k );
	g_catmaps [ g_catmapcount ].from = strdup ( buffer );
	g_catmapcount++;

      }
      SEARCHCHUNK_POST

    } // if key looks like catmap

    searchpath = pnd_box_get_next ( searchpath );
  } // while each conf key

  return ( 1 );
}

mm_category_t *category_map_query ( char *cat ) {
  unsigned char i;

  for ( i = 0; i < g_catmapcount; i++ ) {
    if ( strcasecmp ( g_catmaps [ i ].from, cat ) == 0 ) {
      return ( g_catmaps [ i ].target );
    }
  }

  return ( NULL );
}

unsigned char category_meta_push ( char *catname, pnd_disco_t *app, pnd_conf_handle ovrh ) {
  mm_category_t *cat;

  // do we honour cat mapping at all?
  if ( pnd_conf_get_as_int_d ( g_conf, "categories.map_on", 0 ) ) {

    // is this guy mapped?
    cat = category_map_query ( catname );

    if ( cat ) {
      return ( category_push ( cat -> catname, app, ovrh ) );
    }

    // not mapped.. but default?
    if ( pnd_conf_get_as_int_d ( g_conf, "categories.map_default_on", 0 ) ) {
      char *def = pnd_conf_get_as_char ( g_conf, "categories.map_default_cat" );
      if ( def ) {
	return ( category_push ( def, app, ovrh ) );
      }
    }

  } // cat map is desired?

  // not default, just do it
  return ( category_push ( catname, app, ovrh ) );
}
