
#ifndef h_mmcustom_cats_h
#define h_mmcustom_cats_h

#define MMCUSTOM_CATS_MAX 255
#define MMCUSTOM_CATS_PREF_FILENAME ".mmcats.conf"

#define MMCUSTOM_CATS_SECTION "custom_categories"

#define MMCUSTOM_CATS_NOCAT "*parent*"

typedef struct {
  char *cat;
  char *parent_cat;
} mmcustom_cat_t;

extern mmcustom_cat_t mmcustom_complete[];
extern unsigned int mmcustom_count;

unsigned char mmcustom_setup ( void ); // load
void mmcustom_shutdown ( void );       // unload
unsigned char mmcustom_is_ready ( void );

unsigned char mmcustom_write ( char *fullpath /* if NULL, uses canonical location */ );   // save
char *mmcustom_determine_path ( void );

mmcustom_cat_t *mmcustom_query ( char *catname, char *parentcatname ); // parentcatname NULL for parents
unsigned int mmcustom_subcount ( char *parentcatname ); // how many custom subcats of the named cat (FD or custom)

mmcustom_cat_t *mmcustom_register ( char *catname, char *parentcatname );
void mmcustom_unregister ( char *catname, char *parentcatname );

unsigned int mmcustom_count_subcats ( char *catname );

#endif
