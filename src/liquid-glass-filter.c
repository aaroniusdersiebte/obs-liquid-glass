/*
Liquid Glass filter for OBS
Copyright (C) 2026 aaorn aaronius2109@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "liquid-glass-filter.h"
#include <plugin-support.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/threading.h>
#include <util/platform.h>
#include <math.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#endif

#define S_ZAPPIFY_ENABLED "zappify_enabled"
#define S_ZAPPIFY_PORT "zappify_port"
#define S_POLL_INTERVAL_MS "poll_interval_ms"
#define S_CORNER_RADIUS "corner_radius"
#define S_BLUR_AMOUNT "blur_amount"
#define S_REFRACTION_STRENGTH "refraction_strength"
#define S_LENS_MAGNIFICATION "lens_magnification"
#define S_CHROMATIC_ABERRATION "chromatic_aberration"
#define S_SHIMMER_SPEED "shimmer_speed"
#define S_SHIMMER_AMPLITUDE "shimmer_amplitude"
#define S_SPECULAR_INTENSITY "specular_intensity"
#define S_SPECULAR_SHININESS "specular_shininess"
#define S_EDGE_HIGHLIGHT_WIDTH "edge_highlight_width"
#define S_EDGE_HIGHLIGHT_INTENSITY "edge_highlight_intensity"
#define S_SHEEN_SPEED "sheen_speed"
#define S_SHEEN_WIDTH "sheen_width"
#define S_SHEEN_INTENSITY "sheen_intensity"
#define S_CONTENT_REACTIVITY "content_reactivity"
#define S_ADAPTIVE_BRIGHTNESS "adaptive_brightness"
#define S_VIBRANCY "vibrancy"
#define S_SHADOW_INTENSITY "shadow_intensity"
#define S_SHADOW_SIZE "shadow_size"
#define S_NOISE_AMOUNT "noise_amount"
#define S_BORDER_WIDTH "border_width"
#define S_BORDER_COLOR "border_color"
#define S_TINT_COLOR "tint_color"

#define MAX_PANELS 8

/*
 * Zappify polling: one background thread shared by every filter instance
 * (a user typically attaches this filter to several scenes, and they all
 * want the same live state -- N independent pollers hitting the same local
 * endpoint would be wasteful and pointless). glass_zappify_poller_start()/
 * _stop() are called once from plugin-main.c's obs_module_load()/unload();
 * glass_filter_video_render() just reads the shared cache below.
 *
 * Per-instance settings (port/interval/enabled) all feed into this single
 * shared poller -- whichever filter instance was updated most recently wins.
 * In practice a user runs one Zappify instance, so every filter attached to
 * it uses the same values anyway.
 */
struct zappify_state {
	pthread_mutex_t lock;
	/* xy = top-left, zw = size; all as 0..1 FRACTIONS of the OBS canvas --
	 * Zappify has no notion of this filter's per-instance render target size,
	 * so it never sends pixels. Converted to pixels per-instance below, right
	 * before pushing into the shader, using that instance's own uv_size. */
	struct vec4 panels[MAX_PANELS];
	int panel_count;
};

/* lock is initialized in glass_zappify_poller_start() via pthread_mutex_init()
 * -- not a static PTHREAD_MUTEX_INITIALIZER, since OBS's Windows pthread shim
 * doesn't guarantee that's valid as a static initializer. */
static struct zappify_state g_zappify_state = {
	.panel_count = 0,
};

static pthread_t g_zappify_thread;
static os_event_t *g_zappify_stop_event = NULL;
static volatile bool g_zappify_thread_running = false;

/* Poller config, updated (non-atomically but benignly -- worst case one
 * stale read for a single poll cycle) from whichever filter instance's
 * glass_filter_update() ran most recently. */
static volatile bool g_zappify_enabled = true;
static volatile long g_zappify_port = 3000;
static volatile long g_zappify_poll_interval_ms = 200;

