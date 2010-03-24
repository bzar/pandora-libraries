
#ifndef h_mmcache_h
#define h_mmcache_h

/* this cache is used, rather than just pointing right from the mm_app_t iconcache, since many apps
 * may re-use the same icon, this lets the apps just cross-link to the same icon to save a bit of
 * memory; probably irrelevent, but what the heck, I'm writing this quick ;)
 */

/* the same structure can be used to contain preview pics, in a different list of same type
 */

typedef struct _mm_cache_t {
  char uniqueid [ 1024 ]; // pnd unique-id
  void /*SDL_Surface*/ *i;
  // structure
  struct _mm_cache_t *next; // next in linked list
} mm_cache_t;

unsigned char cache_icon ( pnd_disco_t *app, unsigned char maxwidth, unsigned char maxheight );
unsigned char cache_preview ( pnd_disco_t *app, unsigned int maxwidth, unsigned int maxheight );

mm_cache_t *cache_query ( char *id, mm_cache_t *head );
mm_cache_t *cache_query_icon ( char *id ); // specialized version
mm_cache_t *cache_query_preview ( char *id ); // specialized version

unsigned char cache_find_writable ( char *r_writepath, unsigned int len );

#endif
