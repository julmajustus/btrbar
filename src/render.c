/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:23:38 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/11 21:02:20 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "blocks.h"
#include <stdint.h>
#include "render.h"
#include "bar.h"
#include "config.h"
#include "systray.h"

#include <stdio.h>
#include <stdlib.h>


static void
frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	(void)time;
	bar_t *b = data;
	if (cb)
		wl_callback_destroy(cb);
	b->frame_cb = NULL;
}

static const struct wl_callback_listener frame_listener = {
	.done = frame_done
};

static inline void fill_row(uint32_t *p, int n, uint32_t color)
{
	for (int i = 0; i < n; ++i)
		p[i] = color;
}

void
draw_rect(uint32_t *buf, int buf_w, int buf_h, int x0, int y0, int w, int h, uint32_t color)
{
	// fprintf(stderr, "draw_rect(%d,%d,%d,%d,0x%08x)\n", x0, y0, w, h, color);
	if (x0 < 0) {
		w += x0;
		x0 = 0;
	}
	if (y0 < 0) {
		h += y0;
		y0 = 0;
	}
	if (x0 + w > buf_w)
		w = buf_w  - x0;
	if (y0 + h > buf_h)
		h = buf_h  - y0;
	if (w <= 0 || h <= 0)
		return;

	uint32_t *r_buf = buf + y0 * buf_w + x0;
	for (int r = 0; r < h; r++) {
		fill_row(r_buf, w, color);
		r_buf += buf_w;
	}
}

static inline uint32_t
u8lerp(uint32_t d, uint32_t s, uint32_t a)
{
    return d + (((s - d) * a) >> 8);
}

static inline void
blend_glyph(uint32_t *argb_buf, int buf_w, int buf_h,
			int x, int y, int w, int h, const unsigned char *bitmap,
			int stride, uint32_t color)
{
	uint8_t src_a = (color >> 24) & 0xFF;
	uint8_t src_r = (color >> 16) & 0xFF;
	uint8_t src_g = (color >> 8 ) & 0xFF;
	uint8_t src_b = (color >> 0 ) & 0xFF;

	if (x < 0) {
		w += x;
		bitmap += -x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		bitmap += (-y) * stride;
		y = 0;
	}
	if (x + w > buf_w)
		w = buf_w - x;
	if (y + h > buf_h)
		h = buf_h - y;
	if (w <= 0 || h <= 0)
		return;

	for (int row = 0; row < h; ++row) {
		uint32_t *dst = argb_buf + (y + row) * buf_w + x;
		const unsigned char *ms = bitmap + row * stride;
		for (int col = 0; col < w; ++col) {
			uint32_t a = (ms[col] * src_a) >> 8;
			if (!a)
				continue;
			uint32_t d = dst[col];
			uint32_t dr = (d >> 16) & 0xFF,
					dg = (d >> 8) & 0xFF,
					db = d & 0xFF,
					da = (d >> 24) & 0xFF;
			dr = u8lerp(dr, src_r, a);
			dg = u8lerp(dg, src_g, a);
			db = u8lerp(db, src_b, a);
			da = u8lerp(da, src_a, a);
			dst[col] = (da << 24) | (dr << 16) | (dg << 8) | db;
		}
	}
}

static uint32_t
next_utf8(const char **s)
{
	const unsigned char *us = (const unsigned char *)(*s);
	uint32_t cp;
	if (us[0] < 0x80) {
		cp = us[0];
		*s += 1;
	}
	else if ((us[0] & 0xE0) == 0xC0) {
		cp = ((us[0] & 0x1F) << 6) | (us[1] & 0x3F);
		*s += 2;
	}
	else if ((us[0] & 0xF0) == 0xE0) {
		cp = ((us[0] & 0x0F) << 12) | ((us[1] & 0x3F) << 6) | (us[2] & 0x3F);
		*s += 3;
	}
	else if ((us[0] & 0xF8) == 0xF0) {
		cp = ((us[0] & 0x07) << 18) | ((us[1] & 0x3F) << 12) | ((us[2] & 0x3F) << 6) | (us[3] & 0x3F);
		*s += 4;
	}
	else {
		cp = 0;
		*s += 1; }
	return cp;
}

