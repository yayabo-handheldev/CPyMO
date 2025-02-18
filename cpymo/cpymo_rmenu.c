﻿#include "cpymo_prelude.h"
#include "cpymo_rmenu.h"
#include "cpymo_engine.h"
#include "cpymo_config_ui.h"
#include "cpymo_msgbox_ui.h"
#include "cpymo_save_ui.h"
#include "cpymo_save.h"
#include "cpymo_localization.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef __3DS__
extern bool fill_screen_enabled;
const extern bool fill_screen;
const extern bool enhanced_3ds_display_mode;
#endif

#ifdef ENABLE_TEXT_EXTRACT_ANDROID_ACCESSIBILITY
#include <cpymo_android.h>
#endif

typedef struct {
	cpymo_backend_image bg;
	int bg_w, bg_h;

	cpymo_select_img menu;
	cpymo_wait menu_waiter;

	cpymo_key_hold touch;

	bool alive;
} cpymo_rmenu;

static error_t cpymo_rmenu_update(cpymo_engine *e, void *ui_data, float dt)
{
	cpymo_rmenu *r = (cpymo_rmenu *)ui_data;

	enum cpymo_key_hold_result result = cpymo_key_hold_update(&r->touch, dt, e->input.mouse_button);
	if (result == cpymo_key_hold_result_just_hold) {
		cpymo_ui_exit(e);
		return CPYMO_ERR_SUCC;
	}

	if (r->alive) {
		error_t err = cpymo_wait_update(&r->menu_waiter, e, dt);
		CPYMO_THROW(err);

		if (r->alive) {
			err = cpymo_select_img_update(e, &r->menu, dt);
			CPYMO_THROW(err);
		}
	}

	return CPYMO_ERR_SUCC;
}

static inline float cpymo_rmenu_zoom(const cpymo_engine *e) 
{
	if (cpymo_gameconfig_is_symbian(&e->gameconfig)) {
		if (e->gameconfig.platform[4] == '5') return 1.3f;		// s60v5
		else return 1.6f;	// s60v3
	}
	else return 1.0f;	// android
}

static void cpymo_rmenu_draw(const cpymo_engine *e, const void *ui_data)
{
	const cpymo_rmenu *r = (const cpymo_rmenu *)ui_data;

	cpymo_bg_draw(e);
	cpymo_scroll_draw(&e->scroll);

	#ifdef __3DS__
	if (fill_screen && !enhanced_3ds_display_mode)
		fill_screen_enabled = false;
	#endif

	float zoom = cpymo_rmenu_zoom(e);
	float w = (float)r->bg_w * zoom;
	float h = (float)r->bg_h * zoom;

	float bg_xywh[4] = {
		((float)e->gameconfig.imagesize_w - (float)w) / 2,
		((float)e->gameconfig.imagesize_h - (float)h) / 2 - 0.018f * (float)e->gameconfig.imagesize_h,
		(float)w,
		(float)h,
	};

	if (r->bg) {
#ifdef DISABLE_IMAGE_SCALING
		cpymo_backend_image_draw(
			bg_xywh[0] + (bg_xywh[2] - r->bg_w) / 2,
			bg_xywh[1] + (bg_xywh[3] - r->bg_h) / 2, 
			r->bg_w,
			r->bg_h,
			r->bg,
			0,
			0,
			r->bg_w,
			r->bg_h,
			1.0f,
			cpymo_backend_image_draw_type_ui_element_bg);
#else
		cpymo_backend_image_draw(
			bg_xywh[0],
			bg_xywh[1],
			bg_xywh[2],
			bg_xywh[3],
			r->bg,
			0,
			0,
			r->bg_w,
			r->bg_h,
			1.0f,
			cpymo_backend_image_draw_type_ui_element_bg);
#endif
	}
	else {
		cpymo_backend_image_fill_rects(
			bg_xywh, 1, cpymo_color_black, 0.75f, 
			cpymo_backend_image_draw_type_ui_element_bg);
	}

	cpymo_select_img_draw(&r->menu, e->gameconfig.imagesize_w, e->gameconfig.imagesize_h, false);

	#ifdef __3DS__
	fill_screen_enabled = true;
	#endif
}

static void cpymo_rmenu_delete(cpymo_engine *e, void *ui_data)
{
	cpymo_rmenu *r = (cpymo_rmenu *)ui_data;
	r->alive = false;

	if (r->bg) cpymo_backend_image_free(r->bg);

	cpymo_select_img_free(&r->menu);
}

error_t cpymo_rmenu_restart_game(cpymo_engine *e, void *data)
{
	char *gamedir = 
		cpymo_str_copy_malloc(cpymo_str_pure(e->assetloader.gamedir));
	if (gamedir == NULL) return CPYMO_ERR_OUT_OF_MEM;

	cpymo_engine_free(e);
	error_t err = cpymo_engine_init(e, gamedir);
	free(gamedir);

	return err;
}

