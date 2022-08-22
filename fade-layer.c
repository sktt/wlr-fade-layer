#define _POSIX_C_SOURCE 200112L
#include <linux/input-event-codes.h>
#include <assert.h>
#include <GLES2/gl2.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include "egl_common.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"


// t = current time, d = duration
double easeOutQuad(double t, double d) {
    double td = t / d ;
    return 2*td - td*td;
}

double easeOutCubic(double t, double d) {
    double td = t / d - 1;
    return td*td*td+1;
}

// args
static double fade_time = 5000;



static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_seat *seat;
static struct wl_shm *shm;
static struct wl_pointer *pointer;
static struct wl_keyboard *keyboard;
static struct xdg_wm_base *xdg_wm_base;
static struct zwlr_layer_shell_v1 *layer_shell;
static bool run_display = true;
static enum zwlr_layer_surface_v1_keyboard_interactivity keyboard_interactive =
ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
struct wl_list fl_surfaces;

struct fl_surface {
    struct wl_output *output;
    struct wl_surface *surface;
    struct wl_egl_window *egl_window;
    struct wlr_egl_surface *egl_surface;
    struct wl_callback *frame_callback;
    struct wl_list link;
    uint32_t output_global_name;
    uint32_t width;
    uint32_t height;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct {
        struct timespec first_frame;
        double alpha;
    } demo;
};

struct wl_surface *cursor_surface, *input_surface;


static void draw(struct fl_surface *s);

static void surface_frame_callback(
        void *data, struct wl_callback *cb, uint32_t time) {
    wl_callback_destroy(cb);
    struct fl_surface *s = data;
    s->frame_callback = NULL;
    draw(s);
}

static struct wl_callback_listener frame_listener = {
    .done = surface_frame_callback
};

static void draw(struct fl_surface *s) {
    eglMakeCurrent(egl_display, s->egl_surface, s->egl_surface, egl_context);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    long ms_first_frame = (ts.tv_sec - s->demo.first_frame.tv_sec) * 1000 +
        (ts.tv_nsec - s->demo.first_frame.tv_nsec) / 1000000;

    if (ms_first_frame >= fade_time) {
        s->demo.alpha = 1;
    } else {
        s->demo.alpha = easeOutCubic(ms_first_frame, fade_time);
    }

    glViewport(0, 0, s->width, s->height);
    glClearColor(0, 0, 0, s->demo.alpha);
    glClear(GL_COLOR_BUFFER_BIT);

    s->frame_callback = wl_surface_frame(s->surface);
    wl_callback_add_listener(s->frame_callback, &frame_listener, s);

    eglSwapBuffers(egl_display, s->egl_surface);
}

static void layer_surface_configure(void *data,
        struct zwlr_layer_surface_v1 *surface,
        uint32_t serial, uint32_t w, uint32_t h) {
    struct fl_surface *s = data;
    s->width = w;
    s->height = h;
    if (s->egl_window) {
        wl_egl_window_resize(s->egl_window, s->width, s->height, 0, 0);
    }
    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void layer_surface_closed(void *data,
        struct zwlr_layer_surface_v1 *surface) {

    struct fl_surface *s = data;

    eglDestroySurface(egl_display, s->egl_surface);
    wl_egl_window_destroy(s->egl_window);
    zwlr_layer_surface_v1_destroy(surface);
    wl_surface_destroy(s->surface);
    run_display = false;
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
        uint32_t serial, struct wl_surface *surface,
        wl_fixed_t surface_x, wl_fixed_t surface_y) {
    // dont care
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
        uint32_t serial, struct wl_surface *surface) {
    // dont care
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
        uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    exit(0);
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
        uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    struct fl_surface *s = data;
    if (input_surface == s->surface) {
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
            printf("ptr btn\n");
        }
    } else {
        assert(false && "Unknown surface");
    }
}

static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer,
        uint32_t time, uint32_t axis, wl_fixed_t value) {
    // Who cares
}

static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
    // Who cares
}

static void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
        uint32_t axis_source) {
    // Who cares
}

static void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
        uint32_t time, uint32_t axis) {
    // Who cares
}

static void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
        uint32_t axis, int32_t discrete) {
    // Who cares
}

struct wl_pointer_listener pointer_listener = {
    .enter = wl_pointer_enter,
    .leave = wl_pointer_leave,
    .motion = wl_pointer_motion,
    .button = wl_pointer_button,
    .axis = wl_pointer_axis,
    .frame = wl_pointer_frame,
    .axis_source = wl_pointer_axis_source,
    .axis_stop = wl_pointer_axis_stop,
    .axis_discrete = wl_pointer_axis_discrete,
};

static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t format, int32_t fd, uint32_t size) {
    // Who cares
}

static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    fprintf(stderr, "Keyboard enter\n");
}