void
draw_text(bar_t *b, uint32_t *buf, int buf_w, int buf_h,
		  const char *text, int x, int y, uint32_t color)
{
	// fprintf(stderr, "draw_text('%s',%d,%d,0x%08x)\n", text, x, y, color);
	int pen_x = x;
	const char *s = text;
	int prev_glyph = 0;
	while (*s) {
		uint32_t cp = next_utf8(&s);
		int glyph = stbtt_FindGlyphIndex(&b->font, cp);
		if (prev_glyph) {
			int kern = stbtt_GetGlyphKernAdvance(&b->font, prev_glyph, glyph);
			pen_x += (int)(kern * b->font_scale);
		}
		prev_glyph = glyph;

		glyph_entry *e = ensure_glyph_cached(&b->bar_manager->atlas, &b->font, b->font_scale, (uint32_t)glyph);
		if (!e)
			continue;

		if (e->w && e->h) {
			int dst_x = pen_x + e->x0;
			int dst_y = y + b->font_baseline + e->y0;
			const unsigned char *bitmap = b->bar_manager->atlas.pixels + e->y * ATLAS_W + e->x;

			blend_glyph(buf, buf_w, buf_h, dst_x, dst_y, e->w, e->h, bitmap, ATLAS_W, color);
		}
		pen_x += e->adv;
	}
}

int
text_width_px(bar_t *b, const char *text)
{
	if (!text)
		return 0;
	uint32_t width = 0;
	const char *s = text;
	int prev_glyph = 0;
	while (*s) {
		uint32_t cp = next_utf8(&s);
		int glyph = stbtt_FindGlyphIndex(&b->font, cp);
        if (prev_glyph) {
            int kern = stbtt_GetGlyphKernAdvance(&b->font, prev_glyph, glyph);
            width += (int)(kern * b->font_scale);
        }
        prev_glyph = glyph;

        glyph_entry *e = ensure_glyph_cached(&b->bar_manager->atlas, &b->font, b->font_scale, (uint32_t)glyph);
        if (e)
			width += e->adv;
    }
    return width;
}

static int
block_width_px(bar_t *b, const block_inst_t *bi)
{
	const block_t *block = bi->block;

	if (TAGS && block->type == BLK_TAG) {
		int tx = 0;
		for (uint8_t i = 0; i < ASIZE(tag_icons); i++) {
			int txt_w = text_width_px(b, b->tags[i].icon);
			tx += txt_w + TAG_ICON_PADDING;
		}
		return tx;
	}
	if (TRAY && block->type == BLK_TRAY) {
		int icons = b->tray ? (int)b->tray->n_items : 0;
		return icons ? icons * b->height + icons * TRAY_ICON_PADDING : 0;
	}

	int prefix_w = text_width_px(b, block->prefix ? block->prefix : "");
	int label_w  = text_width_px(b, block->label);
	return prefix_w + label_w + 4 + BLOCK_PADDING * 2;
}

static void draw_tags(bar_t *b, int x, int y)
{
	int tx = x;
	for (uint8_t i = 0; i < ASIZE(tag_icons); i++) {
		tag_t *t = &b->tags[i];
		uint32_t txt_w = text_width_px(b, t->icon);
		t->x0 = tx;
		t->x1 = tx + txt_w + TAG_ICON_PADDING;

		uint32_t fg = t->active   ? TAG_FG_ACTIVE
			: t->urgent  ? TAG_FG_URGENT
			: t->occupied? TAG_FG_OCCUPIED
			: TAG_FG_EMPTY;
		draw_rect(b->argb_buf, b->width, b->height,
			tx, 0, t->x1 - tx, b->height, TAG_BG_COLOR);
		draw_text(b, b->argb_buf, b->width, b->height, t->icon, tx, y, fg);
		tx += txt_w + TAG_ICON_PADDING;
	}
}

static inline void
blend_span(uint32_t *restrict dst, const uint32_t *restrict src, int w)
{
	for (int x = 0; x < w; ++x) {
		uint32_t sp = src[x];
		uint32_t src_a = sp >> 24;
		if (!src_a)
			continue;

		uint32_t dp = dst[x];

		uint32_t src_r = (sp >> 16) & 0xFF;
		uint32_t src_g = (sp >> 8) & 0xFF;
		uint32_t src_b = (sp) & 0xFF;

		uint32_t dr = (dp >> 16) & 0xFF;
		uint32_t dg = (dp >> 8) & 0xFF;
		uint32_t db = (dp) & 0xFF;

		if (src_a == 255) {
			dst[x] = (0xFFu << 24) | (src_r << 16) | (src_g << 8) | src_b;
			continue;
		}

		dr = u8lerp(dr, src_r, src_a);
		dg = u8lerp(dg, src_g, src_a);
		db = u8lerp(db, src_b, src_a);

		dst[x] = (0xFFu << 24) | (dr << 16) | (dg << 8) | db;
	}
}

