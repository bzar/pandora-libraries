
#ifndef h_mmcat_h
#define h_mmcat_h

/* code for storing/retrieving/messing with 'categories' (ie: tabs, more or less)
 */

// an appreg is a minimenu registered app, that in turn points to the libpnd disco-t entry, but also stores anything
// minimenu specific about it.
typedef struct _mm_appref_t {
  pnd_disco_t *ref;
  pnd_conf_handle ovrh;
  // anything else?
  struct _mm_appref_t *next;
} mm_appref_t;

// an actual category reference; points to a list of appref's and supplies a tabname, a filesystem mountpoint, etc.
typedef struct _mm_category_t {
  char *catname;          // name of the category
  char *parent_catname;   // if known, parent cat name
  unsigned int catflags;  // flags

  // current applications
  mm_appref_t *refs;      // apps (from g_active_apps) that are in this category
  unsigned int refcount;  // how many apps in this category

  // if a directory browser category, additional info is needed
  char *fspath;           // NULL if a pnd-category (not a filesystem category)
  pnd_box_handle disco;   // faux-applications generated from filesystem (so that refs can point to here)

} mm_category_t;

// the flags available for the above mm_category_t.catflags
#define CFNORMAL 1 // catflag
#define CFHIDDEN 2 // catflag: 1, 2, 4,...
#define CFSUBCAT 4 // catflag

// how many cats can we handle?
#define MAX_CATS 250

// what do we call the "All" tab, if present
#define CATEGORY_ALL "All"

// try to populate as many cats as necessary
void category_init ( void ); // set up; call first!
unsigned char category_push ( char *catname, char *parentcatname, pnd_disco_t *app, pnd_conf_handle ovrh, char *fspath, unsigned char visiblep ); // catname is not pulled from app, so we can make them up on the fly (ie: "All");
void category_dump ( void ); // sort the apprefs
void category_freeall ( void );
int cat_sort_score ( mm_category_t *cat, mm_appref_t *s1, mm_appref_t *s2 ); // like strcmp, but used to sort apps by title
void category_sort ( void ); // for sorting categories

// category mapping hack
typedef struct {
  mm_category_t *target;  // mapping a category _to_ this other category
  char *from;             // from this category
} mm_catmap_t;

unsigned char category_map_setup ( void ); // set up the mappings
mm_category_t *category_map_query ( char *cat );
unsigned char category_meta_push ( char *catname, char *parentcatname, pnd_disco_t *app, pnd_conf_handle ovrh, unsigned char visiblep, unsigned char parentp );

// filesystem browser
unsigned char category_fs_restock ( mm_category_t *cat );

// apps within cats
unsigned char category_contains_app ( char *catname, char *unique_id );

// advertising to rest of the system
//
extern mm_category_t *g_categories [ MAX_CATS ];
extern unsigned char g_categorycount;
#define CFALL    0xFF // filter mask
#define CFBYNAME 0xFE // filter mask; the name param is used to pick a single category to populate
void category_publish ( unsigned int filter_mask, char *param ); // populates g_categories and g_categorycount; pass in CFNORMAL or CFHIDDEN or CFALL or whatever
unsigned int category_count ( unsigned int filter_mask );
int category_index ( char *catname ); // figure out index in g_categories of named cat

#endif
