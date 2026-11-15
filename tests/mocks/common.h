#ifndef GP_TEST_FLIPPER_STUB_H
#define GP_TEST_FLIPPER_STUB_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define UNUSED(x) (void)(x)
typedef int FuriMutex;
#define FuriWaitForever UINT32_MAX
#define FuriMutexTypeNormal 0
static inline FuriMutex* furi_mutex_alloc(int t){UNUSED(t);return (FuriMutex*)1;}
#ifdef GP_TEST_MUTEX_TRACK
#include <assert.h>
extern unsigned gp_test_mutex_depth;
static inline void furi_mutex_acquire(FuriMutex* m,uint32_t t){UNUSED(m);UNUSED(t);assert(!gp_test_mutex_depth);gp_test_mutex_depth++;}
static inline void furi_mutex_release(FuriMutex* m){UNUSED(m);assert(gp_test_mutex_depth==1);gp_test_mutex_depth--;}
#else
static inline void furi_mutex_acquire(FuriMutex* m,uint32_t t){UNUSED(m);UNUSED(t);}
static inline void furi_mutex_release(FuriMutex* m){UNUSED(m);}
#endif
static inline void furi_mutex_free(FuriMutex* m){UNUSED(m);}
#define RECORD_GUI "gui"
typedef enum { ColorWhite=0,ColorBlack=1 } Color;
typedef enum { FontSecondary=0,FontPrimary=1 } Font;
#ifdef GP_RASTER
#include "raster_canvas.h"
#else
typedef struct Canvas { uint8_t scratch; } Canvas;
void mock_text(int x,int y,int w,int h,const char* s);
static inline void canvas_clear(Canvas* c){UNUSED(c);}
static inline void canvas_set_color(Canvas* c,Color co){UNUSED(c);UNUSED(co);}
static inline void canvas_set_font(Canvas* c,Font ft){UNUSED(c);UNUSED(ft);}
static inline void canvas_draw_str(Canvas* c,int x,int y,const char* s){
    UNUSED(c);int n=0;while(s[n])++n;mock_text(x,y-9,n*6,9,s);
}
static inline void canvas_draw_line(Canvas* c,int x,int y,int x2,int y2){UNUSED(c);UNUSED(x);UNUSED(y);UNUSED(x2);UNUSED(y2);}
static inline void canvas_draw_disc(Canvas* c,int x,int y,int r){UNUSED(c);UNUSED(x);UNUSED(y);UNUSED(r);}
static inline void canvas_draw_circle(Canvas* c,int x,int y,int r){UNUSED(c);UNUSED(x);UNUSED(y);UNUSED(r);}
static inline void canvas_draw_dot(Canvas* c,int x,int y){UNUSED(c);UNUSED(x);UNUSED(y);}
static inline void canvas_draw_frame(Canvas* c,int x,int y,int w,int h){UNUSED(c);UNUSED(x);UNUSED(y);UNUSED(w);UNUSED(h);}
#endif
typedef enum { InputKeyUp,InputKeyDown,InputKeyLeft,InputKeyRight,InputKeyOk,InputKeyBack } InputKey;
typedef enum { InputTypeShort,InputTypeLong,InputTypeRepeat } InputType;
typedef struct { InputKey key; InputType type; } InputEvent;
typedef struct FuriMessageQueue { int ignored; } FuriMessageQueue;
typedef enum { FuriStatusOk=0,FuriStatusError=1 } FuriStatus;
static inline FuriMessageQueue* furi_message_queue_alloc(int count,int size){UNUSED(count);UNUSED(size);return (FuriMessageQueue*)1;}
static inline void furi_message_queue_free(FuriMessageQueue* q){UNUSED(q);}
static inline void furi_message_queue_put(FuriMessageQueue* q,InputEvent* e,int x){UNUSED(q);UNUSED(e);UNUSED(x);}
static inline FuriStatus furi_message_queue_get(FuriMessageQueue* q,InputEvent* e,int t){UNUSED(q);UNUSED(e);UNUSED(t);return FuriStatusError;}
static inline uint32_t furi_ms_to_ticks(uint32_t x){return x;}
static inline uint32_t furi_get_tick(void){return 0xC0FFEEu;}
typedef struct Gui {int ignored;} Gui;
typedef struct ViewPort{int ignored;} ViewPort;
static inline Gui* furi_record_open(const char* name){UNUSED(name);return (Gui*)1;}
static inline void furi_record_close(const char* name){UNUSED(name);}
static inline ViewPort* view_port_alloc(void){return (ViewPort*)1;}
static inline void view_port_free(ViewPort* vp){UNUSED(vp);}
static inline void view_port_draw_callback_set(ViewPort* vp,void(*f)(Canvas*,void*),void* ctx){UNUSED(vp);UNUSED(f);UNUSED(ctx);}
static inline void view_port_input_callback_set(ViewPort* vp,void(*f)(InputEvent*,void*),void* ctx){UNUSED(vp);UNUSED(f);UNUSED(ctx);}
static inline void view_port_update(ViewPort* vp){UNUSED(vp);}
#define GuiLayerFullscreen 0
static inline void gui_add_view_port(Gui* g,ViewPort* vp,int layer){UNUSED(g);UNUSED(vp);UNUSED(layer);}
static inline void gui_remove_view_port(Gui* g,ViewPort* vp){UNUSED(g);UNUSED(vp);}
#endif
