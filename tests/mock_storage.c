#include "../gp_storage.h"
#include "mock_save_io.h"
GpTestFs gp_test_fs;
GpSaveIo gp_storage_open(void){return test_io(&gp_test_fs);}
void gp_storage_close(GpSaveIo* io){(void)io;}