struct glass_filter {
	obs_source_t *source;
	gs_effect_t *effect;

	gs_eparam_t *param_uv_size;
	gs_eparam_t *param_panels;
	gs_eparam_t *param_panel_count;
	gs_eparam_t *param_corner_radius;
	gs_eparam_t *param_blur_amount;
	gs_eparam_t *param_refraction_strength;
	gs_eparam_t *param_lens_magnification;
	gs_eparam_t *param_chromatic_aberration;
	gs_eparam_t *param_shimmer_speed;
	gs_eparam_t *param_shimmer_amplitude;
	gs_eparam_t *param_elapsed_time;
	gs_eparam_t *param_specular_intensity;
	gs_eparam_t *param_specular_shininess;
	gs_eparam_t *param_edge_highlight_width;
	gs_eparam_t *param_edge_highlight_intensity;
	gs_eparam_t *param_sheen_speed;
	gs_eparam_t *param_sheen_width;
	gs_eparam_t *param_sheen_intensity;
	gs_eparam_t *param_content_reactivity;
	gs_eparam_t *param_adaptive_brightness;
	gs_eparam_t *param_vibrancy;
	gs_eparam_t *param_shadow_intensity;
	gs_eparam_t *param_shadow_size;
	gs_eparam_t *param_noise_amount;
	gs_eparam_t *param_border_width;
	gs_eparam_t *param_border_color;
	gs_eparam_t *param_tint_color;

	float corner_radius;
	float blur_amount;
	float refraction_strength;
	float lens_magnification;
	float chromatic_aberration;
	float shimmer_speed;
	float shimmer_amplitude;
	float specular_intensity;
	float specular_shininess;
	float edge_highlight_width;
	float edge_highlight_intensity;
	float sheen_speed;
	float sheen_width;
	float sheen_intensity;
	float content_reactivity;
	float adaptive_brightness;
	float vibrancy;
	float shadow_intensity;
	float shadow_size;
	float noise_amount;
	float border_width;
	struct vec4 border_color;
	struct vec4 tint_color;

	float elapsed_time;
};

static const char *glass_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("LiquidGlass.Name");
}

static void glass_filter_update(void *data, obs_data_t *settings)
{
	struct glass_filter *filter = data;

	/* Shared Zappify-poller config -- see the comment on struct zappify_state.
	 * Whichever filter instance is updated most recently wins; fine in
	 * practice since every instance points at the same local Zappify. */
	g_zappify_enabled = obs_data_get_bool(settings, S_ZAPPIFY_ENABLED);
	g_zappify_port = (long)obs_data_get_int(settings, S_ZAPPIFY_PORT);
	g_zappify_poll_interval_ms = (long)obs_data_get_int(settings, S_POLL_INTERVAL_MS);

	filter->corner_radius = (float)obs_data_get_double(settings, S_CORNER_RADIUS);

	filter->blur_amount = (float)obs_data_get_double(settings, S_BLUR_AMOUNT);
	filter->refraction_strength = (float)obs_data_get_double(settings, S_REFRACTION_STRENGTH);
	filter->lens_magnification = (float)obs_data_get_double(settings, S_LENS_MAGNIFICATION);
	filter->chromatic_aberration = (float)obs_data_get_double(settings, S_CHROMATIC_ABERRATION);

	filter->shimmer_speed = (float)obs_data_get_double(settings, S_SHIMMER_SPEED);
	filter->shimmer_amplitude = (float)obs_data_get_double(settings, S_SHIMMER_AMPLITUDE);

	filter->specular_intensity = (float)obs_data_get_double(settings, S_SPECULAR_INTENSITY);
	filter->specular_shininess = (float)obs_data_get_double(settings, S_SPECULAR_SHININESS);

	filter->edge_highlight_width = (float)obs_data_get_double(settings, S_EDGE_HIGHLIGHT_WIDTH);
	filter->edge_highlight_intensity = (float)obs_data_get_double(settings, S_EDGE_HIGHLIGHT_INTENSITY);

	filter->sheen_speed = (float)obs_data_get_double(settings, S_SHEEN_SPEED);
	filter->sheen_width = (float)obs_data_get_double(settings, S_SHEEN_WIDTH);
	filter->sheen_intensity = (float)obs_data_get_double(settings, S_SHEEN_INTENSITY);

	filter->content_reactivity = (float)obs_data_get_double(settings, S_CONTENT_REACTIVITY);
	filter->adaptive_brightness = (float)obs_data_get_double(settings, S_ADAPTIVE_BRIGHTNESS);
	filter->vibrancy = (float)obs_data_get_double(settings, S_VIBRANCY);

	filter->shadow_intensity = (float)obs_data_get_double(settings, S_SHADOW_INTENSITY);
	filter->shadow_size = (float)obs_data_get_double(settings, S_SHADOW_SIZE);

	filter->noise_amount = (float)obs_data_get_double(settings, S_NOISE_AMOUNT);

	filter->border_width = (float)obs_data_get_double(settings, S_BORDER_WIDTH);

	vec4_from_rgba(&filter->border_color, (uint32_t)obs_data_get_int(settings, S_BORDER_COLOR));
	vec4_from_rgba(&filter->tint_color, (uint32_t)obs_data_get_int(settings, S_TINT_COLOR));
}

