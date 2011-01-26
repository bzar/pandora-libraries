
#ifndef h_mmui_h
#define h_mmui_h

/* this code actually _does_ something; this way, at least all the IO routines are in one place, so
 * I know what to replace with something sensible later :)
 * ... ahh, to have time to make this in C++ as an actual abstract interface...
 */

/* staticly cached stuff, for UI
 */

typedef enum {
  IMG_BACKGROUND_800480 = 0,
  IMG_BACKGROUND_TABMASK,
  IMG_DETAIL_PANEL,
  IMG_DETAIL_BG,
  IMG_SELECTED_ALPHAMASK,
  IMG_TAB_SEL,
  IMG_TAB_SEL_L,
  IMG_TAB_SEL_R,
  IMG_TAB_UNSEL,
  IMG_TAB_UNSEL_L,
  IMG_TAB_UNSEL_R,
  IMG_TAB_LINE,
  IMG_TAB_LINEL,
  IMG_TAB_LINER,
  IMG_ICON_MISSING,
  IMG_SELECTED_HILITE,
  IMG_PREVIEW_MISSING,
  IMG_ARROW_UP,
  IMG_ARROW_DOWN,
  IMG_ARROW_SCROLLBAR,
  IMG_HOURGLASS,
  IMG_FOLDER,
  IMG_EXECBIN,
  IMG_MAX, // before this point is loaded; after is generated
  IMG_TRUEMAX
} mm_imgcache_e;

typedef struct {
  mm_imgcache_e id;
  char *confname;
  void /*SDL_Surface*/ *i;
} mm_imgcache_t;

/* ui stuff
 */

typedef enum {
  uisb_none = 0,
  uisb_x = 1,
  uisb_y = (1<<1),
  uisb_a = (1<<2),
  uisb_b = (1<<3),
  uisb_l = (1<<4),
  uisb_r = (1<<5),
  uisb_start = (1<<6),
  uisb_select = (1<<7),
  uisb_max
} ui_sdl_button_e;

unsigned char ui_setup ( void );
unsigned char ui_imagecache ( char *basepath );
unsigned char ui_pick_skin ( void ); // return >0 if skin has changed

void ui_render ( void );

void ui_loadscreen ( void );        // show screen while loading the menu
void ui_discoverscreen ( unsigned char clearscreen ); // screen to show while scanning for apps
void ui_cachescreen ( unsigned char clearscreen, char *filename ); // while caching icons, categories and preview-pics-Now-mode
void ui_show_hourglass ( unsigned char updaterect );
void ui_post_scan ( void );
unsigned char ui_show_info ( char *pndrun, pnd_disco_t *p );
void ui_aboutscreen ( char *textpath );
void ui_revealscreen ( void );

/* internal functions follow
 */

// show a menu, return when selection made; -1 means no selection. Enter is pick.
int ui_modal_single_menu ( char *argv[], unsigned int argc, char *title, char *footer );

// run a forked app (ie: not wait for it to return)
unsigned char ui_forkexec ( char *argv[] ); // argv[0] is proggy to exec; argv last entry must be NULLptr

// create a thread of this guy, and it'll try to load the preview pic in background and then signal the app
unsigned char ui_threaded_defered_preview ( pnd_disco_t *p );
unsigned char ui_threaded_defered_icon ( void * );

// change the focus
void ui_process_input ( unsigned char block_p );
void ui_push_left ( unsigned char forcecoil );
void ui_push_right ( unsigned char forcecoil );
void ui_push_up ( void );
void ui_push_down ( void );
void ui_push_exec ( void );
void ui_push_backup ( void );
void ui_push_ltrigger ( void );
void ui_push_rtrigger ( void );
unsigned char ui_determine_row ( mm_appref_t *a );
unsigned char ui_determine_screen_row ( mm_appref_t *a );
unsigned char ui_determine_screen_col ( mm_appref_t *a );

// detail panel hide/show
unsigned char ui_is_detail_hideable ( void ); // returns true if detail pane may be hidden with current skin
void ui_toggle_detail_pane ( void );          // toggle it on/off

// ui_render() can register tappable-areas which touchscreen code can then figure out if we made a hit
void ui_register_reset ( void );
void ui_register_tab ( unsigned char catnum, unsigned int x, unsigned int y, unsigned int w, unsigned int h );
void ui_register_app ( mm_appref_t *app, unsigned int x, unsigned int y, unsigned int w, unsigned int h );
void ui_touch_act ( unsigned int x, unsigned int y );

// deferred preview timer
void ui_set_selected ( mm_appref_t *r );
unsigned int ui_callback_f ( unsigned int t );

#endif
