
#ifndef h_mmapps_h
#define h_mmapps_h

typedef struct _mm_app_t {
  char *dispname;  // name to display (already beft-match localized)
  char *exec;      // complete exec-line (ie: bin and all args)
  char *iconpath;  // path to icon
  void *iconcache; // userdata: probably points to an icon cache
  // structure
  struct _mm_app_t *next; // next in linked list
} mm_app_t;

// fullscan implies full searchpath walk and return; no merging new apps with existing list, etc
mm_app_t *apps_fullscan ( char *searchpath );

mm_app_t *apps_fetch_from_dotdesktop ( char *path );

#endif
