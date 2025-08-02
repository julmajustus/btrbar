/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   systray.h                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/25 15:18:07 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/01 00:35:06 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SYSTRAY_H
#define SYSTRAY_H

#include <wayland-client.h>
#include "bar.h"


int		systray_init(bar_t *b);
void	systray_handle(bar_t *b);
void	tray_handle_item_added(systray_t *tray, const char *service, const char *object_path);
void	tray_handle_item_removed(systray_t *tray, const char *service);

int		systray_handle_popup_click(bar_t *bar);
void	tray_get_menu_layout(tray_item_t *it, systray_t *tray);
void	tray_left_click(tray_item_t *it);
void	systray_render_popup(bar_t *bar);
void	popup_menu_hide(systray_t *tray);
void	free_menu_tree(MenuItem **items, size_t n);
void	systray_cleanup(bar_t *b);

#endif
