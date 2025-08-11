/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   font_atlas.h                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/08/11 19:40:52 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/11 21:04:41 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FONT_ATLAS_H
#define FONT_ATLAS_H
#include "stb_truetype.h"
#include <stdint.h>

#define ATLAS_W		1024
#define ATLAS_H		512
#define ATLAS_PAD	1
#define ATLAS_CAP	1024

typedef struct {
	uint32_t		glyph;
	uint16_t		x, y;
	uint16_t		w, h;
	int16_t			x0, y0;
	uint16_t		adv;
	uint8_t			used;
}	glyph_entry;

typedef struct {
	unsigned char	*pixels;

	glyph_entry		*table;
	uint32_t		cap;
	uint32_t		count;

	int				pen_x, pen_y, row_h;
} glyph_atlas;

glyph_entry	  *ensure_glyph_cached(glyph_atlas *a, stbtt_fontinfo *font, float scale, uint32_t glyph_id);

#endif
