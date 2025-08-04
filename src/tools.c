/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tools.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 18:48:24 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/04 21:02:49 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tools.h"
#include "render.h"

#include <time.h>
void
noop() {}

int64_t
current_time_ms(void)
{
	struct timespec ts;
	if ((clock_gettime(CLOCK_MONOTONIC, &ts) == 0))
		return ts.tv_sec*1000 + ts.tv_nsec/1000000;
	return -1;
}
