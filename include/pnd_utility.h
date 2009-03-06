
#ifndef h_pnd_utility_h
#define h_pnd_utility_h

#ifdef __cplusplus
extern "C" {
#endif

// given a malloc'd pointer to a string, expand ~ to $HOME as often as it is found, returning a
// new string; the old string is destroyed in the process, or returned as-is.
char *pnd_expand_tilde ( char *freeable_buffer );

#ifdef __cplusplus
} /* "C" */
#endif

#endif
