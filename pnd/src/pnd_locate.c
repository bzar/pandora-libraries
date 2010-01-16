
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h> /* for making ftw.h happy */

#include <sys/types.h> /* for stat(2)*/
#include <sys/stat.h>
#include <unistd.h>

#include "pnd_pathiter.h"
#include "pnd_locate.h"

static char pnd_locate_buffer[FILENAME_MAX]; // exceedingly lame

char *pnd_locate_filename(char *searchpath, char *filename)
{
    struct stat foo;

    SEARCHPATH_PRE
    {

        strncat(buffer, "/", FILENAME_MAX);
        strncat(buffer, filename, FILENAME_MAX);

        //printf("foo: %s\n", buffer);

        if (stat(buffer, &foo) == 0)
        {
            strcpy(pnd_locate_buffer, buffer);
            return(pnd_locate_buffer);
        }

    }
    SEARCHPATH_POST

    return(NULL);
}
