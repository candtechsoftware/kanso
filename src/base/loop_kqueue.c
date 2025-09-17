#include "arena.h"
#include "loop.h"
#include "tctx.h"
#include "os.h"
#include "util.h"
#include <mach/message.h>
#include <pthread.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

struct Loop {
    Arena                *arena;
    s32                   kq;
    Loop_Submission_List *list;
};

internal Loop *
loop_create(void) {
    Arena *arena = arena_alloc();
    Loop  *loop = push_struct(arena, Loop);
    loop->kq = kqueue();
    if (loop->kq < 0) {
        arena_release(arena);
        return NULL;
    }
    loop->list = push_array(arena, Loop_Submission_List, 1);
    return loop;
}

internal void
loop_destroy(Loop *l) {
    arena_release(l->arena);
}

internal void
loop_enqueue(Loop *l, Loop_Submission *s) {
    SLLQueuePush(l->list->first, l->list->last, s);
    l->list->node_count++;
}

internal void
kqueue_apply(Loop *l, Loop_Submission *s) {
    u64 fd = s->handle.u64s[0];

    switch (s->op) {
    case LOOP_OP_ADD:
    case LOOP_OP_MOD: {
        struct kevent evs[2];

        s32 n = 0;
        if (s->events & Loop_Event_Flag_Read) {
            EV_SET(&evs[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, s->userdata);
        }
        if (s->events & Loop_Event_Flag_Write) {
            EV_SET(&evs[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, s->userdata);
        }
        if (n > 0) {
            kevent(l->kq, evs, n, NULL, 0, NULL);
        }

        break;
    }
    case LOOP_OP_DEL: {
        struct kevent evs[2];
        EV_SET(&evs[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&evs[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        kevent(l->kq, evs, 2, NULL, 0, NULL);
        break;
    }
    case LOOP_OP_USER: {
        break;
    }
    default:
        break;
    }
}

internal int
loop_submit(Loop *l) {
    Loop_Submission *s;
    while ((s = l->list->first)) {
        SLLQueuePop(l->list->first, l->list->last);
        l->list->node_count -= 1;
        kqueue_apply(l, s);
    }
    return 0;
}
internal int
loop_wait(Loop *l, Loop_Event *events, s32 max_events, s32 timeout_ms) {
    ASSERT(max_events > 0, "maxevents needs to be greater than 0");
    struct timespec ts, *tsp = NULL;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
        tsp = &ts;
    }
    struct kevent kev[max_events];
    s32           n = kevent(l->kq, NULL, 0, kev, max_events, tsp);
    if (n <= 0) {
        return n;
    }

    for (s32 i = 0; i < n; i++) {
        events[i].next = NULL;
        events[i].handle = os_handle_from_u64(kev[i].ident);
        events[i].userdata = kev[i].udata;
        events[i].events = 0;
        events[i].result = 0;

        if (kev[i].filter == EVFILT_READ) {
            events[i].events |= Loop_Event_Flag_Read;
        }
        if (kev[i].filter == EVFILT_WRITE) {
            events[i].events |= Loop_Event_Flag_Write;
        }
        if (kev[i].flags & EV_ERROR) {
            events[i].events |= Loop_Event_Flag_Error;
            events[i].result = kev[i].data;
        }
    }

    return n;
}
