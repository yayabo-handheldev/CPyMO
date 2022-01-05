#include "cpymo_select_img.h"
#include "cpymo_engine.h"
#include <assert.h>

static const float fade_in_time = 0.5f;

void cpymo_select_img_reset(cpymo_select_img *img)
{
	if (img->select_img_image) {
		// select_img
		cpymo_backend_image_free(img->select_img_image);
	}
	else if(img->selections) {
		// select_imgs
		for (size_t i = 0; i < img->all_selections; ++i)
			if (img->selections[i].image)
				cpymo_backend_image_free(img->selections[i].image);
	}

	if (img->selections)
		free(img->selections);

	img->selections = NULL;
	img->select_img_image = NULL;
}

error_t cpymo_select_img_configuare_begin(
	struct cpymo_engine *engine, size_t selections,
	cpymo_parser_stream_span image_name_or_empty_when_select_imgs)
{
	engine->select_img.selections = 
		(cpymo_select_img_selection *)malloc(sizeof(cpymo_select_img_selection) * selections);
	if (engine->select_img.selections == NULL) return CPYMO_ERR_SUCC;

	for (size_t i = 0; i < selections; ++i)
		engine->select_img.selections[i].image = NULL;

	if (image_name_or_empty_when_select_imgs.len > 0) {
		error_t err = cpymo_assetloader_load_system_image(
			&engine->select_img.select_img_image,
			&engine->select_img.select_img_image_w, &engine->select_img.select_img_image_h,
			image_name_or_empty_when_select_imgs,
			"png",
			&engine->assetloader,
			cpymo_gameconfig_is_symbian(&engine->gameconfig));

		if (err != CPYMO_ERR_SUCC) {
			free(engine->select_img.selections);
			engine->select_img.selections = NULL;
			return err;
		}
	}

	engine->select_img.current_selection = 0;
	engine->select_img.all_selections = selections;
	engine->select_img.input_enabled = false;

	return CPYMO_ERR_SUCC;
}

void cpymo_select_img_configuare_select_img_selection(cpymo_engine *e, float x, float y, bool enabled)
{
	assert(e->select_img.selections);
	assert(e->select_img.all_selections);
	assert(e->select_img.select_img_image);

	cpymo_select_img_selection *sel = &e->select_img.selections[e->select_img.current_selection++];
	sel->image = e->select_img.select_img_image;

	sel->x = x;
	sel->y = y;
	sel->w = e->select_img.select_img_image_w / 2;
	sel->h = e->select_img.select_img_image_h / e->select_img.all_selections;

	sel->enabled = enabled;
}

error_t cpymo_select_img_configuare_select_imgs_selection(cpymo_engine *e, cpymo_parser_stream_span image_name, float x, float y, bool enabled)
{
	assert(e->select_img.selections);
	assert(e->select_img.all_selections);
	assert(e->select_img.select_img_image == NULL);

	cpymo_select_img_selection *sel = &e->select_img.selections[e->select_img.current_selection++];

	error_t err = cpymo_assetloader_load_system_image(
		&sel->image,
		&sel->w,
		&sel->h,
		image_name,
		"png",
		&e->assetloader,
		cpymo_gameconfig_is_symbian(&e->gameconfig)
	);

	if (err != CPYMO_ERR_SUCC) {
		return err;
	}

	sel->x = x;
	sel->y = y;
	sel->enabled = enabled;

	return CPYMO_ERR_SUCC;
}

static bool cpymo_select_img_wait(struct cpymo_engine *e, float dt)
{
	if (!cpymo_tween_finished(&e->select_img.alpha))
		cpymo_engine_request_redraw(e);

	cpymo_tween_update(&e->select_img.alpha, dt);
	return e->select_img.selections == NULL;
}

void cpymo_select_img_configuare_end(struct cpymo_engine *e, int init_position)
{
	assert(e->select_img.selections);
	assert(e->select_img.all_selections);
	assert(e->select_img.current_selection == e->select_img.all_selections);

	e->select_img.alpha = cpymo_tween_create(fade_in_time);
	e->select_img.current_selection = init_position >= 0 ? init_position : 0;
	e->select_img.save_enabled = init_position == -1;
	e->select_img.input_enabled = true;

	// In pymo, if all options are disabled, it will enable every option.
	bool all_is_disabled = true;
	for (size_t i = 0; i < e->select_img.all_selections; ++i) {
		if (e->select_img.selections[i].enabled) {
			all_is_disabled = false;
			break;
		}
	}

	if (all_is_disabled)
		for (size_t i = 0; i < e->select_img.all_selections; ++i)
			e->select_img.selections[i].enabled = true;

	// Trim disabled images.
	for (size_t i = 0; i < e->select_img.all_selections; ++i) {
		if (!e->select_img.selections[i].enabled) {
			if (e->select_img.selections[i].image) {
				if (e->select_img.select_img_image == NULL)
					cpymo_backend_image_free(e->select_img.selections[i].image);
				e->select_img.selections[i].image = NULL;
			}
		}
	}

	cpymo_wait_register(&e->wait, &cpymo_select_img_wait);
}

error_t cpymo_select_img_update(cpymo_engine * engine)
{
	return CPYMO_ERR_SUCC;
}

void cpymo_select_img_draw(const cpymo_select_img *o)
{
	if (o->selections) {
		for (size_t i = 0; i < o->all_selections; ++i) {
			const cpymo_select_img_selection *sel = &o->selections[i];
			if (sel->image) {
				const bool selected = o->current_selection == i;
				cpymo_backend_image_draw(
					sel->x - (float)sel->w / 2.0f,
					sel->y - (float)sel->h / 2.0f,
					(float)sel->w,
					(float)sel->h,
					sel->image,
					selected ? sel->w : 0,
					o->select_img_image ? i * sel->h : 0,
					sel->w,
					sel->h,
					cpymo_tween_value(&o->alpha),
					cpymo_backend_image_draw_type_sel_img
				);
			}
		}
	}
}