/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:14:52 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/09 01:23:00 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#define _POSIX_C_SOURCE 200809L
#include "bar.h"
#include "tools.h"

#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
	while (1) {
		bar_manager_t *m = calloc(1, sizeof(*m));
		if (!m)
			return 1;

		if (init_manager(m) == -1) {
			fprintf(stderr, "init_manager failed, retrying…\n");
			free(m);
			sleep_ms(500);
			continue;
		}

		int rc = run(m);

		cleanup(m);
		free(m);

		if (rc == RUN_TERM)
			break;

		sleep_ms(500);
	}
	return 0;
}
