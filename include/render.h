/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   render.h                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:22:26 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/02 00:27:41 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RENDER_H
#define RENDER_H


#include "bar.h"
#include "config.h"
#include "tags.h"

int		init_font(bar_t *bar);
void	free_font(bar_t *bar);
int		text_width_px(bar_t *b, const char *text);
void	draw_text(bar_t *b, uint32_t *buf, int buf_w, int buf_h, const char *text, int x, int y, uint32_t color);

void	draw_rect(uint32_t *buf, int buf_w, int buf_h, int x0, int y0, int w, int h, uint32_t color);
void	render_bar(bar_t *bar);

#endif
