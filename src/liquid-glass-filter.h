#pragma once

#include <obs-module.h>

extern struct obs_source_info liquid_glass_filter_info;

/* Shared Zappify state poller -- one background thread serving every filter
 * instance, started/stopped once from plugin-main.c. See the implementation
 * comment on struct zappify_state in liquid-glass-filter.c. */
void glass_zappify_poller_start(void);
void glass_zappify_poller_stop(void);
