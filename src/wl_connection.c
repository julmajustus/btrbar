/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   wl_connection.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 17:27:24 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/04 22:34:20 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "wl_connection.h"
#include "config.h"
#include "ipc.h"
#include "input.h"

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <stdint.h>
#include <stddef.h>

static void
buffer_cleanup(bar_t *b)
{
    if (b->argb_bufs[0]) {
        munmap(b->argb_bufs[0], b->pool_size);
        b->argb_bufs[0] = NULL;
        b->argb_bufs[1] = NULL;
    }
    if (b->buffers[0]) {
        wl_buffer_destroy(b->buffers[0]);
        b->buffers[0] = NULL;
    }
    if (b->buffers[1]) {
        wl_buffer_destroy(b->buffers[1]);
        b->buffers[1] = NULL;
    }
    if (b->shm_fd >= 0) {
        close(b->shm_fd);
        b->shm_fd = -1;
    }
    b->pool_size = 0;
}

void
wl_cleanup(bar_t *b)
{
	buffer_cleanup(b);

    if (b->pointer) {
        wl_pointer_destroy(b->pointer);
        b->pointer = NULL;
    }
	if (b->seat) {
		wl_seat_destroy(b->seat);
		b->seat = NULL;
	}
    if (b->ipc_out) {
        zdwl_ipc_output_v2_destroy(b->ipc_out);
        b->ipc_out = NULL;
    }
    if (b->ipc_mgr) {
        zdwl_ipc_manager_v2_destroy(b->ipc_mgr);
        b->ipc_mgr = NULL;
    }
	for (size_t i = 0; i < b->n_outputs; i++) {
		wl_output_destroy(b->outputs[i]);
	}
	b->n_outputs = 0;
    if (b->layer_surface) {
        zwlr_layer_surface_v1_destroy(b->layer_surface);
        b->layer_surface = NULL;
    }
    if (b->surface) {
        wl_surface_destroy(b->surface);
        b->surface = NULL;
    }
    if (b->layer_shell) {
        zwlr_layer_shell_v1_destroy(b->layer_shell);
        b->layer_shell = NULL;
    }
    if (b->shm) {
        wl_shm_destroy(b->shm);
        b->shm = NULL;
    }
    if (b->compositor) {
        wl_compositor_destroy(b->compositor);
        b->compositor = NULL;
    }
    if (b->registry) {
        wl_registry_destroy(b->registry);
        b->registry = NULL;
    }
}

static int
create_shm_file(size_t size)
{
	char template[] = "/tmp/btrbar-shm-XXXXXX";
	int fd = mkstemp(template);
	if (fd < 0)
		return -1;
	unlink(template);
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static int
create_buffers(bar_t *b)
{
	buffer_cleanup(b);
	int  stride  = b->width * 4;
    size_t buf_sz = (size_t)stride * b->height;
    size_t pool_sz = buf_sz * 2;

    b->shm_fd = create_shm_file(pool_sz);
    if (b->shm_fd < 0)
		return -1;

    void *base = mmap(NULL, pool_sz,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, b->shm_fd, 0);
    if (base == MAP_FAILED)
		return -1;

    b->argb_bufs[0] = base;
    b->argb_bufs[1] = (uint32_t *)((char*)base + buf_sz);

    b->shm_pool = wl_shm_create_pool(b->shm, b->shm_fd, pool_sz);
    b->buffers[0] = wl_shm_pool_create_buffer(
        b->shm_pool, 0,
        b->width, b->height,
        stride,
        WL_SHM_FORMAT_ARGB8888);
    b->buffers[1] = wl_shm_pool_create_buffer(
        b->shm_pool, buf_sz,
        b->width, b->height,
        stride,
        WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(b->shm_pool);
    b->pool_size = pool_sz;
    return 0;
}

static void
layer_surface_handle_configure(
	void *data,
	struct zwlr_layer_surface_v1 *lsurf,
	uint32_t serial,
	uint32_t width,
	uint32_t height)
{

	bar_t *b = data;
	// fprintf(stderr, "configure: serial=%u, width=%u, height=%u\n", serial, width, height);

	b->width = width;
	b->height = height;
	zwlr_layer_surface_v1_ack_configure(lsurf, serial);
	create_buffers(b);
}

static void
layer_surface_handle_closed(void *data,
							struct zwlr_layer_surface_v1 *layer_surface)
{
	(void)data, (void)layer_surface;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_handle_configure,
	.closed = layer_surface_handle_closed
};


static void
registry_global(void *data, struct wl_registry *reg,
				uint32_t id, const char *interface, uint32_t version)
{
	(void)version;
	bar_t *b = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		b->compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 4);
	}
	else if (strcmp(interface, wl_shm_interface.name) == 0) {
		b->shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);
	}
	else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		b->layer_shell = wl_registry_bind(reg, id, &zwlr_layer_shell_v1_interface, 1);
	}
	else if (IPC && strcmp(interface, zdwl_ipc_manager_v2_interface.name) == 0) {
		uint32_t bind_ver = version < 2 ? version : 2;
		b->ipc_mgr = wl_registry_bind(reg, id, &zdwl_ipc_manager_v2_interface, bind_ver);
		zdwl_ipc_manager_v2_add_listener(b->ipc_mgr, &ipc_manager_listener, b);
	}
	else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *wlo = wl_registry_bind(reg, id, &wl_output_interface, 2);
		if (b->n_outputs < MAX_OUTPUTS)
			b->outputs[b->n_outputs++] = wlo;
		wl_output_add_listener(wlo, &output_listener, b);

		if (IPC && b->ipc_mgr) {
			b->ipc_out = zdwl_ipc_manager_v2_get_output(b->ipc_mgr, wlo);
			zdwl_ipc_output_v2_add_listener(b->ipc_out, &ipc_output_listener, b);
		}
	}
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		b->seat = wl_registry_bind(reg, id, &wl_seat_interface,    5);
		b->pointer = wl_seat_get_pointer(b->seat);
		wl_pointer_add_listener(b->pointer, &pointer_listener, b);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *reg, uint32_t id)
{
	(void)data, (void)reg, (void)id;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove
};

int
init_wl(bar_t *b)
{
	b->display = wl_display_connect(NULL);
	if (!b->display) {
		fprintf(stderr, "Failed to connect to Wayland display\n");
		return -1;
	}
	assert(b->display);
	b->registry = wl_display_get_registry(b->display);
	wl_registry_add_listener(b->registry, &registry_listener, b);
	wl_display_roundtrip(b->display);
	wl_display_roundtrip(b->display);
	assert(b->compositor && b->shm && b->layer_shell);

	b->surface = wl_compositor_create_surface(b->compositor);
	b->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		b->layer_shell,
		b->surface,
		NULL, // default output
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		"btrb");
	zwlr_layer_surface_v1_set_anchor(
		b->layer_surface,
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

	zwlr_layer_surface_v1_set_exclusive_zone(b->layer_surface, BAR_HEIGHT);
	zwlr_layer_surface_v1_set_size(b->layer_surface, BAR_WIDTH, BAR_HEIGHT);
	zwlr_layer_surface_v1_add_listener(b->layer_surface, &layer_surface_listener, b);
	wl_surface_commit(b->surface);
	wl_display_roundtrip(b->display);
	return 0;
}
