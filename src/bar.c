/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bar.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 10:13:11 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/02 00:08:51 by julmajustus      ###   ########.fr       */
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
	return 0;
}

void
run(bar_t *b)
{
	install_signal_handlers();
	while (!terminate_requested) {
		wl_display_dispatch_pending(b->display);

		int64_t now = current_time_ms();
		if (now == -1) {
			return;
		}

		// fprintf(stderr, "Check current time: %ld\n", now);
		int64_t next_dead = now + 3600*1000;
		for (int i = 0; i < N_BLOCKS; i++) {
			if (b->blocks[i].type == BLK_TAG ||
				b->blocks[i].type == BLK_LAYOUT ||
				b->blocks[i].type == BLK_TITLE ||
				b->blocks[i].type == BLK_TRAY)
				continue;
			int64_t due = b->blocks[i].last_update_ms + b->blocks[i].interval_ms;
			if (due <= now) {
				update_block(&b->blocks[i], now);
				// fprintf(stderr, "Updating block: %d\n", i);
				b->needs_redraw = 1;
			}
			if (due < next_dead)
				next_dead = due;
		}

		if (b->needs_redraw) {
			render_bar(b);
			b->needs_redraw = 0;
		}

		int timeout = (int)(next_dead - now);
		if (timeout < 0) {
			timeout = 0;
		}

		wl_display_flush(b->display);

		systray_handle(b);

		struct pollfd p = { 
			.fd = wl_display_get_fd(b->display), 
			.events = POLLIN 
		};

		int ret = poll(&p, 1, timeout);
		if (ret < 0) {
			if (errno == EINTR)
				break;
			perror("poll");
			break;
		}
		if (ret > 0 && (p.revents & POLLIN)) {
			if (wl_display_dispatch(b->display) == -1) {
				perror("wl_display_dispatch");
				break;
			}
		}
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
