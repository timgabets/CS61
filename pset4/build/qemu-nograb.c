// Prevent QEMU from grabbing the user's mouse.
// QEMU does not offer an option for doing this, except if we were to
// enable USB within our OS. That's complicated, so we cheat.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* (*next_malloc)(size_t sz);
static void* (*next_calloc)(size_t nmemb, size_t sz);

typedef struct fake_qemu_put_mouse_event {
    void* qemu_put_mouse_event;
    void* qemu_put_mouse_event_opaque;
    int qemu_put_mouse_event_absolute;
    char* qemu_put_mouse_event_name;
    int index;
    void* node_next;
    void* node_prev;
} fake_qemu_put_mouse_event;

static const char mouse_string[] = "QEMU PS/2 Mouse";

static int test_mode = 0;
static fake_qemu_put_mouse_event* test_event;
static char* test_name;

void* malloc(size_t sz) {
    if (!next_malloc) {
        next_malloc = dlsym(RTLD_NEXT, "malloc");
        next_calloc = dlsym(RTLD_NEXT, "calloc");
    }

    if (test_mode == 2 && test_event
        && test_event->qemu_put_mouse_event_name == test_name
        && memcmp(test_name, mouse_string, sizeof(mouse_string)) == 0
        && test_event->qemu_put_mouse_event_absolute == 0) {
        test_event->qemu_put_mouse_event_absolute = 1;
        test_mode = -1;
    }

    void* ptr = next_malloc(sz);

    if (test_mode < 0)
        /* do nothing */;
    else if (test_mode == 1 && sz == sizeof(mouse_string)) {
        test_name = ptr;
        test_mode = 2;
    } else if (sz == sizeof(fake_qemu_put_mouse_event)) {
        test_event = ptr;
        test_mode = 1;
    } else
        test_mode = 0;

    return ptr;
}

/* 0.15 requires we track calloc too */
void* calloc(size_t nmemb, size_t sz) {
    if (!next_calloc) {
        extern void* __libc_calloc(size_t nmemb, size_t sz);
        return __libc_calloc(nmemb, sz); /* avoid infinite regress */
    }

    if (test_mode == 2 && test_event
        && test_event->qemu_put_mouse_event_name == test_name
        && memcmp(test_name, mouse_string, sizeof(mouse_string)) == 0
        && test_event->qemu_put_mouse_event_absolute == 0) {
        test_event->qemu_put_mouse_event_absolute = 1;
        test_mode = -1;
    }


    void* ptr = next_calloc(nmemb, sz);

    if (test_mode < 0)
        /* do nothing */;
    else if (nmemb == 1 && sz == sizeof(fake_qemu_put_mouse_event)
             && test_mode != 2) {
        test_event = ptr;
        test_mode = 1;
    } else if (test_mode != 2)
        test_mode = 0;

    return ptr;
}
