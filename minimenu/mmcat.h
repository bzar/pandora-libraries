
#ifndef h_mmcat_h
#define h_mmcat_h

typedef struct _mm_appref_t {
  pnd_disco_t *ref;
  // anything else?
  struct _mm_appref_t *next;
} mm_appref_t;

typedef struct {
  char *catname;          // name of the category
  mm_appref_t *refs;      // apps (from g_active_apps) that are in this category
  unsigned int refcount;  // how many apps in this category
} mm_category_t;

#define MAX_CATS 100

#define CATEGORY_ALL "All"

// try to populate as many cats as necessary
unsigned char category_push ( char *catname, pnd_disco_t *app ); // catname is not pulled from app, so we can make them up on the fly (ie: "All")
mm_category_t *category_query ( char *catname );
void category_dump ( void ); // sort the apprefs

#endif