static void *glass_filter_create(obs_data_t *settings, obs_source_t *source)
{
	struct glass_filter *filter = bzalloc(sizeof(struct glass_filter));
	filter->source = source;

	char *effect_path = obs_module_file("liquid_glass.effect");

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		obs_log(LOG_ERROR, "Failed to load liquid_glass.effect");
		bfree(filter);
		return NULL;
	}

	filter->param_uv_size = gs_effect_get_param_by_name(filter->effect, "uv_size");
	filter->param_panels = gs_effect_get_param_by_name(filter->effect, "panels");
	filter->param_panel_count = gs_effect_get_param_by_name(filter->effect, "panel_count");
	filter->param_corner_radius = gs_effect_get_param_by_name(filter->effect, "corner_radius");
	filter->param_blur_amount = gs_effect_get_param_by_name(filter->effect, "blur_amount");
	filter->param_refraction_strength = gs_effect_get_param_by_name(filter->effect, "refraction_strength");
	filter->param_lens_magnification = gs_effect_get_param_by_name(filter->effect, "lens_magnification");
	filter->param_chromatic_aberration = gs_effect_get_param_by_name(filter->effect, "chromatic_aberration");
	filter->param_shimmer_speed = gs_effect_get_param_by_name(filter->effect, "shimmer_speed");
	filter->param_shimmer_amplitude = gs_effect_get_param_by_name(filter->effect, "shimmer_amplitude");
	filter->param_elapsed_time = gs_effect_get_param_by_name(filter->effect, "elapsed_time");
	filter->param_specular_intensity = gs_effect_get_param_by_name(filter->effect, "specular_intensity");
	filter->param_specular_shininess = gs_effect_get_param_by_name(filter->effect, "specular_shininess");
	filter->param_edge_highlight_width = gs_effect_get_param_by_name(filter->effect, "edge_highlight_width");
	filter->param_edge_highlight_intensity =
		gs_effect_get_param_by_name(filter->effect, "edge_highlight_intensity");
	filter->param_sheen_speed = gs_effect_get_param_by_name(filter->effect, "sheen_speed");
	filter->param_sheen_width = gs_effect_get_param_by_name(filter->effect, "sheen_width");
	filter->param_sheen_intensity = gs_effect_get_param_by_name(filter->effect, "sheen_intensity");
	filter->param_content_reactivity = gs_effect_get_param_by_name(filter->effect, "content_reactivity");
	filter->param_adaptive_brightness = gs_effect_get_param_by_name(filter->effect, "adaptive_brightness");
	filter->param_vibrancy = gs_effect_get_param_by_name(filter->effect, "vibrancy");
	filter->param_shadow_intensity = gs_effect_get_param_by_name(filter->effect, "shadow_intensity");
	filter->param_shadow_size = gs_effect_get_param_by_name(filter->effect, "shadow_size");
	filter->param_noise_amount = gs_effect_get_param_by_name(filter->effect, "noise_amount");
	filter->param_border_width = gs_effect_get_param_by_name(filter->effect, "border_width");
	filter->param_border_color = gs_effect_get_param_by_name(filter->effect, "border_color");
	filter->param_tint_color = gs_effect_get_param_by_name(filter->effect, "tint_color");

	glass_filter_update(filter, settings);

	return filter;
}

