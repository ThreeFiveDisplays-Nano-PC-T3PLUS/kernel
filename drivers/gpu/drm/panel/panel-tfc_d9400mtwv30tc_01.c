/*
 * MIPI-DSI based TFC_D9400MTWV30TC_01  LCD panel driver.
 *
 *
 * Three Five Displays
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/backlight.h>

#define TFC_D9400MTWV30TC_01_WIDTH_MM		217
#define TFC_D9400MTWV30TC_01_HEIGHT_MM		136

struct TFC_D9400MTWV30TC_01 {
	struct device *dev;
	struct drm_panel panel;

	struct regulator_bulk_data supplies[2];
	int reset_gpio;
	u32 power_on_delay;
	u32 reset_delay;
	u32 init_delay;
	bool flip_horizontal;
	bool flip_vertical;
	struct videomode vm;
	u32 width_mm;
	u32 height_mm;
	bool is_power_on;

	struct backlight_device *bl_dev;
	u8 id[3];
	/* This field is tested by functions directly accessing DSI bus before
	 * transfer, transfer is skipped if it is set. In case of transfer
	 * failure or unexpected response the field is set to error value.
	 * Such construct allows to eliminate many checks in higher level
	 * functions.
	 */
	int error;
};

static inline struct lt101mb *panel_to_TFC_D9400MTWV30TC_01(struct drm_panel *panel)
{
	return container_of(panel, struct TFC_D9400MTWV30TC_01, panel);
}

static int TFC_D9400MTWV30TC_01_clear_error(struct TFC_D9400MTWV30TC_01b *ctx)
{
	int ret = ctx->error;

	ctx->error = 0;
	return ret;
}

static void TFC_D9400MTWV30TC_01_dcs_write(struct TFC_D9400MTWV30TC_01 *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing dcs seq: %*ph\n", ret,
			(int)len, data);
		ctx->error = ret;
	}
}

#define TFC_D9400MTWV30TC_01_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	TFC_D9400MTWV30TC_01_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	TFC_D9400MTWV30TC_01_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static void TFC_D9400MTWV30TC_01_set_sequence(struct TFC_D9400MTWV30TC_01 *ctx)
{
	if (ctx->error != 0)
		return;

	usleep_range(17000, 18000);

	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0x11); // Exit Sleep Mode
	mdelay(200);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10); // Select Command 2 Bk 0 Mode
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xC0, 0x3B, 0x00); // Set Display Line to 59 Default is 107
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xC1, 0x0D, 0x0C); // Set V-Bk Pch to 13 and Front to 12
	mdelay(16);	
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xC2, 0x21, 0x08); // Set V-Bk Pch to 13 and Front to 12
	mdelay(16);	
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18); // Set Pos Gamma
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB1, 0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18); // Set Neg Gamma
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11); // Select Command 2 Bk 1 Mode
	mdelay(16);	
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB0, 0x60); // Vop set to 4.73 Volts
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB1, 0x30); // Vcom set to 0.7 Volts
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB2, 0x87); // VGH set to 15 Volts
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB3, 0x80); // Test Command
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB5, 0x49); // VGL set to -10.17 Volts
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB7, 0x85); // Gamma Op set to Mid range
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xB8, 0x21); // AVDD set to 6.6 Volts
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xC1, 0x78); // Preset Timing to 1.6uSec
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xC2, 0x78); // Source EQ2 set to 6.4uSec
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE0, 0x00, 0x1B, 0x02); // The following are Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE1, 0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0. 0x00, 0x00, 0x00, 0x44, 0x44); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE2, 0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE3, 0x00, 0x00, 0x11, 0x11); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE4, 0x44, 0x44); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE5, 0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE6, 0x00, 0x00, 0x11, 0x11); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE7, 0x44, 0x44); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xE8, 0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xEB, 0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xEC, 0x3C, 0x00); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xED, 0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00); // Gate Commands
	mdelay(16);
	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, 0x29); //Display On
	mdelay(200);

}

