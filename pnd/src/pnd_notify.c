
#include <sys/inotify.h>    // for INOTIFY duh
#include <stdio.h>          // for stdio, NULL
#include <stdlib.h>         // for malloc, etc
#include <unistd.h>         // for close
#include <time.h>           // for time()

#define _XOPEN_SOURCE 500
#define __USE_XOPEN_EXTENDED
#include <ftw.h> /* for nftw, tree walker */

#include "pnd_notify.h"
#include "pnd_pathiter.h"

typedef struct
{
    int fd;              // notify API file descriptor
} pnd_notify_t;

static int notify_handle;

//static void pnd_notify_hookup(int fd);

#define PND_INOTIFY_MASK     IN_CREATE | IN_DELETE | IN_UNMOUNT \
    | IN_DELETE_SELF | IN_MOVE_SELF    \
    | IN_MOVED_FROM | IN_MOVED_TO

pnd_notify_handle pnd_notify_init(void)
{
    int fd;
    pnd_notify_t *p;

    fd = inotify_init();

    if (fd < 0)
    {
        return(NULL);
    }

    p = malloc(sizeof(pnd_notify_t));

    if (! p)
    {
        close(fd);
        return(NULL); // uhh..
    }

    p -> fd = fd;

    // setup some default watches
    //pnd_notify_hookup(fd);

    return(p);
}

void pnd_notify_shutdown(pnd_notify_handle h)
{
    pnd_notify_t *p = (pnd_notify_t*)h;

    close(p -> fd);

    free(p);

    return;
}

static int pnd_notify_callback(const char *fpath, const struct stat *sb __attribute__((unused)),
                               int typeflag, struct FTW *ftwbuf __attribute__((unused)))
{

    // only include directories
    if (!(typeflag & FTW_D))
    {
        return 0; // continue the tree walk
    }

    //printf("Implicitly watching dir '%s'\n", fpath);

    inotify_add_watch(notify_handle, fpath, PND_INOTIFY_MASK);

    return 0; // continue the tree walk
}

void pnd_notify_watch_path(pnd_notify_handle h, char *fullpath, unsigned int flags)
{
    pnd_notify_t *p = (pnd_notify_t*)h;

#if 1
    inotify_add_watch(p -> fd, fullpath, PND_INOTIFY_MASK);
#else
    inotify_add_watch(p -> fd, fullpath, IN_ALL_EVENTS);
#endif

    if (flags & PND_NOTIFY_RECURSE)
    {

        notify_handle = p -> fd;

        nftw(fullpath,             // path to descend
             pnd_notify_callback,  // callback to do processing
             10,                   // no more than X open fd's at once
             FTW_PHYS);           // do not follow symlinks

        notify_handle = -1;

    } // recurse

    return;
}

#if 0
static void pnd_notify_hookup(int fd)
{

    inotify_add_watch(fd, "./testdata", IN_CREATE | IN_DELETE | IN_UNMOUNT);
    inotify_add_watch(fd, "/media", IN_CREATE | IN_DELETE | IN_UNMOUNT);

    return;
}
#endif

unsigned char pnd_notify_rediscover_p(pnd_notify_handle h)
{
    pnd_notify_t *p = (pnd_notify_t*)h;

    struct timeval t;
    fd_set rfds;
    int retcode;

    // don't block for long..
    //t.tv_sec = 1;
    //t.tv_usec = 0; //5000;
    t.tv_sec = 0;
    t.tv_usec = 5000;

    // only for our useful fd
    FD_ZERO(&rfds);
    FD_SET((p->fd), &rfds);

    // wait and test
    retcode = select((p->fd) + 1, &rfds, NULL, NULL, &t);

    if (retcode < 0)
    {
        return 0; // hmm.. need a better error code handler
    }
    else if (retcode == 0)
    {
        return 0; // timeout
    }

    if (! FD_ISSET((p->fd), &rfds))
    {
        return 0;
    }

    // by this point, something must have happened on our watch
#define BINBUFLEN (100 *(sizeof(struct inotify_event)+ 30))/* approx 100 events in a shot? */
    unsigned char binbuf[BINBUFLEN];
    int actuallen;

    actuallen = read((p->fd), binbuf, BINBUFLEN);

    if (actuallen < 0)
    {
        return 0; // error
    }
    else if (actuallen == 0)
    {
        return 0; // nothing, or overflow, or .. whatever.
    }

    unsigned int i;
    struct inotify_event *e;

    while ((int)i < actuallen)
    {
        e = (struct inotify_event *) & binbuf[i];

        /* do it!
         */

        if (e -> len)
        {
            //printf("Got event against '%s'\n", e -> name);
        }

        /* do it!
         */

        // next
        i += (sizeof(struct inotify_event) + e -> len);
    } // while

    return 1;
}

#define _INOTIFY_TEST_PATH "/tmp/.notifytest"
#define _INOTIFY_TEST_FILE "/tmp/.notifytest/foopanda"
static unsigned char _inotify_test_run(void)
{

    // set up inotify
    pnd_notify_t fdt;
    int wd; // watch-descriptor

    // set up inotify
    fdt.fd = inotify_init();

    if (fdt.fd < 0)
    {
        return 0; // failed to init at all
    }

    // make a temp dir
    mkdir(_INOTIFY_TEST_PATH, 0777); // if it fails we assume it exists, which is fine

    // watch the dir
    if ((wd = inotify_add_watch(fdt.fd, _INOTIFY_TEST_PATH, IN_DELETE)) < 0)
    {
        return 0; // couldn't watch dir
    }

    // sleep a sec, just to be safe; seems to dislike being called immediately sometimes
    usleep(1000000 / 2);

    // create a temp file
    FILE *f;

    if (!(f = fopen(_INOTIFY_TEST_FILE, "w")))
    {
        close(fdt.fd);
        return 0; // couldn't create test file?!
    }

    fclose(f);

    // delete the temp file; this should trigger an event
    if (unlink(_INOTIFY_TEST_FILE) < 0)
    {
        return 0; // couldn't rm a file? life is harsh.
    }

    // did we get anything?
    unsigned char s;
    s = pnd_notify_rediscover_p(&fdt);

    // ditch inotify
    close(fdt.fd);

    // success
    return(s);
}

/* we've run into the issue where inotify returns that it is set up, but in
 * fact is not doing anything; restarting the process repairs it.. so here
 * we devise a wank that continually tests inotify until it responds, then
 * returns knowing we're good
 */
unsigned char pnd_notify_wait_until_ready(unsigned int secs_timeout)
{
    time_t start = time(NULL);

    while ((secs_timeout == 0) ||
            (time(NULL) - start < secs_timeout))
    {
        if (_inotify_test_run())
        {
            return 1;
        }

        usleep(1000000 / 2);
    }

    return 0; // fail
}
