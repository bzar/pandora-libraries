
#ifndef h_mmconf_h
#define h_mmconf_h

typedef enum {
  ct_nil,                       // nothing special
  ct_switch_page,               // change to another page
  ct_reset,                     // reset to defaults
  ct_visible_tab_list,          // show list of _visible tabs_
  ct_cpu_speed,                 // show available CPU speeds
  ct_boolean,                   // boolean 1/0 value
  ct_exit,                      // save and quit configuring
  ct_filename,                  // pick a filename
  ct_go_manage_categories,      // go to other menu, manage categories
  ct_go_manage_skins,           // to to other menu, pick skins
  //ct_category_list
} change_type_e;

#define CONF_MAX_LISTLENGTH 2000 /* no more than this number of tabs or apps in the show/hide config pages; perhaps should go multi-page for so many.. */

#define CONF_APPLIST_TAG "*applist*"
#define CONF_TABLIST_TAG "*tablist*"

typedef struct _confitem_t {
  char *text;                   // human readable title
  char *desc;                   // slightly longer description
  char *def;                    // default value (to show user, and for resets); NULL means key is normally undefined

  char *key;                    // key to find this item in the config box
  change_type_e type;           // what happens with this value
  struct _confitem_t *newhead;  // value for type: if specified, will switch to this page (for TOC page)
} confitem_t;

unsigned char conf_run_menu ( confitem_t *toplevel ); // returns >0 for 'request restart'
void conf_display_page ( confitem_t *page, unsigned int selitem, unsigned int first_visible, unsigned int max_visible );
#define CONF_SELECTED 1
#define CONF_UNSELECTED 0
unsigned char conf_prepare_page ( confitem_t *page );
unsigned int conf_determine_pagelength ( confitem_t *page );

void conf_merge_into ( char *fullpath, pnd_conf_handle h ); // merge fullpath as a conf file into 'h'; no turning back.
unsigned char conf_write ( pnd_conf_handle h, char *fullpath ); // emit a conf file, based on items known to conf-ui
#define CONF_PREF_FILENAME ".mmpref.conf" // or should be .mmprefrc? tend to use '.conf' already so go with it.
char *conf_determine_location ( pnd_conf_handle h ); // where to stick the conf file?
void conf_setup_missing ( pnd_conf_handle h ); // find missing keys, set them to default values

void conf_reset_to_default ( pnd_conf_handle h ); // sets keys to their default value (if they have one), all apps to show, all tabs to show

#endif
