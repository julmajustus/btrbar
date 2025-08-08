/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tools.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 18:48:24 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/09 01:22:19 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tools.h"

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

void
sleep_ms(long ms)
{
	struct timespec ts;
	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}

