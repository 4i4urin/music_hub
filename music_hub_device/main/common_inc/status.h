#ifndef DEVICE_STATUS
#define DEVICE_STATUS

#include "base_types.h"


#define TRACK_ST_PLAYING 1
#define TRACK_ST_TRANSMITTED 2
#define TRACK_ST_READY 3

typedef struct _t_track {
    u8_t* pmem_file;
    u16_t size;
    u16_t hash_name;
    u8_t statys;
} t_track;


typedef struct _t_track_list {
    t_track prev;
    t_track current;
    t_track next;
} t_track_list;


#endif /* DEVICE_STATUS */