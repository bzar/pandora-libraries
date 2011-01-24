
#ifndef h_mmcat_h
#define h_mmcat_h

typedef struct _mm_appref_t {
  pnd_disco_t *ref;
  pnd_conf_handle ovrh;
  // anything else?
  struct _mm_appref_t *next;
} mm_appref_t;

typedef struct {
  char *catname;          // name of the category
  // current applications
  mm_appref_t *refs;      // apps (from g_active_apps) that are in this category
  unsigned int refcount;  // how many apps in this category
  // if a directory browser category, additional info is needed
  char *fspath;           // NULL if a pnd-category (not a filesystem category)
  pnd_box_handle disco;   // faux-applications generated from filesystem (so that refs can point to here)
} mm_category_t;

#define MAX_CATS 200

#define CATEGORY_ALL "All"

// try to populate as many cats as necessary
unsigned char category_push ( char *catname, pnd_disco_t *app, pnd_conf_handle ovrh, char *fspath ); // catname is not pulled from app, so we can make them up on the fly (ie: "All");
mm_category_t *category_query ( char *catname );
void category_dump ( void ); // sort the apprefs
void category_freeall ( void );
int cat_sort_score ( mm_appref_t *s1, mm_appref_t *s2 ); // like strcmp, but used to sort apps by title
void category_sort ( void ); // for sorting categories

// category mapping hack
typedef struct {
  mm_category_t *target;  // mapping a category _to_ this other category
  char *from;             // from this category
} mm_catmap_t;

unsigned char category_map_setup ( void ); // set up the mappings
mm_category_t *category_map_query ( char *cat );
unsigned char category_meta_push ( char *catname, char *parentcatname, pnd_disco_t *app, pnd_conf_handle ovrh, unsigned char visiblep );

// filesystem browser
unsigned char category_fs_restock ( mm_category_t *cat );

#endif
