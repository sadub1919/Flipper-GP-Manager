#ifndef GP_STORAGE_H
#define GP_STORAGE_H
#include "gp_save.h"
/* Storage lifetime is owned by the application thread; never called from draw. */
GpSaveIo gp_storage_open(void);
void gp_storage_close(GpSaveIo*);
#endif
