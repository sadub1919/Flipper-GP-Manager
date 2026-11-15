#ifndef GP_TEST_SAVE_IO_H
#define GP_TEST_SAVE_IO_H
#include "../gp_save.h"
#include <string.h>
typedef struct {
    uint8_t data[9][GP_SAVE_BYTES];size_t sizes[9];bool present[9];
    bool no_card,fail_read,fail_write,fail_replace,bad_readback;
    int truncate_at;
} GpTestFs;
static inline bool test_ready(void* ctx){return !((GpTestFs*)ctx)->no_card;}
static inline GpIoResult test_read(void* ctx,unsigned slot,uint8_t* out,size_t capacity,size_t* size){
    GpTestFs* fs=ctx;if(fs->fail_read)return GpIoError;if(!fs->present[slot])return GpIoMissing;
    *size=fs->sizes[slot];memcpy(out,fs->data[slot],*size<capacity?*size:capacity);
    if(slot%3==2&&fs->bad_readback)out[20]^=1;
    return GpIoOk;
}
static inline bool test_write(void* ctx,unsigned slot,const uint8_t* data,size_t size){
    GpTestFs* fs=ctx;fs->present[slot]=true;
    size_t n=fs->fail_write?(size_t)fs->truncate_at:size;
    fs->sizes[slot]=n;memcpy(fs->data[slot],data,n);return !fs->fail_write;
}
static inline bool test_replace(void* ctx,unsigned from,unsigned to){
    GpTestFs* fs=ctx;if(fs->fail_replace)return false;
    memcpy(fs->data[to],fs->data[from],GP_SAVE_BYTES);fs->sizes[to]=fs->sizes[from];fs->present[to]=true;fs->present[from]=false;return true;
}
static inline GpSaveIo test_io(GpTestFs* fs){return (GpSaveIo){fs,test_ready,test_read,test_write,test_replace,0};}
extern GpTestFs gp_test_fs;
#endif