static int TFC_D9400MTWV30TC_01_power_on(struct TFC_D9400MTWV30TC_01 *ctx)
{
	int ret;

	if (ctx->is_power_on)
		return 0;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	msleep(ctx->power_on_delay);

	gpio_direction_output(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpio_set_value(ctx->reset_gpio, 0);
	msleep(100);
	gpio_set_value(ctx->reset_gpio, 1);
	msleep(10);
	msleep(ctx->reset_delay);

	ctx->is_power_on = true;

	return 0;
}

static int TFC_D9400MTWV30TC_01_power_off(struct TFC_D9400MTWV30TC_01 *ctx)
{
	if (!ctx->is_power_on)
		return 0;

	gpio_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);

	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	ctx->is_power_on = false;

	return 0;
}

static int TFC_D9400MTWV30TC_01_disable(struct drm_panel *panel)
{
	struct TFC_D9400MTWV30TC_01 *ctx = panel_to_lt101mb(panel);

	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	if (ctx->error != 0)
		return ctx->error;

	msleep(35);

	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	if (ctx->error != 0)
		return ctx->error;

	msleep(125);
	if (ctx->bl_dev) {
		ctx->bl_dev->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->bl_dev);
	}

	return 0;
}

static int TFC_D9400MTWV30TC_01_unprepare(struct drm_panel *panel)
{
	struct TFC_D9400MTWV30TC_01 *ctx = panel_to_TFC_D9400MTWV30TC_01(panel);
	int ret;

	ret = TFC_D9400MTWV30TC_01_power_off(ctx);
	if (ret)
		return ret;

	TFC_D9400MTWV30TC_01_clear_error(ctx);

	return 0;
}

static int TFC_D9400MTWV30TC_01_prepare(struct drm_panel *panel)
{
	struct TFC_D9400MTWV30TC_01 *ctx = panel_to_TFC_D9400MTWV30TC_01(panel);
	int ret;

	ret = TFC_D9400MTWV30TC_01_power_on(ctx);
	if (ret < 0)
		return ret;

	TFC_D9400MTWV30TC_01_set_sequence(ctx);
	ret = ctx->error;

	if (ret < 0)
		TFC_D9400MTWV30TC_01_unprepare(panel);

	return ret;
}

static int TFC_D9400MTWV30TC_01_enable(struct drm_panel *panel)
{
	struct TFC_D9400MTWV30TC_01 *ctx = panel_to_TFC_D9400MTWV30TC_01(panel);

	if (ctx->bl_dev) {
		ctx->bl_dev->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->bl_dev);
	}

	TFC_D9400MTWV30TC_01_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_ON);
	if (ctx->error != 0)
		return ctx->error;

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 1535985,
	.hdisplay = 1920,
	.hsync_start = 1920 + 466,
	.hsync_end = 1920 + 466 + 11,
	.htotal = 1920 + 466 + 11 + 11,
	.vdisplay = 1200,
	.vsync_start = 1200 + 10,
	.vsync_end = 1200 + 10 + 5,
	.vtotal = 1200 + 10 + 5 + 5,
	.vrefresh = 0,
};

static int TFC_D9400MTWV30TC_01_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct TFC_D9400MTWV30TC_01 *ctx = panel_to_TFC_D9400MTWV30TC_01(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		DRM_ERROR("failed to create a new display mode\n");
		return 0;
	}

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	drm_mode_set_name(mode);
	mode->width_mm = ctx->width_mm;
	mode->height_mm = ctx->height_mm;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs TFC_D9400MTWV30TC_01_drm_funcs = {
	.disable = TFC_D9400MTWV30TC_01_disable,
	.unprepare = TFC_D9400MTWV30TC_01_unprepare,
	.prepare = TFC_D9400MTWV30TC_01_prepare,
	.enable = TFC_D9400MTWV30TC_01_enable,
	.get_modes = TFC_D9400MTWV30TC_01_get_modes,
};

