
#ifndef h_freedesktop_cats_h
#define h_freedesktop_cats_h

typedef struct {
  char *cat;
  char *parent_cat;
  char *desc;
} freedesktop_cat_t;

extern freedesktop_cat_t freedesktop_complete[];

// return NULL on error, otherwise a category entry
freedesktop_cat_t *freedesktop_category_query ( char *name );

#define BADCATNAME "Other" /* irony: Other is itself not a freedesktop category */

#endif