static void glass_filter_destroy(void *data)
{
	struct glass_filter *filter = data;

	obs_enter_graphics();
	gs_effect_destroy(filter->effect);
	obs_leave_graphics();

	bfree(filter);
}

static void glass_filter_video_tick(void *data, float seconds)
{
	struct glass_filter *filter = data;

	filter->elapsed_time += seconds;
	if (filter->elapsed_time > 6283.0f) /* ~1000*2pi, keep trig args sane */
		filter->elapsed_time -= 6283.0f;
}

static void glass_filter_video_render(void *data, gs_effect_t *effect)
{
	struct glass_filter *filter = data;

	if (!obs_source_process_filter_begin(filter->source, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;

	obs_source_t *target = obs_filter_get_target(filter->source);
	uint32_t width = target ? obs_source_get_base_width(target) : 0;
	uint32_t height = target ? obs_source_get_base_height(target) : 0;

	struct vec2 uv_size;
	vec2_set(&uv_size, (float)width, (float)height);

	// Snapshot the shared Zappify state (fractions 0..1) and convert each
	// panel to this instance's own pixel space. Keep every panel fully
	// inside the actual frame -- without this, a panel that touches or
	// crosses the frame boundary (e.g. a module partially off the edge of a
	// mismatched canvas size) can push past it: the rounded corner there
	// then never gets rasterized at all, and what's left looks like a flat,
	// square cut instead of a round one. Sliding it inward (or shrinking it,
	// only if it's larger than the frame) guarantees every corner is
	// actually on-screen.
	struct vec4 panels_px[MAX_PANELS] = {0};
	int panel_count = 0;

	pthread_mutex_lock(&g_zappify_state.lock);
	panel_count = g_zappify_state.panel_count;
	if (panel_count > MAX_PANELS)
		panel_count = MAX_PANELS;
	memcpy(panels_px, g_zappify_state.panels, sizeof(struct vec4) * (size_t)panel_count);
	pthread_mutex_unlock(&g_zappify_state.lock);

	if (width > 0 && height > 0) {
		for (int i = 0; i < panel_count; i++) {
			float panel_x = panels_px[i].x * (float)width;
			float panel_y = panels_px[i].y * (float)height;
			float panel_w = panels_px[i].z * (float)width;
			float panel_h = panels_px[i].w * (float)height;

			if (panel_w > (float)width)
				panel_w = (float)width;
			if (panel_h > (float)height)
				panel_h = (float)height;
			float max_x = (float)width - panel_w;
			float max_y = (float)height - panel_h;
			panel_x = fmaxf(0.0f, fminf(panel_x, max_x));
			panel_y = fmaxf(0.0f, fminf(panel_y, max_y));

			panels_px[i].x = panel_x;
			panels_px[i].y = panel_y;
			panels_px[i].z = panel_w;
			panels_px[i].w = panel_h;
		}
	} else {
		panel_count = 0;
	}

	gs_effect_set_vec2(filter->param_uv_size, &uv_size);
	gs_effect_set_val(filter->param_panels, panels_px, sizeof(struct vec4) * MAX_PANELS);
	gs_effect_set_int(filter->param_panel_count, panel_count);
	gs_effect_set_float(filter->param_corner_radius, filter->corner_radius);
	gs_effect_set_float(filter->param_blur_amount, filter->blur_amount);
	gs_effect_set_float(filter->param_refraction_strength, filter->refraction_strength);
	gs_effect_set_float(filter->param_lens_magnification, filter->lens_magnification);
	gs_effect_set_float(filter->param_chromatic_aberration, filter->chromatic_aberration);
	gs_effect_set_float(filter->param_shimmer_speed, filter->shimmer_speed);
	gs_effect_set_float(filter->param_shimmer_amplitude, filter->shimmer_amplitude);
	gs_effect_set_float(filter->param_elapsed_time, filter->elapsed_time);
	gs_effect_set_float(filter->param_specular_intensity, filter->specular_intensity);
	gs_effect_set_float(filter->param_specular_shininess, filter->specular_shininess);
	gs_effect_set_float(filter->param_edge_highlight_width, filter->edge_highlight_width);
	gs_effect_set_float(filter->param_edge_highlight_intensity, filter->edge_highlight_intensity);
	gs_effect_set_float(filter->param_sheen_speed, filter->sheen_speed);
	gs_effect_set_float(filter->param_sheen_width, filter->sheen_width);
	gs_effect_set_float(filter->param_sheen_intensity, filter->sheen_intensity);
	gs_effect_set_float(filter->param_content_reactivity, filter->content_reactivity);
	gs_effect_set_float(filter->param_adaptive_brightness, filter->adaptive_brightness);
	gs_effect_set_float(filter->param_vibrancy, filter->vibrancy);
	gs_effect_set_float(filter->param_shadow_intensity, filter->shadow_intensity);
	gs_effect_set_float(filter->param_shadow_size, filter->shadow_size);
	gs_effect_set_float(filter->param_noise_amount, filter->noise_amount);
	gs_effect_set_float(filter->param_border_width, filter->border_width);
	gs_effect_set_vec4(filter->param_border_color, &filter->border_color);
	gs_effect_set_vec4(filter->param_tint_color, &filter->tint_color);

	obs_source_process_filter_end(filter->source, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

static obs_properties_t *glass_filter_get_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_bool(props, S_ZAPPIFY_ENABLED, obs_module_text("LiquidGlass.ZappifyEnabled"));
	obs_properties_add_int(props, S_ZAPPIFY_PORT, obs_module_text("LiquidGlass.ZappifyPort"), 1, 65535, 1);
	obs_properties_add_int(props, S_POLL_INTERVAL_MS, obs_module_text("LiquidGlass.PollIntervalMs"), 50, 5000, 10);

	obs_properties_add_float_slider(props, S_CORNER_RADIUS, obs_module_text("LiquidGlass.CornerRadius"), 0.0, 400.0,
					1.0);

	obs_properties_add_float_slider(props, S_BLUR_AMOUNT, obs_module_text("LiquidGlass.BlurAmount"), 0.0, 64.0,
					0.5);
	obs_properties_add_float_slider(props, S_REFRACTION_STRENGTH, obs_module_text("LiquidGlass.RefractionStrength"),
					0.0, 80.0, 0.5);
	obs_properties_add_float_slider(props, S_LENS_MAGNIFICATION, obs_module_text("LiquidGlass.LensMagnification"),
					0.0, 1.0, 0.01);
	obs_properties_add_float_slider(props, S_CHROMATIC_ABERRATION,
					obs_module_text("LiquidGlass.ChromaticAberration"), 0.0, 20.0, 0.1);

	obs_properties_add_float_slider(props, S_SHIMMER_SPEED, obs_module_text("LiquidGlass.ShimmerSpeed"), 0.0, 5.0,
					0.05);
	obs_properties_add_float_slider(props, S_SHIMMER_AMPLITUDE, obs_module_text("LiquidGlass.ShimmerAmplitude"),
					0.0, 20.0, 0.1);

	obs_properties_add_float_slider(props, S_SPECULAR_INTENSITY, obs_module_text("LiquidGlass.SpecularIntensity"),
					0.0, 1.5, 0.01);
	obs_properties_add_float_slider(props, S_SPECULAR_SHININESS, obs_module_text("LiquidGlass.SpecularShininess"),
					1.0, 128.0, 1.0);

	obs_properties_add_float_slider(props, S_EDGE_HIGHLIGHT_WIDTH,
					obs_module_text("LiquidGlass.EdgeHighlightWidth"), 0.0, 60.0, 0.5);
	obs_properties_add_float_slider(props, S_EDGE_HIGHLIGHT_INTENSITY,
					obs_module_text("LiquidGlass.EdgeHighlightIntensity"), 0.0, 3.0, 0.02);

	obs_properties_add_float_slider(props, S_SHEEN_SPEED, obs_module_text("LiquidGlass.SheenSpeed"), 0.0, 2.0,
					0.01);
	obs_properties_add_float_slider(props, S_SHEEN_WIDTH, obs_module_text("LiquidGlass.SheenWidth"), 0.05, 1.5,
					0.01);
	obs_properties_add_float_slider(props, S_SHEEN_INTENSITY, obs_module_text("LiquidGlass.SheenIntensity"), 0.0,
					1.5, 0.01);

	obs_properties_add_float_slider(props, S_CONTENT_REACTIVITY, obs_module_text("LiquidGlass.ContentReactivity"),
					0.0, 1.0, 0.01);
	obs_properties_add_float_slider(props, S_ADAPTIVE_BRIGHTNESS, obs_module_text("LiquidGlass.AdaptiveBrightness"),
					0.0, 1.0, 0.01);
	obs_properties_add_float_slider(props, S_VIBRANCY, obs_module_text("LiquidGlass.Vibrancy"), 0.0, 1.0, 0.01);

	obs_properties_add_float_slider(props, S_SHADOW_INTENSITY, obs_module_text("LiquidGlass.ShadowIntensity"), 0.0,
					1.0, 0.01);
	obs_properties_add_float_slider(props, S_SHADOW_SIZE, obs_module_text("LiquidGlass.ShadowSize"), 0.0, 120.0,
					1.0);

	obs_properties_add_float_slider(props, S_NOISE_AMOUNT, obs_module_text("LiquidGlass.NoiseAmount"), 0.0, 0.15,
					0.002);

	obs_properties_add_float_slider(props, S_BORDER_WIDTH, obs_module_text("LiquidGlass.BorderWidth"), 0.0, 20.0,
					0.1);
	obs_properties_add_color_alpha(props, S_BORDER_COLOR, obs_module_text("LiquidGlass.BorderColor"));
	obs_properties_add_color_alpha(props, S_TINT_COLOR, obs_module_text("LiquidGlass.TintColor"));

	return props;
}

static void glass_filter_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_ZAPPIFY_ENABLED, true);
	obs_data_set_default_int(settings, S_ZAPPIFY_PORT, 3000);
	obs_data_set_default_int(settings, S_POLL_INTERVAL_MS, 200);
	obs_data_set_default_double(settings, S_CORNER_RADIUS, 56.0);

	obs_data_set_default_double(settings, S_BLUR_AMOUNT, 24.0);
	obs_data_set_default_double(settings, S_REFRACTION_STRENGTH, 40.0);
	obs_data_set_default_double(settings, S_LENS_MAGNIFICATION, 0.55);
	obs_data_set_default_double(settings, S_CHROMATIC_ABERRATION, 4.5);

	obs_data_set_default_double(settings, S_SHIMMER_SPEED, 0.5);
	obs_data_set_default_double(settings, S_SHIMMER_AMPLITUDE, 1.2);

	obs_data_set_default_double(settings, S_SPECULAR_INTENSITY, 0.5);
	obs_data_set_default_double(settings, S_SPECULAR_SHININESS, 24.0);

	obs_data_set_default_double(settings, S_EDGE_HIGHLIGHT_WIDTH, 8.0);
	obs_data_set_default_double(settings, S_EDGE_HIGHLIGHT_INTENSITY, 0.75);

	obs_data_set_default_double(settings, S_SHEEN_SPEED, 0.12);
	obs_data_set_default_double(settings, S_SHEEN_WIDTH, 0.35);
	obs_data_set_default_double(settings, S_SHEEN_INTENSITY, 0.4);

	obs_data_set_default_double(settings, S_CONTENT_REACTIVITY, 0.8);
	obs_data_set_default_double(settings, S_ADAPTIVE_BRIGHTNESS, 0.5);
	obs_data_set_default_double(settings, S_VIBRANCY, 0.3);

	obs_data_set_default_double(settings, S_SHADOW_INTENSITY, 0.35);
	obs_data_set_default_double(settings, S_SHADOW_SIZE, 28.0);

	obs_data_set_default_double(settings, S_NOISE_AMOUNT, 0.02);

	obs_data_set_default_double(settings, S_BORDER_WIDTH, 1.5);
	obs_data_set_default_int(settings, S_BORDER_COLOR, 0x40FFFFFF);
	obs_data_set_default_int(settings, S_TINT_COLOR, 0x00FFFFFF);
}

/* ===================== Zappify state poller ===================== */

#ifdef _WIN32

static void glass_zappify_clear_panels(void)
{
	pthread_mutex_lock(&g_zappify_state.lock);
	g_zappify_state.panel_count = 0;
	pthread_mutex_unlock(&g_zappify_state.lock);
}

/* One blocking GET against Zappify's local API, parsed with libobs's own
 * JSON reader (no third-party JSON dependency). Any failure just clears the
 * panel list for this cycle -- Zappify not running is a normal, common state
 * (the user hasn't started the app, or closed it), never a crash. */
static void glass_zappify_poll_once(void)
{
	if (!g_zappify_enabled) {
		glass_zappify_clear_panels();
		return;
	}

	long port = g_zappify_port;
	if (port < 1 || port > 65535)
		port = 3000;

	HINTERNET hSession = WinHttpOpen(L"obs-liquid-glass/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
					 WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		glass_zappify_clear_panels();
		return;
	}

	/* Keep every poll cycle snappy even if Zappify hangs or the port is
	 * firewalled -- this must never stall the shared poller thread for
	 * longer than a cycle or two. */
	WinHttpSetTimeouts(hSession, 1000, 1000, 1500, 1500);

	HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", (INTERNET_PORT)port, 0);
	if (!hConnect) {
		WinHttpCloseHandle(hSession);
		glass_zappify_clear_panels();
		return;
	}

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/glass-blur/state", NULL, WINHTTP_NO_REFERER,
						WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		glass_zappify_clear_panels();
		return;
	}

	bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
		  WinHttpReceiveResponse(hRequest, NULL);

	char *body = NULL;
	size_t body_len = 0;

	if (ok) {
		for (;;) {
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0)
				break;

			char *grown = brealloc(body, body_len + (size_t)avail + 1);
			body = grown;

			DWORD did_read = 0;
			if (!WinHttpReadData(hRequest, body + body_len, avail, &did_read)) {
				break;
			}
			body_len += did_read;
			body[body_len] = '\0';
			if (did_read == 0)
				break;
		}
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	if (!ok || !body || body_len == 0) {
		bfree(body);
		glass_zappify_clear_panels();
		return;
	}

	obs_data_t *root = obs_data_create_from_json(body);
	bfree(body);

	if (!root) {
		glass_zappify_clear_panels();
		return;
	}

	struct vec4 parsed[MAX_PANELS] = {0};
	int parsed_count = 0;

	if (obs_data_get_bool(root, "enabled")) {
		obs_data_array_t *arr = obs_data_get_array(root, "panels");
		if (arr) {
			size_t count = obs_data_array_count(arr);
			if (count > MAX_PANELS)
				count = MAX_PANELS;

			for (size_t i = 0; i < count; i++) {
				obs_data_t *item = obs_data_array_item(arr, i);
				if (item) {
					parsed[i].x = (float)obs_data_get_double(item, "x");
					parsed[i].y = (float)obs_data_get_double(item, "y");
					parsed[i].z = (float)obs_data_get_double(item, "w");
					parsed[i].w = (float)obs_data_get_double(item, "h");
					obs_data_release(item);
				}
			}
			parsed_count = (int)count;
			obs_data_array_release(arr);
		}
	}

	obs_data_release(root);

	pthread_mutex_lock(&g_zappify_state.lock);
	memcpy(g_zappify_state.panels, parsed, sizeof(parsed));
	g_zappify_state.panel_count = parsed_count;
	pthread_mutex_unlock(&g_zappify_state.lock);
}

