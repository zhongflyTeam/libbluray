
#include <dlfcn.h>
#include <stdio.h>

#include "bluray.h"
#include "util/macro.h"
#include "util/logging.h"

BLURAY *bd_open(const char* device_path, const char* keyfile_path)
{
    BLURAY *bd = malloc(sizeof(BLURAY));

    bd->aacs = NULL;
    bd->h_libaacs = NULL;
    bd->fp = NULL;

    // open aacs decryptor if present
    if ((bd->h_libaacs = dlopen("libaacs.so", RTLD_LAZY))) {
        DEBUG(DBG_BLURAY, "Downloaded libaacs (0x%08x)\n", bd->h_libaacs);

        typedef AACS_KEYS* (*fptr)();

        fptr fptr_s = dlsym(bd->h_libaacs, "aacs_open");
        bd->aacs = fptr_s(device_path, keyfile_path);
    } else {
        DEBUG(DBG_BLURAY, "libaacs not present\n");
    }

    DEBUG(DBG_BLURAY, "BLURAY initialized! (0x%08x)\n", bd);

    return bd;
}

void bd_close(BLURAY *bd)
{
    if (bd->h_libaacs) {
        typedef void* (*fptr)();

        fptr fptr_s = dlsym(bd->h_libaacs, "aacs_close");
        fptr_s(bd->aacs);
    }

    dlclose(bd->h_libaacs);

    file_close(bd->fp);

    DEBUG(DBG_BLURAY, "BLURAY destroyed! (0x%08x)\n", bd);

    X_FREE(bd);
}

off_t bd_seek(BLURAY *bd, off_t pos)
{
    if (pos < bd->s_size) {
        bd->s_pos = pos - (pos % 6144);

        file_seek(bd->fp, bd->s_pos, SEEK_SET);
    }

    return bd->s_pos;
}

int bd_read(BLURAY *bd, unsigned char *buf, int len)
{
    if (len + bd->s_pos < bd->s_size) {
        int read;

        if ((read = file_read(bd->fp, buf, len))) {
            if (bd->h_libaacs) {
                bd->libaacs_decrypt_unit = dlsym(bd->h_libaacs, "aacs_decrypt_unit");
                if (!bd->libaacs_decrypt_unit(bd->aacs, buf, len)) {
                    return 0;
                }
            }

            bd->s_pos += len;

            return read;
        }
    }

    return 0;
}
