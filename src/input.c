/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   input.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 18:57:57 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/11 22:03:21 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "input.h"
#include "bar.h"
#include "blocks.h"
#include "config.h"
#include "ipc.h"
#include "render.h"
#include "tools.h"
#include "systray.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-client-core.h>

static void
pointer_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *surf, wl_fixed_t sx, wl_fixed_t sy)
{
	(void)p, (void)serial, (void)sx, (void)sy;

	bar_t *b = data;
	if (TRAY && surf == b->tray->menu.surf)
		b->tray->menu.is_inside_menu = 1;
	if (b->surface == surf)
		b->is_focused = 1;
}

static void
pointer_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *surf)
{
	(void)p, (void)serial;

	bar_t *b = data;
	if (TRAY && surf == b->tray->menu.surf)
		popup_menu_hide(b->tray);
	if (b->surface == surf)
		b->is_focused = 0;
}

static void
pointer_motion(void *data, struct wl_pointer *p, uint32_t t, wl_fixed_t sx, wl_fixed_t sy)
{
	(void)p, (void)t, (void)sy;
	bar_t *b = data;

	b->last_x = wl_fixed_to_int(sx);
	b->last_y = wl_fixed_to_int(sy);

	if (TRAY) {
		PopupMenu *m = &b->tray->menu;
		if (!m->active)
			return;
		int line_h = F_SIZE + 4;
		int idx = b->last_y / line_h;
		if (idx < 0)
			idx = 0;

		if (idx >= (int)m->n_items)
			idx = m->n_items - 1;

		if (idx != m->highlighted && b->tray->menu.is_inside_menu != 0) {
			m->highlighted = idx;
			systray_render_popup(b);
		}
	}
}

static void
pointer_button(void *data, struct wl_pointer *p, uint32_t time, uint32_t serial, uint32_t button, uint32_t state)
{
	if (state != WL_POINTER_BUTTON_STATE_PRESSED)
		return;
	(void)p, (void)time, (void)serial;
	bar_t *b = data;
	// Limit to only one listerner call if multiple outputs present
	if (b->is_clicked)
		return;
	b->is_clicked = 1;
	if (TRAY) {
		if (b->tray->menu.active) {
			if (systray_handle_popup_click(b)) {
				b->is_clicked = 0;
				return;
			}
		}

		block_inst_t *bi = NULL;
		for (uint8_t k = 0; k < ASIZE(blocks_cfg); k++) {
			bi = &b->block_inst[k];
			if (bi->block->type == BLK_TRAY)
				break;
		}

		for (uint8_t j = 0; j < b->tray->n_items; j++) {
			tray_span_t *span = &bi->local.tray.spans[j];
			if (b->last_x >= span->x0 && b->last_x < span->x1 && !b->tray->menu.active) {
				tray_item_t *it = &b->tray->items[span->item_index];
				if (button == 272) // left click
					tray_left_click(it);
				else if (button == 273) {  // right click
					tray_get_menu_layout(it, b->tray);
				}
				b->needs_redraw = 1;
				b->is_clicked = 0;
				return;
			}
		}
	}

	if (!b->is_focused)
		return;

	if (TAGS) {
		for (uint8_t i = 0; i < ASIZE(tag_icons); i++) {
			if (b->last_x >= b->tags[i].x0 && b->last_x < b->tags[i].x1) {
				uint32_t mask = 1u << i;
				zdwl_ipc_output_v2_set_tags(b->ipc_out, mask, 0);
				for (long i = 0; i < ASIZE(blocks_cfg); i++) {
					block_t *block = b->block_inst[i].block;
					if (block->type == BLK_TAG)
						block->version++;
					b->needs_redraw = 1;
				}
				b->is_clicked = 0;
				return;
			}
		}
	}
	handle_click(b->block_inst, b->last_x, button);
	b->needs_redraw = 1;
	b->is_clicked = 0;
}

static void
pointer_axis(void *data, struct wl_pointer *p, uint32_t t, uint32_t axis, wl_fixed_t value)
{
	(void)p, (void)t;
	bar_t *b = data;
	if (!b->is_focused)
		return;
	int amt = wl_fixed_to_int(value);
	handle_scroll(b->block_inst, b->last_x, axis, amt);
	b->needs_redraw = 1;
}

const struct wl_pointer_listener pointer_listener = {
	.enter  = pointer_enter,
	.leave  = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis   = pointer_axis,
	.frame  = noop,
	.axis_source  = noop,
	.axis_stop  = noop,
	.axis_discrete  = noop,
};
