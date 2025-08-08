/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   user_blocks.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/07/24 20:24:15 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/09 01:35:10 by julmajustus      ###   ########.fr       */
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
#include <glob.h>

typedef struct {
	unsigned long long idle;
	unsigned long long total;
} CPUStats;

static uint64_t _last_rx_bytes = 0;
static uint64_t _last_tx_bytes = 0;

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

	CPUStats prev[ncores];
	CPUStats curr[ncores];

	int count1 = read_cpu_stats(prev, ncores);
	if (count1 <= 0) {
		fprintf(stderr, "Failed to read CPU stats (1)\n");
		return;
	}

	struct timespec req = { .tv_sec = 0, .tv_nsec = 100 * 1000000L };
	nanosleep(&req, NULL);

	int count2 = read_cpu_stats(curr, ncores);
	if (count2 <= 0) {
		fprintf(stderr, "Failed to read CPU stats (2)\n");
		return;
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
		if (block->interval_ms == 0) {
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
			block->interval_ms = 0;
			block->label[0]= 0;
		}
	}
}

static uint64_t
sum_glob(const char *pattern)
{
	glob_t gl = {0};
	uint64_t sum = 0;

	if (glob(pattern, 0, NULL, &gl) == 0) {
		for (size_t i = 0; i < gl.gl_pathc; i++) {
			FILE *f = fopen(gl.gl_pathv[i], "r");
			if (!f)
				continue;
			uint64_t v = 0;
			if (fscanf(f, "%llu", (unsigned long long*)&v) == 1)
				sum += v;
			fclose(f);
		}
	}
	globfree(&gl);
	return sum;
}

static void
fmt_iec(uint64_t bytes, char *buf, size_t len)
{
	static const char *units[] = { "B", "K", "M", "G" };
	double   v = (double)bytes;
	int      u = 0;
	while (v >= 1024.0 && u < (int)(sizeof units/sizeof *units)-1) {
		v /= 1024.0;
		u++;
	}
	if (v < 10.0)
		snprintf(buf, len, "%.1f%s", v, units[u]);
	else
		snprintf(buf, len, "%.0f%s", v, units[u]);
}

void
net_usage(char *buf, size_t bufsz)
{
	const char *rx_pat = "/sys/class/net/[ew]*/statistics/rx_bytes";
	const char *tx_pat = "/sys/class/net/[ew]*/statistics/tx_bytes";

	uint64_t cur_rx = sum_glob(rx_pat);
	uint64_t cur_tx = sum_glob(tx_pat);

	uint64_t delta_rx = cur_rx - _last_rx_bytes;
	uint64_t delta_tx = cur_tx - _last_tx_bytes;

	_last_rx_bytes = cur_rx;
	_last_tx_bytes = cur_tx;

	char rx_s[16], tx_s[16];
	fmt_iec(delta_rx, rx_s, sizeof rx_s);
	fmt_iec(delta_tx, tx_s, sizeof tx_s);

	snprintf(buf, bufsz, "  %4s   %4s", rx_s, tx_s);
}

static void
update_vol_block(block_t *block)
{
	char body[MAX_LABEL_LEN];
	if (run_cmd("wpctl get-volume @DEFAULT_AUDIO_SINK@ | tr -d 'Volume: '", body, sizeof(body)) != 0) {
		memcpy(block->label, "err", sizeof("err"));
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
			memcpy(block->label, "Mute", sizeof("Mute"));
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
	char body[MAX_LABEL_LEN];
	// fprintf(stderr, "Check button pressed: %d\n", button);
	if (button == 272) { /* left click*/
		time_t now = time(NULL);
		struct tm tm;

		localtime_r(&now, &tm);

		char buf[3];
		strftime(buf, sizeof buf, "%u", &tm);
		int weekday = atoi(buf);

		int month = tm.tm_mon + 1;

		int year = tm.tm_year + 1900;

		strftime(buf, sizeof buf, "%V", &tm);
		int weeknum = atoi(buf);

		snprintf(block->label, sizeof(block->label), "%d.%d.%d Week: %d", weekday, month, year, weeknum);
	}
	if (button == 274) { /* middle click*/
		run_cmd("notify-send 'Hello world'", body, MAX_LABEL_LEN);
	}
	if (button == 273) { /* right click*/
		run_cmd("notify-send 'Hello world'", body, MAX_LABEL_LEN);
	}
}
