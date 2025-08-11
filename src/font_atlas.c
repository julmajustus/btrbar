/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   font_atlas.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/11 19:40:42 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/11 21:08:54 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#define STB_TRUETYPE_IMPLEMENTATION
#include "bar.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static inline uint32_t
hash_u32(uint32_t x)
{
	x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
	return x;
}

static void
atlas_clear(glyph_atlas *a)
{
	memset(a->pixels, 0, ATLAS_W * ATLAS_H);
	memset(a->table,  0, a->cap * sizeof(glyph_entry));
	a->count = 0;
	a->pen_x = 0;
	a->pen_y = 0;
	a->row_h = 0;
}

static glyph_entry *
atlas_lookup(glyph_atlas *a, uint32_t glyph)
{
	uint32_t cap = a->cap;
	if (cap == 0)
		return NULL;
	uint32_t i = hash_u32(glyph) & (cap - 1);
	for (uint32_t step = 0; step < cap; ++step, i = (i + 1) & (cap - 1)) {
		glyph_entry *e = &a->table[i];
		if (!e->used)
			return NULL;
		if (e->glyph == glyph)
			return e;
	}
	return NULL;
}

static glyph_entry *
atlas_insert(glyph_atlas *a, const glyph_entry *src)
{
	uint32_t cap = a->cap;
	if (cap == 0)
		return NULL;
	if (a->count * 2 >= cap) {
		atlas_clear(a);
	}

	uint32_t i = hash_u32(src->glyph) & (cap - 1);
	for (;;) {
		glyph_entry *e = &a->table[i];
		if (!e->used) {
			*e = *src;
			e->used = 1;
			a->count++;
			return e;
		}
		if (e->glyph == src->glyph) {
			*e = *src;
			e->used = 1;
			return e;
		}
		i = (i + 1) & (cap - 1);
	}
}

static int
atlas_alloc_rect(glyph_atlas *a, int w, int h, int *out_x, int *out_y)
{
	int W = ATLAS_W, H = ATLAS_H;
	if (w + ATLAS_PAD > W || h + ATLAS_PAD > H)
		return -1;

	if (a->pen_x + w + ATLAS_PAD > W) {
		a->pen_x = 0;
		a->pen_y += a->row_h + ATLAS_PAD;
		a->row_h = 0;
	}
	if (a->pen_y + h + ATLAS_PAD > H)
		return -1;

	*out_x = a->pen_x;
	*out_y = a->pen_y;
	a->pen_x += w + ATLAS_PAD;
	if (h > a->row_h)
		a->row_h = h;
	return 0;
}

glyph_entry *
ensure_glyph_cached(glyph_atlas *a, stbtt_fontinfo *font, float scale, uint32_t glyph_id)
{
	glyph_entry *ep = atlas_lookup(a, glyph_id);
	if (ep)
		return ep;

	int x0, y0, x1, y1;
	stbtt_GetGlyphBitmapBox(font, glyph_id, scale, scale, &x0, &y0, &x1, &y1);
	int w = x1 - x0, h = y1 - y0;
	if (w <= 0 || h <= 0) {
		int adv, lsb; stbtt_GetGlyphHMetrics(font, glyph_id, &adv, &lsb);
		glyph_entry tmp = {
			.glyph = glyph_id,
			.x=0,
			.y=0,
			.w=0,
			.h=0,
			.x0=x0,
			.y0=y0,
			.adv=(uint16_t)(adv * scale),
			.used=1
		};
		return atlas_insert(a, &tmp);
	}

	int ax, ay;
	if (atlas_alloc_rect(a, w, h, &ax, &ay) != 0) {
		atlas_clear(a);
		if (atlas_alloc_rect(a, w, h, &ax, &ay) != 0)
			return NULL;
	}

	unsigned char *dst = a->pixels + ay * ATLAS_W + ax;
	stbtt_MakeGlyphBitmap(font, dst, w, h, ATLAS_W, scale, scale, glyph_id);

	int adv, lsb;
	stbtt_GetGlyphHMetrics(font, glyph_id, &adv, &lsb);

	glyph_entry e = {
		.glyph = glyph_id,
		.x = (uint16_t)ax, .y = (uint16_t)ay,
		.w = (uint16_t)w,  .h = (uint16_t)h,
		.x0 = (int16_t)x0, .y0 = (int16_t)y0,
		.adv = (uint16_t)(adv * scale),
		.used = 1
	};
	return atlas_insert(a, &e);
}
