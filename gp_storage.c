#include <furi.h>
#include <storage/storage.h>
#include "gp_storage.h"
#define SAVE_DIR EXT_PATH("apps_data/flipper_gp_manager")
static const char* paths[]={SAVE_DIR "/race.sav",SAVE_DIR "/race.bak",SAVE_DIR "/race.tmp",SAVE_DIR "/slot2.sav",SAVE_DIR "/slot2.bak",SAVE_DIR "/slot2.tmp",SAVE_DIR "/slot3.sav",SAVE_DIR "/slot3.bak",SAVE_DIR "/slot3.tmp"};
static bool ready(void* context){return storage_sd_status(context)==FSE_OK;}
static GpIoResult read_data(void* context,unsigned slot,uint8_t* out,size_t capacity,size_t* length){
    Storage* storage=context;
    FS_Error stat=storage_common_stat(storage,paths[slot],NULL);
    if(stat==FSE_NOT_EXIST)return GpIoMissing;
    if(stat!=FSE_OK)return GpIoError;
    File* file=storage_file_alloc(storage);GpIoResult result=GpIoError;
    if(storage_file_open(file,paths[slot],FSAM_READ,FSOM_OPEN_EXISTING)){
        uint64_t size=storage_file_size(file);
        *length=size>capacity?capacity+1:(size_t)size;
        if(size>capacity)result=GpIoOk;
        else if(storage_file_read(file,out,*length)==*length)result=GpIoOk;
    }
    storage_file_close(file);storage_file_free(file);return result;
}
static bool write_data(void* context,unsigned slot,const uint8_t* data,size_t length){
    Storage* storage=context;
    if(!storage_simply_mkdir(storage,EXT_PATH("apps_data"))||!storage_simply_mkdir(storage,SAVE_DIR))return false;
    File* file=storage_file_alloc(storage);bool ok=false;
    if(storage_file_open(file,paths[slot],FSAM_WRITE,FSOM_CREATE_ALWAYS))
        ok=storage_file_write(file,data,length)==length&&storage_file_sync(file);
    bool closed=storage_file_close(file);storage_file_free(file);return ok&&closed;
}
static bool replace(void* context,unsigned from,unsigned to){return storage_common_rename(context,paths[from],paths[to])==FSE_OK;}
GpSaveIo gp_storage_open(void){return (GpSaveIo){furi_record_open(RECORD_STORAGE),ready,read_data,write_data,replace,0};}
void gp_storage_close(GpSaveIo* io){furi_record_close(RECORD_STORAGE);io->context=NULL;}