static error_t cpymo_rmenu_ok(cpymo_engine *e, int sel, uint64_t hash, bool _)
{
	switch (sel) {
	case 0: cpymo_save_ui_enter(e, false); break;
	case 1: cpymo_save_ui_enter(e, true); break;
	case 2: cpymo_ui_exit(e); e->skipping = true; break;
	case 3: cpymo_say_hidewindow_until_click(e); cpymo_ui_exit(e); break;
	case 4: cpymo_backlog_ui_enter(e); break;
	case 5: cpymo_config_ui_enter(e); break;
	case 6: 
		cpymo_msgbox_ui_enter(
			e, 
			cpymo_str_pure(
				cpymo_localization_get(e)->rmenu_restat_game_confirm_message), 
			&cpymo_rmenu_restart_game, 
			NULL);
		break;
	
	case 7: cpymo_ui_exit(e); break;
	default:
		assert(false);
	}

	return CPYMO_ERR_SUCC;
}

error_t cpymo_rmenu_enter(cpymo_engine *e)
{
	cpymo_rmenu *rmenu = NULL;
	error_t err = cpymo_ui_enter(
		(void **)&rmenu,
		e,
		sizeof(cpymo_rmenu),
		&cpymo_rmenu_update,
		&cpymo_rmenu_draw,
		&cpymo_rmenu_delete);
	CPYMO_THROW(err);

#ifdef ENABLE_TEXT_EXTRACT_ANDROID_ACCESSIBILITY
	cpymo_android_play_sound(SOUND_MENU);
#endif

	rmenu->alive = true;

	cpymo_key_hold_init(&rmenu->touch, e->input.mouse_button);

	cpymo_select_img_init(&rmenu->menu);
	cpymo_wait_reset(&rmenu->menu_waiter);

	rmenu->bg = NULL;
	err = cpymo_assetloader_load_system_image(
		&rmenu->bg,
		&rmenu->bg_w,
		&rmenu->bg_h,
		cpymo_str_pure("menu"),
		&e->assetloader,
		true);

	if (err != CPYMO_ERR_SUCC) {
		rmenu->bg = NULL;

		if (strcmp(e->gameconfig.platform, "s60v3") == 0) {
			rmenu->bg_w = 240;
			rmenu->bg_h = 144;
		}
		else if (strcmp(e->gameconfig.platform, "s60v5") == 0) {
			rmenu->bg_w = 360;
			rmenu->bg_h = 216;
		}
		else {
			rmenu->bg_w = (int)((float)e->gameconfig.imagesize_w * 0.75f);
			rmenu->bg_h = (int)((float)e->gameconfig.imagesize_h * 0.6f);
		}
	}

	err = cpymo_select_img_configuare_begin(
		&rmenu->menu,
		8,
		cpymo_str_pure(""),
		&e->assetloader,
		&e->gameconfig);
	if (err != CPYMO_ERR_SUCC) {
		cpymo_ui_exit(e);
		return err;
	}

	const float font_size = cpymo_rmenu_zoom(e) * 16 / 240.0f * e->gameconfig.imagesize_h * 0.75f;

	#define RMENU_ITEM(_, TEXT, ENABLED) \
		err = cpymo_select_img_configuare_select_text( \
			&rmenu->menu, \
			&e->assetloader, \
			&e->gameconfig, \
			&e->flags, \
			cpymo_str_pure(TEXT), \
			ENABLED, \
			cpymo_select_img_selection_nohint, \
			0xFFFFFFFFFFFFFFFF, \
			font_size); \
		if (err != CPYMO_ERR_SUCC) { \
			cpymo_ui_exit(e); \
			return err; \
		}

	cpymo_select_img_configuare_set_ok_callback(&rmenu->menu, &cpymo_rmenu_ok);

	const cpymo_localization *l = cpymo_localization_get(e);

	RMENU_ITEM(0, l->rmenu_save, true);
	RMENU_ITEM(1, l->rmenu_load, true);
	RMENU_ITEM(2, l->rmenu_skip, e->select_img.selections == NULL);
	RMENU_ITEM(3, l->rmenu_hide_window, e->select_img.selections == NULL);
	RMENU_ITEM(4, l->rmenu_backlog, true);
	RMENU_ITEM(5, l->rmenu_config, true)
	RMENU_ITEM(6, l->rmenu_restart, true);
	RMENU_ITEM(7, l->rmenu_back_to_game, true);

	float xywh[4] = {
		((float)e->gameconfig.imagesize_w - (float)rmenu->bg_w) / 2,
		((float)e->gameconfig.imagesize_h - (float)rmenu->bg_h) / 2,
		(float)rmenu->bg_w,
		(float)rmenu->bg_h,
	};

	cpymo_select_img_configuare_end_select_text(
		&rmenu->menu,
		&rmenu->menu_waiter,
		e,
		xywh[0],
		xywh[1],
		xywh[0] + xywh[2],
		xywh[1] + xywh[3],
		cpymo_color_white,
		0,
		false);

	rmenu->menu.draw_type = cpymo_backend_image_draw_type_ui_element;

	return CPYMO_ERR_SUCC;
}