#endif /* _WIN32 */

static void *glass_zappify_poll_thread(void *unused)
{
	UNUSED_PARAMETER(unused);
	os_set_thread_name("glass-zappify-poll");

#ifdef _WIN32
	while (g_zappify_thread_running) {
		glass_zappify_poll_once();

		long interval = g_zappify_poll_interval_ms;
		if (interval < 50)
			interval = 50;

		/* Returns 0 if the stop event fired, ETIMEDOUT if the interval
		 * elapsed with no stop request -- either way this doubles as our
		 * interruptible sleep. */
		if (os_event_timedwait(g_zappify_stop_event, (unsigned long)interval) == 0)
			break;
	}
#else
	/* Only Windows ships a HTTP client (see the #ifdef _WIN32 block above) --
	 * this plugin only targets Windows. On any other platform the panel
	 * list simply stays empty; just wait here for a clean, joinable exit. */
	os_event_wait(g_zappify_stop_event);
#endif

	return NULL;
}

/* Called once from plugin-main.c's obs_module_load(). Shared by every filter
 * instance -- see the comment on struct zappify_state above. */
void glass_zappify_poller_start(void)
{
	if (g_zappify_thread_running)
		return;

	pthread_mutex_init(&g_zappify_state.lock, NULL);

	if (os_event_init(&g_zappify_stop_event, OS_EVENT_TYPE_MANUAL) != 0) {
		obs_log(LOG_ERROR, "Zappify poller: failed to create stop event");
		pthread_mutex_destroy(&g_zappify_state.lock);
		return;
	}

	g_zappify_thread_running = true;
	if (pthread_create(&g_zappify_thread, NULL, glass_zappify_poll_thread, NULL) != 0) {
		obs_log(LOG_ERROR, "Zappify poller: failed to start thread");
		g_zappify_thread_running = false;
		os_event_destroy(g_zappify_stop_event);
		g_zappify_stop_event = NULL;
		pthread_mutex_destroy(&g_zappify_state.lock);
	}
}

/* Called once from plugin-main.c's obs_module_unload(). Joins (never
 * detaches) the poll thread so it can't touch freed state during shutdown. */
void glass_zappify_poller_stop(void)
{
	if (!g_zappify_thread_running)
		return;

	g_zappify_thread_running = false;
	if (g_zappify_stop_event)
		os_event_signal(g_zappify_stop_event);

	pthread_join(g_zappify_thread, NULL);

	if (g_zappify_stop_event) {
		os_event_destroy(g_zappify_stop_event);
		g_zappify_stop_event = NULL;
	}

	pthread_mutex_destroy(&g_zappify_state.lock);
}

struct obs_source_info liquid_glass_filter_info = {
	.id = "liquid_glass_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = glass_filter_get_name,
	.create = glass_filter_create,
	.destroy = glass_filter_destroy,
	.update = glass_filter_update,
	.get_defaults = glass_filter_get_defaults,
	.get_properties = glass_filter_get_properties,
	.video_render = glass_filter_video_render,
	.video_tick = glass_filter_video_tick,
};