static int TFC_D9400MTWV30TC_01_parse_dt(struct TFC_D9400MTWV30TC_01 *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *np = dev->of_node;

	of_property_read_u32(np, "power-on-delay", &ctx->power_on_delay);
	of_property_read_u32(np, "reset-delay", &ctx->reset_delay);
	of_property_read_u32(np, "init-delay", &ctx->init_delay);

	ctx->flip_horizontal = of_property_read_bool(np, "flip-horizontal");
	ctx->flip_vertical = of_property_read_bool(np, "flip-vertical");

	return 0;
}

static int TFC_D9400MTWV30TC_01_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *backlight;
	struct TFC_D9400MTWV30TC_01 *ctx;
	int ret;

	if (!drm_panel_connected("TFC_D9400MTWV30TC_01"))
		return -ENODEV;

	ctx = devm_kzalloc(dev, sizeof(struct TFC_D9400MTWV30TC_01), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	ctx->is_power_on = false;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO
		| MIPI_DSI_MODE_VIDEO_HFP | MIPI_DSI_MODE_VIDEO_HBP
		| MIPI_DSI_MODE_VIDEO_HSA | MIPI_DSI_MODE_VSYNC_FLUSH;

	ret = TFC_D9400MTWV30TC_01_parse_dt(ctx);
	if (ret < 0)
		return ret;

	ctx->supplies[0].supply = "vdd3";
	ctx->supplies[1].supply = "vci";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		dev_warn(dev, "failed to get regulators: %d\n", ret);

	ctx->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpio", 0);
	if (ctx->reset_gpio < 0) {
		dev_err(dev, "cannot get reset-gpios %d\n",
			ctx->reset_gpio);
		return ctx->reset_gpio;
	}

	ret = devm_gpio_request(dev, ctx->reset_gpio, "reset-gpio");
	if (ret) {
		dev_err(dev, "failed to request reset-gpio\n");
		return ret;
	}

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->bl_dev = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->bl_dev)
			return -EPROBE_DEFER;
	}

	ctx->width_mm = TFC_D9400MTWV30TC_01_WIDTH_MM;
	ctx->height_mm = TFC_D9400MTWV30TC_01_HEIGHT_MM;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &TFC_D9400MTWV30TC_01_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0) {
		backlight_device_unregister(ctx->bl_dev);
		return ret;
	}

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		backlight_device_unregister(ctx->bl_dev);
		drm_panel_remove(&ctx->panel);
	}

	return ret;
}

static int TFC_D9400MTWV30TC_01_remove(struct mipi_dsi_device *dsi)
{
	struct TFC_D9400MTWV30TC_01 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	backlight_device_unregister(ctx->bl_dev);
	TFC_D9400MTWV30TC_01_power_off(ctx);

	return 0;
}

static void TFC_D9400MTWV30TC_01_shutdown(struct mipi_dsi_device *dsi)
{
	struct TFC_D9400MTWV30TC_01 *ctx = mipi_dsi_get_drvdata(dsi);

	TFC_D9400MTWV30TC_01_power_off(ctx);
}

static const struct of_device_id TFC_D9400MTWV30TC_01_of_match[] = {
	{ .compatible = "TFC_D9400MTWV30TC_0102000" },
	{ }
};
MODULE_DEVICE_TABLE(of, TFC_D9400MTWV30TC_01_of_match);

static struct mipi_dsi_driver TFC_D9400MTWV30TC_01_driver = {
	.probe = TFC_D9400MTWV30TC_01_probe,
	.remove = TFC_D9400MTWV30TC_01_remove,
	.shutdown = TFC_D9400MTWV30TC_01_shutdown,
	.driver = {
		.name = "panel-TFC_D9400MTWV30TC_0102000",
		.of_match_table = TFC_D9400MTWV30TC_01_of_match,
	},
};
module_mipi_dsi_driver(TFC_D9400MTWV30TC_01_driver);

MODULE_AUTHOR("Three-Five Displays");
MODULE_DESCRIPTION("MIPI-DSI based TFC_D9400MTWV30TC_01 LCD Panel Driver");
MODULE_LICENSE("GPL v2");
