/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ipc.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 18:35:46 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/04 22:23:57 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ipc.h"
#include "bar.h"
#include "blocks.h"
#include "config.h"
#include "tools.h"
#include <stdint.h>
#include <string.h>

const struct wl_output_listener output_listener = {
	.geometry    = noop, 
	.mode        = noop, 
	.done        = noop, 
	.scale       = noop, 
	.name        = noop, 
	.description = noop, 
};

const struct zdwl_ipc_manager_v2_listener ipc_manager_listener = {
	.tags   = noop,
	.layout = noop,
};

static void
on_output_tag(void *data, struct zdwl_ipc_output_v2 *out, uint32_t tag, uint32_t state, uint32_t clients, uint32_t focused)
{
	(void)out, (void)focused;
	bar_t *b = data;
	if (TAGS) {
		b->tags[tag].occupied = (clients > 0);
		b->tags[tag].active   = !!(state & ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE);
		b->tags[tag].urgent   = !!(state & ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT);
	}
}

static void
on_output_frame(void *data, struct zdwl_ipc_output_v2 *out)
{
	(void)out;
	bar_t *b = data;
	b->needs_redraw = 1;
}

static void
on_output_layout(void *data, struct zdwl_ipc_output_v2 *out, const char *layout)
{
	(void)out;
	bar_t *b = data;
	if (LAYOUT) {
		for (int i = 0; i < N_BLOCKS; i++) {
			if (b->blocks[i].type == BLK_LAYOUT) {
				if (strncmp(b->blocks[i].label, layout, MAX_LABEL_LEN-1) != 0) {
					strncpy(b->blocks[i].label, layout, MAX_LABEL_LEN-1);
					b->blocks[i].needs_redraw = 1;
					b->needs_redraw = 1;
				}
			}
		}
	}
}

static void
on_output_title(void *data, struct zdwl_ipc_output_v2 *out, const char *title)
{
	(void)out;
	bar_t *b = data;
	if (TITLE) {
		for (int i = 0; i < N_BLOCKS; i++) {
			if (b->blocks[i].type == BLK_TITLE) {
				if (strncmp(b->blocks[i].label, title, MAX_LABEL_LEN-1) != 0) {
					strncpy(b->blocks[i].label, title, MAX_LABEL_LEN-1);
					b->blocks[i].needs_redraw = 1;
					b->needs_redraw = 1;
				}
			}
		}
	}
}

const struct zdwl_ipc_output_v2_listener ipc_output_listener = {
	.toggle_visibility = noop,
	.active = noop,
	.tag   = on_output_tag,
	.layout = noop,
	.title = on_output_title,
	.appid = noop,
	.layout_symbol = on_output_layout,
	.fullscreen = noop,
	.floating = noop,
	.frame = on_output_frame,
};
