#include "pnd_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

unsigned char pnd_device_open_write_close(char *name, char *buffer)
{
    int f;

    if ((f = open(name, O_WRONLY)) < 0)
        return 0;

    if (write(f, buffer, strlen(buffer)) < (int)strlen(buffer))
        return 0;

    close(f);
    return 1;
}

unsigned char pnd_device_open_read_close(char *name, char *r_buffer, unsigned int buffer_len)
{
    FILE *f = fopen(name, "r");

    if (!f)
        return 0;

    if (!fgets(r_buffer, buffer_len, f))
    {
        fclose(f);
        return 0;
    }

    fclose(f);

    return 1;
}

unsigned char pnd_device_set_clock(unsigned int c)
{
    char buffer[100];
    sprintf(buffer, "%u", c);
    return pnd_device_open_write_close(PND_DEVICE_PROC_CLOCK, buffer);
}

unsigned int pnd_device_get_clock(void)
{
    char buffer[100];

    if (pnd_device_open_read_close(PND_DEVICE_PROC_CLOCK, buffer, 100))
        return(atoi(buffer));

    return 0;
}

unsigned char pnd_device_set_backlight(unsigned int c)
{
    char buffer[100];
    sprintf(buffer, "%u", c);
    return pnd_device_open_write_close(PND_DEVICE_SYS_BACKLIGHT_BRIGHTNESS, buffer);
}

unsigned int pnd_device_get_backlight(void)
{
    char buffer[100];

    if (pnd_device_open_read_close(PND_DEVICE_SYS_BACKLIGHT_BRIGHTNESS, buffer, 100))
        return atoi(buffer);

    return 0;
}