static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, struct wl_surface *surface) {
    fprintf(stderr, "Keyboard leave\n");
}

static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    fprintf(stderr, "Key event: %d %d\n", key, state);
    exit(0);
}

static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
        uint32_t mods_locked, uint32_t group) {
    // Who cares
}

static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
        int32_t rate, int32_t delay) {
    // Who cares
}

static struct wl_keyboard_listener keyboard_listener = {
    .keymap = wl_keyboard_keymap,
    .enter = wl_keyboard_enter,
    .leave = wl_keyboard_leave,
    .key = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers,
    .repeat_info = wl_keyboard_repeat_info,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
        enum wl_seat_capability caps) {
    if ((caps & WL_SEAT_CAPABILITY_POINTER)) {
        pointer = wl_seat_get_pointer(wl_seat);
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
        keyboard = wl_seat_get_keyboard(wl_seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
    }
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
        const char *name) {
    // Who cares
}

const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name,
};
static void handle_wl_output_geometry(void *data, struct wl_output *wl_output,
        int32_t x, int32_t y, int32_t width_mm, int32_t height_mm,
        int32_t subpixel, const char *make, const char *model,
        int32_t transform) {
}

static void handle_wl_output_mode(void *data, struct wl_output *output,
        uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    // Who cares
}

static void handle_wl_output_done(void *data, struct wl_output *output) {
    // Who cares
}

static void handle_wl_output_scale(void *data, struct wl_output *output,
        int32_t factor) {

}

static void handle_wl_output_name(void *data, struct wl_output *output,
        const char *name) {
}

static void handle_wl_output_description(void *data, struct wl_output *output,
        const char *description) {
    // Who cares
}

struct wl_output_listener _wl_output_listener = {
    .geometry = handle_wl_output_geometry,
    .mode = handle_wl_output_mode,
    .done = handle_wl_output_done,
    .scale = handle_wl_output_scale,
    .name = handle_wl_output_name,
    .description = handle_wl_output_description,
};

static void handle_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name,
                &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name,
                &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct fl_surface *s = calloc(1, sizeof(struct fl_surface));
        clock_gettime(CLOCK_MONOTONIC, &s->demo.first_frame);

        s->output = wl_registry_bind(registry, name,
                &wl_output_interface, version < 4 ? version : 4 );
        s->output_global_name = name;
        wl_output_add_listener(s->output, &_wl_output_listener, s->surface);
        wl_list_insert(&fl_surfaces, &s->link);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        seat = wl_registry_bind(registry, name,
                &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        layer_shell = wl_registry_bind(registry, name,
                &zwlr_layer_shell_v1_interface, version < 4 ? version : 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind(
                registry, name, &xdg_wm_base_interface, 1);
    }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
        uint32_t name) {
    // who cares
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

int main(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, "t:")) != -1) {
        switch (c) {
            case 't':
                fade_time = atoi(optarg);
                break;
            default:
                return 1;
        }
    }
    char *namespace = "wlroots";
    display = wl_display_connect(NULL);
    if (display == NULL) {
        fprintf(stderr, "Failed to create display\n");
        return 1;
    }
    wl_list_init(&fl_surfaces);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (compositor == NULL) {
        fprintf(stderr, "wl_compositor not available\n");
        return 1;
    }
    if (shm == NULL) {
        fprintf(stderr, "wl_shm not available\n");
        return 1;
    }
    if (layer_shell == NULL) {
        fprintf(stderr, "layer_shell not available\n");
        return 1;
    }

    egl_init(display);

    struct fl_surface *s;
    wl_list_for_each(s, &fl_surfaces, link) {
        s->surface = wl_compositor_create_surface(compositor);
        assert(s->surface);
        s->layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell,
                s->surface, s->output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, namespace);
        assert(s->layer_surface);
        zwlr_layer_surface_v1_set_size(s->layer_surface, 0, 0);
        zwlr_layer_surface_v1_set_anchor(s->layer_surface,
                ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
        zwlr_layer_surface_v1_set_exclusive_zone(s->layer_surface, 1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(
                s->layer_surface, keyboard_interactive);
        zwlr_layer_surface_v1_add_listener(s->layer_surface,
                &layer_surface_listener, s);
        wl_surface_commit(s->surface);
        wl_display_roundtrip(display);

        s->egl_window = wl_egl_window_create(s->surface, s->width, s->height);
        assert(s->egl_window);
        s->egl_surface = eglCreatePlatformWindowSurfaceEXT(
                egl_display, egl_config, s->egl_window, NULL);
        assert(s->egl_surface != EGL_NO_SURFACE);

        wl_display_roundtrip(display);

        draw(s);
    }
    while (wl_display_dispatch(display) != -1 && run_display) {
        // This space intentionally left blank
    }

    return 0;
}
