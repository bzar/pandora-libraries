
#ifndef h_pnd_desktop_h
#define h_pnd_desktop_h

#ifdef __cplusplus
extern "C" {
#endif


#define PND_PNDHUP_KEY "launcher.hupscript"
#define PND_PNDHUP_FILENAME "pnd_hup.sh"

// emit_dotdesktop() will determine a filename and create a FILENAME.desktop file in the targetpath
// TODO: Copy the icon into this directory as well, if its source is a .pnd or info is in the dico struct
#define PND_DOTDESKTOP_HEADER "[Desktop Entry]"
#define PND_DOTDESKTOP_SOURCE "_Source=libpnd"
unsigned char pnd_emit_dotdesktop ( char *targetpath, char *pndrun, pnd_disco_t *p );

// emit_icon() will attempt to copy the icon from a PXML directory, or from a pnd file if appended,
// to the given directory; returns 1 on sucess, otherwise is a fail.
unsigned char pnd_emit_icon ( char *targetpath, pnd_disco_t *p );


#ifdef __cplusplus
} /* "C" */
#endif

#endif