static void
draw_tray(bar_t *b, block_inst_t *bi, int x0)
{
	if (!TRAY || !b->tray)
		return;
	bi->local.tray.n = 0;

	int x = x0;

	for (uint8_t i = 0; i < b->tray->n_items; i++) {
		tray_item_t *it = &b->tray->items[i];
		if (!it->pixels)
			continue;

		int iw = (int)it->width;
		int ih = (int)it->height;

		int draw_x = x;
		int src_x_off = 0;
		if (draw_x < 0) {
			src_x_off = -draw_x;
			iw -= src_x_off;
			draw_x = 0;
		}
		if (draw_x >= (int)b->width || iw <= 0)
			goto advance;
		if (draw_x + iw > (int)b->width)
			iw = (int)b->width - draw_x;

		int draw_h = ih;
		int max_h = (int)b->height;
		if (draw_h > max_h)
			draw_h = max_h;
		if (draw_h > 0 && iw > 0) {
			for (int yy = 0; yy < draw_h; ++yy) {
				uint32_t *dst = b->argb_buf + yy * (int)b->width + draw_x;
				const uint32_t *src = it->pixels + yy * (int)it->width + src_x_off;
				blend_span(dst, src, iw);
			}
		}

	advance:
		if (bi->local.tray.n < MAX_TRAY_ITEMS) {
			tray_span_t *span = &bi->local.tray.spans[bi->local.tray.n++];
			span->x0 = x;
			x += (int)it->width + TRAY_ICON_PADDING;
			span->x1 = x;
			span->item_index = i;
		}
		else 
			x += (int)it->width + TRAY_ICON_PADDING;
	}
}

void
render_bar(bar_t *b)
{
	if (!b->needs_redraw)
		return;

	b->argb_buf = b->argb_bufs[b->cur_buf];

	draw_rect(b->argb_buf, b->width, b->height, 0, 0, b->width, b->height, BG_COLOR);

  int y = (b->height - b->font_baseline) / 2;  //Vertical centering

	for (uint8_t i = 0; i < ASIZE(blocks_cfg); ++i) {
		block_inst_t *bi = &b->block_inst[i];
		if (bi->seen_version != bi->block->version) {
			bi->seen_version = bi->block->version;
		}
	}

	for (int align = ALIGN_LEFT; align <= ALIGN_RIGHT; align++) {
		int right = (align == ALIGN_RIGHT);
		int x = right ? (b->width - EDGE_PADDING)
			: (align == ALIGN_LEFT ? EDGE_PADDING : b->width / 2);
		int start = right ? ASIZE(blocks_cfg) - 1 : 0;
		int end   = right ? -1 : ASIZE(blocks_cfg);
		int step  = right ? -1 : 1;

		for (int i = start; i != end; i += step) {
			block_inst_t *bi = &b->block_inst[i];
			if (bi->block->align != (align_t)align)
				continue;

			int w = block_width_px(b, bi);
			int new_x0 = right ? x - w : x;
			int new_x1 = right ? x : x + w;

			bi->old_x0 = bi->x0; bi->old_x1 = bi->x1;
			bi->x0 = new_x0; bi->x1 = new_x1;
			bi->current_width = w;

			draw_rect(b->argb_buf, b->width, b->height,
			 bi->x0, 0, w, b->height, bi->block->bg_color);

			if (TAGS && bi->block->type == BLK_TAG)
				draw_tags(b, bi->x0, y);
			else if (TRAY && bi->block->type == BLK_TRAY)
				draw_tray(b, bi, bi->x0 + BLOCK_PADDING);
			else {
				uint32_t tx = bi->x0 + BLOCK_PADDING;
				if (bi->block->prefix) {
					draw_text(b, b->argb_buf, b->width, b->height,
			   bi->block->prefix, tx + 2, y, bi->block->pfx_color);
					tx += text_width_px(b, bi->block->prefix);
				}
				draw_text(b, b->argb_buf, b->width, b->height,
			  bi->block->label, tx + 2, y, bi->block->fg_color);
			}

			x = right ? new_x0 : new_x1;
		}
	}

	wl_surface_attach(b->surface, b->buffers[b->cur_buf], 0, 0);
	wl_surface_damage(b->surface, 0, 0, INT32_MAX, INT32_MAX);
	if (b->frame_cb)
		wl_callback_destroy(b->frame_cb);
	b->frame_cb = wl_surface_frame(b->surface);
	wl_callback_add_listener(b->frame_cb, &frame_listener, b);
	wl_surface_commit(b->surface);

	b->cur_buf ^= 1;
	b->needs_redraw ^= 1;
}
