/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bar.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 10:13:11 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/11 21:05:43 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#define _POSIX_C_SOURCE 200809L
#include "bar.h"
#include "blocks.h"
#include "render.h"
#include "wl_connection.h"
#include "tools.h"
#include "systray.h"
#include "systray_watcher.h"
#include "input.h"
#include "ipc.h"

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

static volatile sig_atomic_t terminate_requested = 0;

static inline int is_disconnect_errno(void) {
	return errno == EPIPE || errno == ECONNRESET;
}

static inline int
block_is_shared(blk_type_t t)
{
	return !(t == BLK_TAG || t == BLK_LAYOUT || t == BLK_TITLE || t == BLK_TRAY);
}

static void
on_signal(int sig)
{
	(void)sig;
	terminate_requested = 1;
}

static void
free_font(bar_manager_t *m)
{
	free(m->ttf_buffer);
	free(m->atlas.pixels);
	free(m->atlas.table);
	m->atlas.pixels = NULL;
	m->atlas.table  = NULL;
	m->atlas.cap = m->atlas.count = 0;
}

static int
init_font(bar_manager_t *m)
{
	FILE *fp = fopen(FONT, "rb");
	if (!fp) {
		perror("font file");
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	m->ttf_buffer = malloc(sz);
	if (!m->ttf_buffer) {
		fclose(fp);
		return -1;
	}
	if (fread(m->ttf_buffer, 1, sz, fp) == 0) {
		if (ferror(fp))
			fclose(fp);
		return -1;
	}
	fclose(fp);

	if (!stbtt_InitFont(&m->font, m->ttf_buffer, stbtt_GetFontOffsetForIndex(m->ttf_buffer,0))) {
		free_font(m);
		return -1;
	}

	m->font_scale = stbtt_ScaleForPixelHeight(&m->font, F_SIZE);
	stbtt_GetFontVMetrics(&m->font, &m->font_ascent, 0, 0);
	m->font_baseline = (int)(m->font_ascent * m->font_scale);
	
	m->atlas.pixels = calloc(ATLAS_W * ATLAS_H, 1);
	m->atlas.table  = calloc(ATLAS_CAP, sizeof(glyph_entry));
	m->atlas.cap    = ATLAS_CAP;
	m->atlas.count  = 0;
	m->atlas.pen_x  = 0;
	m->atlas.pen_y  = 0;
	m->atlas.row_h  = 0;
	if (!m->atlas.pixels || !m->atlas.table) {
		free(m->atlas.pixels);
		free(m->atlas.table);
		return -1;
	}
	return 0;
}

static void
install_signal_handlers(void)
{
	struct sigaction sa = {
		.sa_handler = on_signal,
		.sa_flags   = 0,
	};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
}

static int
init_shared_blocks(bar_manager_t *m)
{
	m->block_cfg = blocks_cfg;

	for (long i = 0; i < ASIZE(blocks_cfg); ++i) {
		const block_cfg_t *c = &blocks_cfg[i];
		block_t *sb = &m->shared_blocks[i];

		sb->type      = c->type;
		sb->cmd       = c->cmd;
		sb->prefix    = c->prefix;
		sb->get_label = c->get_label;
		sb->pfx_color = c->pfx_color;
		sb->fg_color  = c->fg_color;
		sb->bg_color  = c->bg_color;
		sb->on_click  = c->on_click;
		sb->on_scroll = c->on_scroll;
		sb->align     = c->align;
		sb->interval_ms = c->interval_ms * 1000u;

		sb->last_update_ms = 0;
		sb->label[0] = '\0';
		sb->version = 1;
	}

	for (long i = 0; i < ASIZE(blocks_cfg); ++i) {
		if (block_is_shared(m->shared_blocks[i].type))
			if (update_block(&m->shared_blocks[i], 0))
				m->shared_blocks[i].version++;
	}
	return 0;
}

static void
init_bar_blocks(bar_t *b, bar_manager_t *m)
{
	m->block_cfg = blocks_cfg;

	for (long i = 0; i < ASIZE(blocks_cfg); ++i) {
		const block_cfg_t *c = &blocks_cfg[i];
		block_inst_t *bi = &b->block_inst[i];

		if (block_is_shared(c->type)) {
			bi->block = &m->shared_blocks[i];
		} else {
			block_t *block = &b->local_blocks[i];
			block->type      = c->type;
			block->cmd       = c->cmd;
			block->prefix    = c->prefix;
			block->get_label = c->get_label;
			block->pfx_color = c->pfx_color;
			block->fg_color  = c->fg_color;
			block->bg_color  = c->bg_color;
			block->on_click  = c->on_click;
			block->on_scroll = c->on_scroll;
			block->align     = c->align;
			block->interval_ms = c->interval_ms * 1000u;
			block->last_update_ms = 0;

			block->label[0] = '\0';
			block->version = 1;

			bi->block = block;
		}

		bi->x0 = bi->x1 = bi->old_x0 = bi->old_x1 = 0;
		bi->current_width = 0;
		bi->seen_version  = 0;
		bi->local.tray.n = 0;
	}
}

int
init_bar(bar_manager_t *m, struct wl_output *wlo, uint32_t id)
{
	bar_t *b = calloc(1, sizeof(bar_t));

	b->display = m->display;
	b->registry = m->registry;
	b->compositor = m->compositor;
	b->layer_shell = m->layer_shell;
	b->shm = m->shm;
	b->ipc_mgr = m->ipc_mgr;
	b->output = wlo;
	b->output_id = id;
	m->bars[m->n_bars++] = b;
	if (TRAY) {

		b->tray = m->tray;
		b->tray->bar = b;
		b->tray->manager = m;
	}
	b->bar_manager = m;
	b->is_clicked = 0;
	b->font = m->font;
	b->font_scale = m->font_scale;
	b->font_ascent = m->font_ascent;
	b->font_baseline = m->font_baseline;
	init_bar_blocks(b, m);
	wl_output_add_listener(wlo, &output_listener, b);

	if (init_bar_surface(m, b) == -1) {
		fprintf(stderr, "Failed to init bar surface\n");
		return -1;
	}

	if (IPC && m->ipc_mgr) {
		b->ipc_out = zdwl_ipc_manager_v2_get_output(m->ipc_mgr, wlo);
		if (TAGS) {
			b->tags = calloc(ASIZE(tag_icons), sizeof(*b->tags));
			if (!b->tags || init_tags(b->tags) == -1) {
				fprintf(stderr, "Tag init failed\n");
				b->tags = NULL;
				return -1;
			}
		}
		zdwl_ipc_output_v2_add_listener(b->ipc_out, &ipc_output_listener, b);
	}
	b->pointer = wl_seat_get_pointer(m->seat);
	wl_pointer_add_listener(b->pointer, &pointer_listener, b);

	return 0;
}

int
init_manager(bar_manager_t *m)
{
	memset(m, 0, sizeof *m);
	if (init_font(m) == -1) {
		fprintf(stderr, "Failed to init font\n");
		return -1;
	}

	if (TRAY) {
		m->tray = calloc(1, sizeof(*m->tray));
		if (!m->tray || systray_init(m->tray) == -1) {
			fprintf(stderr, "Failed to init systray\n");
			return -1;
		}
	}

	init_shared_blocks(m);

	if (init_wl(m) == -1) {
		fprintf(stderr, "Failed to init wayland connection\n");
		return -1;
	}
	return 0;
}

int
run(bar_manager_t *m)
{
	install_signal_handlers();
	int wl_fd = wl_display_get_fd(m->display);

	while (!terminate_requested) {
		if (TRAY && m->tray && m->tray->conn)
			dbus_connection_read_write_dispatch(m->tray->conn, 0);

		int64_t now = current_time_ms();
		int64_t next = now + 3600 * 1000;
		int shared_changed = 0;

		for (long i = 0; i < ASIZE(blocks_cfg); ++i) {
			block_t *b = &m->shared_blocks[i];
			if (!(b->interval_ms))
				continue;
			if (b->type == BLK_TAG ||
				b->type == BLK_LAYOUT ||
				b->type == BLK_TITLE)
				continue;

			int64_t due = b->last_update_ms + b->interval_ms;
			if (due <= now) {
				if (update_block(b, now)) {
					b->version++;
					shared_changed = 1;
				}
				due = now + b->interval_ms;
			}
			if (due < next) next = due;
		}

		if (shared_changed) {
			for (size_t i = 0; i < m->n_bars; ++i)
				m->bars[i]->needs_redraw = 1;
		}

		int timeout = next > now ? (int)(next - now) : 0;

		while (wl_display_prepare_read(m->display) != 0)
			wl_display_dispatch_pending(m->display);
		wl_display_flush(m->display);

		struct pollfd fd = { .fd = wl_fd, .events = POLLIN };
		int ret = poll(&fd, 1, timeout);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			return RUN_WAYLAND_DISCONNECT;
		}
		if (fd.revents & POLLIN) {
			if (wl_display_read_events(m->display) == -1) {
				if (is_disconnect_errno()) {
					fprintf(stderr, "Wayland disconnected (read_events)\n");
					return RUN_WAYLAND_DISCONNECT;
				}
				perror("wl_display_read_events");
				return RUN_WAYLAND_DISCONNECT;
			}
		} else {
			wl_display_cancel_read(m->display);
		}
		if (wl_display_dispatch_pending(m->display) == -1) {
			if (is_disconnect_errno()) {
				fprintf(stderr, "Wayland disconnected (dispatch_pending)\n");
				return RUN_WAYLAND_DISCONNECT;
			}
			perror("wl_display_dispatch_pending");
			return RUN_WAYLAND_DISCONNECT;
		}

		for (size_t i = 0; i < m->n_bars; ++i) {
			bar_t *b = m->bars[i];
			if (b->needs_redraw)
				render_bar(b);
		}
	}
	return RUN_TERM;
}


void
cleanup(bar_manager_t *m)
{
	free_font(m); 
	if (TRAY) {
		m->tray->bar = NULL;
		systray_watcher_stop(m->tray->conn);
		dbus_connection_unref(m->tray->conn);
		dbus_shutdown();

		systray_cleanup(m->tray);
	}
	wl_cleanup(m);
	wl_display_disconnect(m->display);
}
