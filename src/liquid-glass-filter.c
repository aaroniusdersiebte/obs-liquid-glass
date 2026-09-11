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
#include <math.h>

#define S_PANEL_X "panel_x"
#define S_PANEL_Y "panel_y"
#define S_PANEL_W "panel_w"
#define S_PANEL_H "panel_h"
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

struct glass_filter {
	obs_source_t *source;
	gs_effect_t *effect;

	gs_eparam_t *param_uv_size;
	gs_eparam_t *param_panel_pos;
	gs_eparam_t *param_panel_size;
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

	float panel_x, panel_y, panel_w, panel_h;
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

	filter->panel_x = (float)obs_data_get_double(settings, S_PANEL_X);
	filter->panel_y = (float)obs_data_get_double(settings, S_PANEL_Y);
	filter->panel_w = (float)obs_data_get_double(settings, S_PANEL_W);
	filter->panel_h = (float)obs_data_get_double(settings, S_PANEL_H);
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
	filter->param_panel_pos = gs_effect_get_param_by_name(filter->effect, "panel_pos");
	filter->param_panel_size = gs_effect_get_param_by_name(filter->effect, "panel_size");
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

	// Keep the panel fully inside the actual frame. Without this, panel
	// coordinates typed for one canvas size (or a filter target smaller
	// than expected, e.g. a cropped/lower-res source) can push the right
	// or bottom edge past the frame boundary -- the rounded corner there
	// then never gets rasterized at all, and what's left looks like a
	// flat, square cut instead of a round one. Sliding the panel inward
	// (or shrinking it, only if it's larger than the frame) guarantees
	// every corner is always actually on-screen.
	float panel_w = filter->panel_w;
	float panel_h = filter->panel_h;
	float panel_x = filter->panel_x;
	float panel_y = filter->panel_y;
	if (width > 0 && height > 0) {
		if (panel_w > (float)width)
			panel_w = (float)width;
		if (panel_h > (float)height)
			panel_h = (float)height;
		float max_x = (float)width - panel_w;
		float max_y = (float)height - panel_h;
		panel_x = fmaxf(0.0f, fminf(panel_x, max_x));
		panel_y = fmaxf(0.0f, fminf(panel_y, max_y));
	}

	struct vec2 pos;
	vec2_set(&pos, panel_x, panel_y);

	struct vec2 size;
	vec2_set(&size, panel_w, panel_h);

	gs_effect_set_vec2(filter->param_uv_size, &uv_size);
	gs_effect_set_vec2(filter->param_panel_pos, &pos);
	gs_effect_set_vec2(filter->param_panel_size, &size);
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

	obs_properties_add_float(props, S_PANEL_X, obs_module_text("LiquidGlass.PanelX"), -10000.0, 10000.0, 1.0);
	obs_properties_add_float(props, S_PANEL_Y, obs_module_text("LiquidGlass.PanelY"), -10000.0, 10000.0, 1.0);
	obs_properties_add_float(props, S_PANEL_W, obs_module_text("LiquidGlass.PanelW"), 1.0, 10000.0, 1.0);
	obs_properties_add_float(props, S_PANEL_H, obs_module_text("LiquidGlass.PanelH"), 1.0, 10000.0, 1.0);
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
	obs_data_set_default_double(settings, S_PANEL_X, 120.0);
	obs_data_set_default_double(settings, S_PANEL_Y, 700.0);
	obs_data_set_default_double(settings, S_PANEL_W, 640.0);
	obs_data_set_default_double(settings, S_PANEL_H, 260.0);
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
