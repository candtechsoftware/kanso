#pragma once

#include "../os/os.h"
#include "types.h"

typedef enum Loop_Operation
{
    LOOP_OP_NOP = 0,
    LOOP_OP_ADD,
    LOOP_OP_MOD,
    LOOP_OP_DEL,
    LOOP_OP_READ,
    LOOP_OP_WRITE,
    LOOP_OP_USER
} Loop_Operation;

typedef enum Loop_Event_Flags
{
    Loop_Event_Flag_Read = (1 << 0),
    Loop_Event_Flag_Write = (1 << 1),
    Loop_Event_Flag_Error = (1 << 2),
} Loop_Event_Flags;

typedef struct Loop_Submission Loop_Submission;
struct Loop_Submission
{
    Loop_Submission *next;
    Loop_Operation   op;
    OS_Handle        handle;
    unsigned         events;
    void            *userdata;
};

typedef struct Loop_Submission_List Loop_Submission_List;
struct Loop_Submission_List
{
    Loop_Submission *first;
    Loop_Submission *last;
    uint64_t         node_count;
};

typedef struct Loop_Event Loop_Event;
struct Loop_Event
{
    Loop_Event *next;
    OS_Handle   handle;
    u32         events;
    void       *userdata;
    s64         result;
};

typedef struct Loop_Event_List Loop_Event_List;
struct Loop_Event_List
{
    Loop_Event *first;
    Loop_Event *last;
    u64         node_count;
};

typedef struct Loop Loop;

internal Loop *loop_create(void);
internal void  loop_destroy(Loop *l);
internal void  loop_enqueue(Loop *l, Loop_Submission *s);
internal int   loop_submit(Loop *l);
internal int   loop_wait(Loop *l, Loop_Event *events, s32 maxevents, s32 timeout_ms);
