
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pnd_conf.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "../lib/pnd_pathiter.h"

#include "mmenu.h"
#include "mmcache.h"
#include "mmcat.h"

mm_category_t _categories [ MAX_CATS ];
mm_category_t *g_categories = _categories;
unsigned char g_categorycount = 0;

mm_category_t _categories_invis [ MAX_CATS ];
unsigned char _categories_inviscount = 0;

mm_catmap_t g_catmaps [ MAX_CATS ];
unsigned char g_catmapcount = 0;

extern pnd_conf_handle g_conf;

unsigned char category_push ( char *catname, pnd_disco_t *app, pnd_conf_handle ovrh, char *fspath ) {
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
    //pnd_log ( PND_LOG_DEFAULT, "New category '%s'\n", catname );
    g_categories [ g_categorycount ].catname = strdup ( catname );
    g_categories [ g_categorycount ].refs = NULL;
    c = &(g_categories [ g_categorycount ]);

    if ( fspath ) {
      g_categories [ g_categorycount ].fspath = strdup ( fspath );;
    }

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
	if ( cat_sort_score ( ar, iter ) < 0 ) {
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

int cat_sort_score ( mm_appref_t *s1, mm_appref_t *s2 ) {

  extern unsigned char ui_category;

  // are we in a directory browser, or looking at pnd-files?
  if ( g_categories [ ui_category ].fspath ) {

    if ( s1 == s2 ) {
      return ( 0 ); // equal

    } else if ( s1 -> ref -> object_type == pnd_object_type_directory &&
		s2 -> ref -> object_type == pnd_object_type_directory )
    {
      // both are directories, be nice
      return ( strcmp ( s1 -> ref -> title_en, s2 -> ref -> title_en ) );
    } else if ( s1 -> ref -> object_type == pnd_object_type_directory &&
		s2 -> ref -> object_type != pnd_object_type_directory )
    {
      return ( -1 ); // dir on the left is earlier than file on the right
    } else if ( s1 -> ref -> object_type != pnd_object_type_directory &&
		s2 -> ref -> object_type == pnd_object_type_directory )
    {
      return ( 1 ); // dir on the right is earlier than file on the left
    } else {
      // file on file
      return ( strcmp ( s1 -> ref -> title_en, s2 -> ref -> title_en ) );
    }

  }

  return ( strcmp ( s1 -> ref -> title_en, s2 -> ref -> title_en ) );
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

static void _category_freeall ( mm_category_t *p, unsigned int c ) {
  unsigned int i;
  mm_appref_t *iter, *next;

  for ( i = 0; i < c; i++ ) {

    iter = p [ i ].refs;

    while ( iter ) {
      next = iter -> next;
      free ( iter );
      iter = next;
    }

    p [ i ].refs = NULL;

    if ( p [ i ].catname ) {
      free ( p [ i ].catname );
      p [ i ].catname = NULL;
    }

    if ( p [ i ].fspath ) {
      free ( p [ i ].fspath );
      p [ i ].fspath = NULL;
    }

  } // for

  return;
}

void category_freeall ( void ) {

  _category_freeall ( g_categories, g_categorycount );
  _category_freeall ( _categories_invis, _categories_inviscount );

  g_categorycount = 0;
  _categories_inviscount = 0;

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

	category_push ( k, NULL, 0, NULL /* fspath */ );
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

unsigned char category_meta_push ( char *catname, char *parentcatname, pnd_disco_t *app, pnd_conf_handle ovrh, unsigned char visiblep ) {
  mm_category_t *cat;
  unsigned char catcount = g_categorycount;
  char catnamebuffer [ 512 ] = "";

  if ( ! catname ) {
    return ( 1 ); // fine, just nada
  }

  if ( ! visiblep ) {
    //return ( 1 ); // fine, suppress it

    // serious evidence this was a rushed program
    g_categories = _categories_invis;
    g_categorycount = _categories_inviscount;

    // if invisible, and a parent category name is known, prepend it for ease of use
    if ( parentcatname ) {
      snprintf ( catnamebuffer, 500, "%s.%s", parentcatname, catname );
      catname = catnamebuffer;
    }

  }

  // do we honour cat mapping at all?
  if ( pnd_conf_get_as_int_d ( g_conf, "categories.map_on", 0 ) ) {

    // is this guy mapped?
    cat = category_map_query ( catname );

    if ( cat ) {
      category_push ( cat -> catname, app, ovrh, NULL /* fspath */ );
      goto visibility_hack_cleanup;
    }

    // not mapped.. but default?
    if ( pnd_conf_get_as_int_d ( g_conf, "categories.map_default_on", 0 ) ) {
      char *def = pnd_conf_get_as_char ( g_conf, "categories.map_default_cat" );
      if ( def ) {
	category_push ( def, app, ovrh, NULL /* fspath */ );
	goto visibility_hack_cleanup;
      }
    }

  } // cat map is desired?

  // not default, just do it
  category_push ( catname, app, ovrh, NULL /* fspath */ );
  // hack :(
 visibility_hack_cleanup:
  if ( ! visiblep ) {
    _categories_inviscount = g_categorycount;
    g_categories = _categories;
    g_categorycount = catcount;
  }
  return ( 1 );
}

unsigned char category_fs_restock ( mm_category_t *cat ) {

  if ( ! cat -> fspath ) {
    return ( 1 ); // not a filesystem browser tab
  }

  // clear any existing baggage
  //

  // apprefs
  mm_appref_t *iter = cat -> refs, *next;
  while ( iter ) {
    next = iter -> next;
    free ( iter );
    iter = next;
  }
  cat -> refs = NULL;

  // discos
  if ( cat -> disco ) {
    pnd_disco_t *p = pnd_box_get_head ( cat -> disco );
    pnd_disco_t *n;
    while ( p ) {
      n = pnd_box_get_next ( p );
      pnd_disco_destroy ( p );
      p = n;
    }
    pnd_box_delete ( cat -> disco );
  }

  // rescan the filesystem
  //

  //pnd_log ( pndn_debug, "Restocking cat %s with path %s\n", cat -> catname, cat -> fspath );
  DIR *d;

  if ( ( d = opendir ( cat -> fspath ) ) ) {
    struct dirent *de = readdir ( d );

    pnd_disco_t *disco;
    char uid [ 100 ];

    cat -> disco = pnd_box_new ( cat -> catname );

    while ( de ) {

      struct stat buffy;
      char fullpath [ PATH_MAX ];
      sprintf ( fullpath, "%s/%s", cat -> fspath, de -> d_name );
      int statret = stat ( fullpath, &buffy );

      // if file is executable somehow or another
      if ( statret == 0 &&
	   buffy.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)
	 )
      {
	// determine unique-id
	sprintf ( uid, "%d", (int) de -> d_ino );
	disco = NULL;

	switch ( de -> d_type ) {

	case DT_DIR:
	  if ( strcmp ( de -> d_name, "." ) == 0 ) {
	    // ignore ".", but ".." is fine
	  } else {
	    disco = pnd_box_allocinsert ( cat -> disco, uid, sizeof(pnd_disco_t) );
	    disco -> object_type = pnd_object_type_directory; // suggest to Grid that its a dir
	  }
	  break;
	case DT_UNKNOWN:
	case DT_REG:
	  disco = pnd_box_allocinsert ( cat -> disco, uid, sizeof(pnd_disco_t) );
	  disco -> object_type = pnd_object_type_unknown; // suggest to Grid that its a file
	  break;

	} // switch

	// found a directory or executable?
	if ( disco ) {
	  // register with this category
	  disco -> unique_id = strdup ( uid );
	  disco -> title_en = strdup ( de -> d_name );
	  disco -> object_flags = PND_DISCO_GENERATED;
	  disco -> object_path = strdup ( cat -> fspath );
	  disco -> object_filename = strdup ( de -> d_name );
	  category_push ( cat -> catname, disco, 0, NULL /* fspath already set */ );
	  // if a override icon exists, cache it up
	  cache_icon ( disco, pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_width", 50 ),
		       pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_height", 50 ) );
	}

      } // stat

      // next
      de = readdir ( d );
    }

    closedir ( d );
  }

  return ( 1 );
}

static int catname_cmp ( const void *p1, const void *p2 ) {
  mm_category_t *c1 = (mm_category_t*) p1;
  mm_category_t *c2 = (mm_category_t*) p2;
  return ( strcasecmp ( c1 -> catname, c2 -> catname ) );
}

void category_sort ( void ) {
  // we probably don't want to sort tab categories, since the user may have specified an ordering
  // But we can sort invisi-cats, to make them easier to find, and ordered by parent category

  qsort ( _categories_invis, _categories_inviscount, sizeof(mm_category_t), catname_cmp );

  return;
}
