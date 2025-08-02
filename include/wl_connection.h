/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   wl_connection.h                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 17:27:10 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/24 19:22:44 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WL_CONNECTION_H
#define WL_CONNECTION_H

#define _POSIX_C_SOURCE 200809L
#include "bar.h"

int		init_wl(bar_t *bar);
void	wl_cleanup(bar_t *bar);

#endif
