/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   user_blocks.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:24:15 by julmajustus       #+#    #+#             */
/*   Updated: 2025/07/29 23:21:27 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tools.h"
#define _POSIX_C_SOURCE 200809L
#include "blocks.h"
#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

typedef struct {
	unsigned long long idle;
	unsigned long long total;
} CPUStats;

static int
read_cpu_stats(CPUStats stats[], int max_cpus)
{
	FILE *f = fopen("/proc/stat", "r");
	if (!f)
		return -1;

	char line[256];
	int idx = 0;
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "cpu", 3) != 0)
			break;
		if (!isdigit((unsigned char)line[3]))
			continue;

		unsigned int cpu_id;
		unsigned long long user, nice, syst, idle, iowait, irq, softirq, steal;
		int matched = sscanf(line,
					   "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu",
					   &cpu_id, &user, &nice, &syst, &idle,
					   &iowait, &irq, &softirq, &steal);

		if (matched < 5)
			continue;
		unsigned long long idle_all  = idle + (matched >= 6 ? iowait : 0);
		unsigned long long total_all = user + nice + syst + idle
			+ (matched >= 6 ? iowait : 0)
			+ (matched >= 7 ? irq     : 0)
			+ (matched >= 8 ? softirq : 0)
			+ (matched >= 9 ? steal   : 0);

		if (idx < max_cpus) {
			stats[idx].idle  = idle_all;
			stats[idx].total = total_all;
			idx++;
		}
		else 
		break;
	}

	fclose(f);
	return idx;
}

void
cpu_usage(char *buf, size_t bufsz)
{
	long ncores = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncores <= 0) {
		fprintf(stderr, "Could not get number of CPUs\n");
		return;
	}

	CPUStats *prev = malloc(sizeof(CPUStats) * ncores);
	CPUStats *curr = malloc(sizeof(CPUStats) * ncores);
	if (!prev || !curr) {
		perror("malloc");
		free(prev);
		free(curr);
		return;
	}

	int count1 = read_cpu_stats(prev, ncores);
	if (count1 <= 0) {
		fprintf(stderr, "Failed to read CPU stats (1)\n");
		goto cleanup;
	}

	struct timespec req = { .tv_sec = 0, .tv_nsec = 100 * 1000000L };
	nanosleep(&req, NULL);

	int count2 = read_cpu_stats(curr, ncores);
	if (count2 <= 0) {
		fprintf(stderr, "Failed to read CPU stats (2)\n");
		goto cleanup;
	}

	int count = count1 < count2 ? count1 : count2;
	double sum_usage = 0.0;
	for (int i = 0; i < count; i++) {
		unsigned long long idle_delta  = curr[i].idle  - prev[i].idle;
		unsigned long long total_delta = curr[i].total - prev[i].total;
		if (total_delta == 0)
			continue;
		double usage = (double)(total_delta - idle_delta) / total_delta;
		sum_usage += usage * 100.0;
	}

	double avg = sum_usage / count;
	snprintf(buf, bufsz, "%.2f%%", avg);
cleanup:
	free(prev);
	free(curr);
}

static int
get_memory_gib(double *used_gb, double *total_gb)
{
	FILE *f = fopen("/proc/meminfo", "r");
	if (!f)
		return -1;

	long total_kb     = 0;
	long available_kb = 0;
	long free_kb      = 0;
	long buffers_kb   = 0;
	long cached_kb    = 0;

	char line[256];
	while (fgets(line, sizeof(line), f)) {
		if (sscanf(line, "MemTotal: %ld kB", &total_kb) == 1)
			continue;
		if (sscanf(line, "MemAvailable: %ld kB", &available_kb) == 1)
			break;
		if (sscanf(line, "MemFree: %ld kB", &free_kb) == 1)
			continue;
		if (sscanf(line, "Buffers: %ld kB", &buffers_kb) == 1)
			continue;
		if (sscanf(line, "Cached: %ld kB", &cached_kb) == 1)
			continue;
	}
	fclose(f);

	if (total_kb == 0)
		return -1;

	if (available_kb == 0) {
		long sum = free_kb + buffers_kb + cached_kb;
		if (sum > 0)
			available_kb = sum;
		else
			return -1;
	}

	long used_kb = total_kb - available_kb;

	*total_gb = (double)total_kb / (1024.0 * 1024.0);
	*used_gb  = (double)used_kb  / (1024.0 * 1024.0);
	return 0;
}

void
mem_usage_simple(char *buf, size_t bufsz)
{
	double used, total;
	if (get_memory_gib(&used, &total) == 0)
		snprintf(buf, bufsz, "%.2fG", used);
	else
		fprintf(stderr, "Error: could not read /proc/meminfo\n");
}

void
mem_usage_full(char *buf, size_t bufsz)
{
	double used, total;
	if (get_memory_gib(&used, &total) == 0) {
		snprintf(buf, bufsz, "%.2fG / %.2f G", used, total);
	} else {
		fprintf(stderr, "Error: could not read /proc/meminfo\n");
	}
}

void
_clock(char *buf, size_t bufsz)
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	snprintf(buf, bufsz,  "%02d:%02d", tm.tm_hour, tm.tm_min);
}

void
power_consumption_click(block_t *block, int button)
{
	(void)block;
	char body[MAX_LABEL_LEN];
	if (button == 272) {
		if (!block->label[0]) {
			block->pfx_color = L_GREEN;
			if (run_cmd(block->cmd, body, sizeof(body)) != 0) {
				strncpy(body, "err", sizeof(body));
				body[sizeof(body)-1] = '\0';
			}
			block->interval_ms = 7*1000;
			update_block(block, current_time_ms());
		}
		else {
			block->pfx_color = 0xffff22aa;
			block->interval_ms = -1;
			block->label[0]= 0;
		}
	}
}


static void
update_vol_block(block_t *block)
{
	char body[MAX_LABEL_LEN];
	if (run_cmd("wpctl get-volume @DEFAULT_AUDIO_SINK@ | tr -d 'Volume: '", body, sizeof(body)) != 0) {
		strncpy(block->label, "err", sizeof(block->label));
	} else {
		double vol = atof(body) * 100.0;
		snprintf(block->label, sizeof(block->label), "%.0f%%", vol);
	}

	double vol = atof(block->label) / 1.0;
	if (vol >= 30) {
		block->prefix = "  ";
	} else if (vol >= 15) {
		block->prefix = " ";
	} else {
		block->prefix = " ";
	}
}

void
vol_click(block_t *block, int button)
{
	(void)block;
	if (button == 272) {
		run_cmd("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle", NULL, 0);
		if (block->label[strlen(block->label) - 1] == '%') {
			block->pfx_color = PURPLE;
			block->prefix = " ";
			strncpy(block->label, "Mute", sizeof(block->label));
		}
		else {
			block->pfx_color = L_GREEN;
			update_vol_block(block);
		}
	}
}

void
vol_scroll(block_t *block, int amt)
{

	if (block->label[strlen(block->label) - 1] == '%') {
		if (amt > 0) {
			run_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 1%-", NULL, 0);
		} else if (amt < 0) {
			run_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 1%+", NULL, 0);
		}
		update_vol_block(block);
	}
}

void
clock_click(block_t *block, int button)
{
	(void)block;
	char body[MAX_LABEL_LEN];
	fprintf(stderr, "Check button pressed: %d\n", button);
	if (button == 272) {
		run_cmd("notify-send 'Hello world'", body, MAX_LABEL_LEN);
	}
	if (button == 274) {
		run_cmd("notify-send 'Hello world'", body, MAX_LABEL_LEN);
	}
	if (button == 273) {
		run_cmd("notify-send 'Hello world'", body, MAX_LABEL_LEN);
	}
}
