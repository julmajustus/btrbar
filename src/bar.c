/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bar.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 10:13:11 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/04 21:03:53 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#define _POSIX_C_SOURCE 200809L
#include "bar.h"
#include "blocks.h"
#include "render.h"
#include "wl_connection.h"
#include "tools.h"
#include "systray.h"

#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

static volatile sig_atomic_t terminate_requested = 0;

static void
on_signal(int sig)
{
    (void)sig;
    terminate_requested = 1;
}

void
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

int
bar_init(bar_t *b)
{
	memset(b, 0, sizeof *b);
	b->needs_redraw = 1;

	b->blocks = calloc(N_BLOCKS, sizeof(*b->blocks));
	if (!b->blocks || init_blocks(b->blocks) == -1) {
		fprintf(stderr, "Blocks init failed\n");
		return -1;
	}

	if (TAGS) {
		b->tags = calloc(N_TAGS, sizeof(*b->tags));
		if (!b->tags || init_tags(b->tags) == -1) {
			fprintf(stderr, "Tag init failed\n");
			return -1;
		}
	}

	if (init_wl(b) == -1) {
		fprintf(stderr, "Failed to init wayland connection\n");
		return -1;
	}

	if (init_font(b) == -1) {
		return -1;
		fprintf(stderr, "Failed to init font\n");
	}

	if (!TRAY)
		return 0;
	
	if (systray_init(b) == -1) {
		return -1;
		fprintf(stderr, "Failed to init systray\n");
	}
	for (int i = 0; i < 2; i++) {
		b->argb_buf = b->argb_bufs[i];
		draw_rect(b->argb_buf, b->width, b->height, 0, 0, b->width, b->height, BG_COLOR);
	}
	return 0;
}

void
run(bar_t *b)
{
	install_signal_handlers();
	int wl_fd = wl_display_get_fd(b->display);

	while (!terminate_requested) {

		dbus_connection_read_write_dispatch(b->tray->conn, 0);
		int64_t now = current_time_ms();
		int64_t next = now + 3600 * 1000;

		for (int i = 0; i < N_BLOCKS; i++) {
			block_t *blk = &b->blocks[i];
			if (blk->type == BLK_TAG ||
				blk->type == BLK_LAYOUT ||
				blk->type == BLK_TITLE ||
				blk->type == BLK_TRAY)
				continue;

			int64_t due = blk->last_update_ms + blk->interval_ms;
			if (due <= now) {
				if (update_block(blk, now))
					b->needs_redraw = 1;
			}
			if (due < next)
				next = due;
		}

		int timeout = next > now ? (int)(next - now) : 0;

		while (wl_display_prepare_read(b->display) != 0)
			wl_display_dispatch_pending(b->display);
		wl_display_flush(b->display);

		struct pollfd fd = {.fd = wl_fd, .events = POLLIN};
		int ret = poll(&fd, 1, timeout);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			break;
		}

		if (fd.revents & POLLIN) {
			if (wl_display_read_events(b->display) == -1) {
				perror("wl_display_read_events");
				break;
			}
		}
		else
			wl_display_cancel_read(b->display);

		wl_display_dispatch_pending(b->display);

		if (b->needs_redraw)
			render_bar(b);
	}
}

void
cleanup(bar_t *b)
{
	free_font(b);
	wl_cleanup(b);
	if (TAGS)
		free_tags(b->tags);
	free_blocks(b->blocks);
	if (TRAY)
		systray_cleanup(b);
	wl_display_disconnect(b->display);
}
