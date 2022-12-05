/*
 * (C) Copyright 2008-2017 Fuzhou Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <drm/drm_mipi_dsi.h>

#include <config.h>
#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <video.h>
#include <backlight.h>
#include <asm/gpio.h>
#include <dm/device.h>
#include <dm/read.h>
#include <dm/uclass.h>
#include <dm/uclass-id.h>
#include <linux/media-bus-format.h>
#include <power/regulator.h>

#include "rockchip_display.h"
#include "rockchip_crtc.h"
#include "rockchip_connector.h"
#include "rockchip_panel.h"

#include <i2c.h>

extern bool powertip_panel_connected;
extern bool rpi_panel_connected;
extern unsigned int panel_i2c_busnum;
extern unsigned int powertip_id;

#if defined(CONFIG_DRM_I2C_LT9211)
extern bool lt9211_is_connected(void);
extern void lt9211_bridge_enable(void);
extern void lt9211_bridge_disable(void);
extern void lt9211_lvds_power_on(void);
extern void lt9211_lvds_power_off(void);
#else
static bool lt9211_is_connected(void) {
       return false;
}
extern void lt9211_bridge_enable(void) {
       return;
}
extern void lt9211_bridge_disable(void) {
       return;
}
extern void lt9211_lvds_power_on(void) {
       return;
}
extern void lt9211_lvds_power_off(void) {
       return;
}
#endif

struct pwseq {
       unsigned int t1;//VCC on to start lvds signal
       unsigned int t2;//LVDS signal(start) to turn Backlihgt on
       unsigned int t3;//Backlihgt(off) to stop lvds signal
       unsigned int t4;//LVDS signal to turn VCC off
       unsigned int t5;//VCC off to turn VCC on
};

struct rockchip_cmd_header {
	u8 data_type;
	u8 delay_ms;
	u8 payload_length;
} __packed;

struct rockchip_cmd_desc {
	struct rockchip_cmd_header header;
	const u8 *payload;
};

struct rockchip_panel_cmds {
	struct rockchip_cmd_desc *cmds;
	int cmd_cnt;
};

struct rockchip_panel_plat {
	bool power_invert;
	u32 bus_format;
	unsigned int bpc;
	struct pwseq pwseq_delay;

	struct {
		unsigned int prepare;
		unsigned int unprepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int reset;
		unsigned int init;
	} delay;

	struct rockchip_panel_cmds *on_cmds;
	struct rockchip_panel_cmds *off_cmds;
};

struct rockchip_panel_priv {
	bool prepared;
	bool enabled;
	struct udevice *power_supply;
	struct udevice *backlight;
	struct gpio_desc enable_gpio;
	struct gpio_desc reset_gpio;

	int cmd_type;
	struct gpio_desc spi_sdi_gpio;
	struct gpio_desc spi_scl_gpio;
	struct gpio_desc spi_cs_gpio;
};

void panel_i2c_reg_write(struct udevice *dev, uint offset, uint value)
{
	#define PANEL_I2C_WRITE_RETRY_COUNT (6)
	int ret = 0;
	int i = 0;

	do {
		ret = dm_i2c_reg_write(dev, offset, value);
		if (ret < 0) {
			printf("panel_i2c_reg_write fail, reg = %x value = %x  i = %d ret = %d\n", offset, value, i, ret);
			mdelay(20);
		}
	} while ((++i <= PANEL_I2C_WRITE_RETRY_COUNT) && (ret < 0));
}

int  panel_i2c_reg_read(struct udevice *dev, uint offset)
{
	#define PANEL_I2C_READ_RETRY_COUNT (3)
	int ret = 0;
	int i = 0;

	do {
		ret = dm_i2c_reg_read(dev, offset);
		if (ret < 0) {
			printf("panel_i2c_reg_read fail, i = %d  reg = %x ret = %d\n", i, offset, ret);
			mdelay(20);
		} else
			return ret;
	} while ((++i <= PANEL_I2C_READ_RETRY_COUNT) && (ret < 0));

	return ret;
}

static inline int get_panel_cmd_type(const char *s)
{
	if (!s)
		return -EINVAL;

	if (strncmp(s, "spi", 3) == 0)
		return CMD_TYPE_SPI;
	else if (strncmp(s, "mcu", 3) == 0)
		return CMD_TYPE_MCU;

	return CMD_TYPE_DEFAULT;
}

static int rockchip_panel_parse_cmds(const u8 *data, int length,
				     struct rockchip_panel_cmds *pcmds)
{
	int len;
	const u8 *buf;
	const struct rockchip_cmd_header *header;
	int i, cnt = 0;

	/* scan commands */
	cnt = 0;
	buf = data;
	len = length;
	while (len > sizeof(*header)) {
		header = (const struct rockchip_cmd_header *)buf;
		buf += sizeof(*header) + header->payload_length;
		len -= sizeof(*header) + header->payload_length;
		cnt++;
	}

	pcmds->cmds = calloc(cnt, sizeof(struct rockchip_cmd_desc));
	if (!pcmds->cmds)
		return -ENOMEM;

	pcmds->cmd_cnt = cnt;

	buf = data;
	len = length;
	for (i = 0; i < cnt; i++) {
		struct rockchip_cmd_desc *desc = &pcmds->cmds[i];

		header = (const struct rockchip_cmd_header *)buf;
		length -= sizeof(*header);
		buf += sizeof(*header);
		desc->header.data_type = header->data_type;
		desc->header.delay_ms = header->delay_ms;
		desc->header.payload_length = header->payload_length;
		desc->payload = buf;
		buf += header->payload_length;
		length -= header->payload_length;
	}

	return 0;
}

static void rockchip_panel_write_spi_cmds(struct rockchip_panel_priv *priv,
					  u8 type, int value)
{
	int i;

	dm_gpio_set_value(&priv->spi_cs_gpio, 0);

	if (type == 0)
		value &= (~(1 << 8));
	else
		value |= (1 << 8);

	for (i = 0; i < 9; i++) {
		if (value & 0x100)
			dm_gpio_set_value(&priv->spi_sdi_gpio, 1);
		else
			dm_gpio_set_value(&priv->spi_sdi_gpio, 0);

		dm_gpio_set_value(&priv->spi_scl_gpio, 0);
		udelay(10);
		dm_gpio_set_value(&priv->spi_scl_gpio, 1);
		value <<= 1;
		udelay(10);
	}

	dm_gpio_set_value(&priv->spi_cs_gpio, 1);
}

static int rockchip_panel_send_mcu_cmds(struct rockchip_panel *panel, struct display_state *state,
					struct rockchip_panel_cmds *cmds)
{
	int i;

	if (!cmds)
		return -EINVAL;

	display_send_mcu_cmd(state, MCU_SETBYPASS, 1);
	for (i = 0; i < cmds->cmd_cnt; i++) {
		struct rockchip_cmd_desc *desc = &cmds->cmds[i];
		int value = 0;

		value = desc->payload[0];
		display_send_mcu_cmd(state, desc->header.data_type, value);

		if (desc->header.delay_ms)
			mdelay(desc->header.delay_ms);
	}
	display_send_mcu_cmd(state, MCU_SETBYPASS, 0);

	return 0;
}

static int rockchip_panel_send_spi_cmds(struct rockchip_panel *panel, struct display_state *state,
					struct rockchip_panel_cmds *cmds)
{
	struct rockchip_panel_priv *priv = dev_get_priv(panel->dev);
	int i;

	if (!cmds)
		return -EINVAL;

	for (i = 0; i < cmds->cmd_cnt; i++) {
		struct rockchip_cmd_desc *desc = &cmds->cmds[i];
		int value = 0;

		if (desc->header.payload_length == 2)
			value = (desc->payload[0] << 8) | desc->payload[1];
		else
			value = desc->payload[0];
		rockchip_panel_write_spi_cmds(priv,
					      desc->header.data_type, value);

		if (desc->header.delay_ms)
			mdelay(desc->header.delay_ms);
	}

	return 0;
}

static int rockchip_panel_send_dsi_cmds(struct mipi_dsi_device *dsi,
					struct rockchip_panel_cmds *cmds)
{
	int i, ret;
	struct drm_dsc_picture_parameter_set *pps = NULL;

	if (!cmds)
		return -EINVAL;

	for (i = 0; i < cmds->cmd_cnt; i++) {
		struct rockchip_cmd_desc *desc = &cmds->cmds[i];
		const struct rockchip_cmd_header *header = &desc->header;

		switch (header->data_type) {
		case MIPI_DSI_COMPRESSION_MODE:
			ret = mipi_dsi_compression_mode(dsi, desc->payload[0]);
			break;
		case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
		case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
		case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
		case MIPI_DSI_GENERIC_LONG_WRITE:
			ret = mipi_dsi_generic_write(dsi, desc->payload,
						     header->payload_length);
			break;
		case MIPI_DSI_DCS_SHORT_WRITE:
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		case MIPI_DSI_DCS_LONG_WRITE:
			ret = mipi_dsi_dcs_write_buffer(dsi, desc->payload,
							header->payload_length);
			break;
		case MIPI_DSI_PICTURE_PARAMETER_SET:
			pps = kzalloc(sizeof(*pps), GFP_KERNEL);
			if (!pps)
				return -ENOMEM;

			memcpy(pps, desc->payload, header->payload_length);
			ret = mipi_dsi_picture_parameter_set(dsi, pps);
			kfree(pps);
			break;
		default:
			printf("unsupport command data type: %d\n",
			       header->data_type);
			return -EINVAL;
		}

		if (ret < 0) {
			printf("failed to write cmd%d: %d\n", i, ret);
			return ret;
		}

		if (header->delay_ms)
			mdelay(header->delay_ms);
	}

	return 0;
}
struct rockchip_panel *g_panel=NULL;

static void panel_simple_prepare(struct rockchip_panel *panel)
{
	struct rockchip_panel_plat *plat = dev_get_platdata(panel->dev);
	struct rockchip_panel_priv *priv = dev_get_priv(panel->dev);
	struct mipi_dsi_device *dsi = dev_get_parent_platdata(panel->dev);
	struct udevice *dev;
	int ret;

	if(!g_panel)
		g_panel = panel;

	printf("panel_simple_prepaer\n");

	if (priv->prepared)
		return;

    if (lt9211_is_connected()) {
		lt9211_lvds_power_on();
        if(plat->pwseq_delay.t1)
            mdelay(plat->pwseq_delay.t1);//lvds power on to lvds signal
    }

	if (powertip_panel_connected) {
		i2c_get_chip_for_busnum(panel_i2c_busnum, 0x36, 1, &dev);
		panel_i2c_reg_write(dev, 0x5, 0x3);
		mdelay(20);
		panel_i2c_reg_write(dev, 0x5, 0x0);
		mdelay(20);
		panel_i2c_reg_write(dev, 0x5, 0x3);
		mdelay(200);
		printf("panel_simple_prepare powertip powerting on\n");
	}

	if (priv->power_supply)
		regulator_set_enable(priv->power_supply, !plat->power_invert);

	if (dm_gpio_is_valid(&priv->enable_gpio))
		dm_gpio_set_value(&priv->enable_gpio, 1);

	if (plat->delay.prepare)
		mdelay(plat->delay.prepare);

	if (dm_gpio_is_valid(&priv->reset_gpio))
		dm_gpio_set_value(&priv->reset_gpio, 1);

	if (plat->delay.reset)
		mdelay(plat->delay.reset);

	if (dm_gpio_is_valid(&priv->reset_gpio))
		dm_gpio_set_value(&priv->reset_gpio, 0);

	if (plat->delay.init)
		mdelay(plat->delay.init);

	if (!rpi_panel_connected && plat->on_cmds) {
		if (priv->cmd_type == CMD_TYPE_SPI)
			ret = rockchip_panel_send_spi_cmds(panel, panel->state,
							   plat->on_cmds);
		else if (priv->cmd_type == CMD_TYPE_MCU)
			ret = rockchip_panel_send_mcu_cmds(panel, panel->state,
							   plat->on_cmds);
		else
			ret = rockchip_panel_send_dsi_cmds(dsi, plat->on_cmds);
		if (ret)
			printf("failed to send on cmds: %d\n", ret);
	}

	priv->prepared = true;
}

static void panel_simple_unprepare(struct rockchip_panel *panel)
{
	struct rockchip_panel_plat *plat = dev_get_platdata(panel->dev);
	struct rockchip_panel_priv *priv = dev_get_priv(panel->dev);
	struct mipi_dsi_device *dsi = dev_get_parent_platdata(panel->dev);
	int ret;

	if (!priv->prepared)
		return;

	if (plat->off_cmds) {
		if (priv->cmd_type == CMD_TYPE_SPI)
			ret = rockchip_panel_send_spi_cmds(panel, panel->state,
							   plat->off_cmds);
		else if (priv->cmd_type == CMD_TYPE_MCU)
			ret = rockchip_panel_send_mcu_cmds(panel, panel->state,
							   plat->off_cmds);
		else
			ret = rockchip_panel_send_dsi_cmds(dsi, plat->off_cmds);
		if (ret)
			printf("failed to send off cmds: %d\n", ret);
	}

	if (dm_gpio_is_valid(&priv->reset_gpio))
		dm_gpio_set_value(&priv->reset_gpio, 1);

	if (dm_gpio_is_valid(&priv->enable_gpio))
		dm_gpio_set_value(&priv->enable_gpio, 0);

	if (priv->power_supply)
		regulator_set_enable(priv->power_supply, plat->power_invert);

	if (plat->delay.unprepare)
		mdelay(plat->delay.unprepare);

	priv->prepared = false;
}

static void panel_simple_enable(struct rockchip_panel *panel)
{
	struct rockchip_panel_plat *plat = dev_get_platdata(panel->dev);
	struct rockchip_panel_priv *priv = dev_get_priv(panel->dev);

	struct mipi_dsi_device *dsi = dev_get_parent_platdata(panel->dev);
	struct udevice *dev;

	printf("panel_simple_enable\n");

	if (lt9211_is_connected()) {
		lt9211_bridge_enable();
        if(plat->pwseq_delay.t2)
            mdelay(plat->pwseq_delay.t2);//lvds signal to turn on backlight
    }

	if (rpi_panel_connected) {
		i2c_get_chip_for_busnum(panel_i2c_busnum, 0x45, 1, &dev);
		panel_i2c_reg_write(dev, 0x85, 0x0);
		mdelay(200);
		panel_i2c_reg_write(dev, 0x85, 0x1);
		mdelay(100);
		panel_i2c_reg_write(dev, 0x81, 0x4);
		printf("panel_simple_prepare rpi powerting on\n");
		mdelay(100);
		rockchip_panel_send_dsi_cmds(dsi, plat->on_cmds);
		mdelay(50);
		panel_i2c_reg_write(dev, 0x86, 255);
		printf("panel_simple_enable rpi backlight on\n");
	} else if (powertip_panel_connected) {
		i2c_get_chip_for_busnum(panel_i2c_busnum, 0x36, 1, &dev);
		mdelay(50);
		panel_i2c_reg_write(dev, 0x6, 0x80|0x0F);
		printf("panel_simple_enable powertip backlight on\n");
		mdelay(50);
		panel_i2c_reg_write(dev, 0x6, 0x80|0x1F);
	}

	if (priv->enabled)
		return;

	if (plat->delay.enable)
		mdelay(plat->delay.enable);

	if (priv->backlight)
		backlight_enable(priv->backlight);

	priv->enabled = true;
}

static void panel_simple_disable(struct rockchip_panel *panel)
{
	struct rockchip_panel_plat *plat = dev_get_platdata(panel->dev);
	struct rockchip_panel_priv *priv = dev_get_priv(panel->dev);
	struct udevice *dev;

	printf("panel_simple_disable\n");
	if (!priv->enabled)
		return;

	if (rpi_panel_connected) {
		i2c_get_chip_for_busnum(panel_i2c_busnum, 0x45, 1, &dev);
		dm_i2c_reg_write(dev, 0x86, 0);
		dm_i2c_reg_write(dev, 0x85, 0x0);
		mdelay(50);
	} else if (powertip_panel_connected) {
		i2c_get_chip_for_busnum(panel_i2c_busnum, 0x36, 1, &dev);
		dm_i2c_reg_write(dev, 0x6, 0);
		dm_i2c_reg_write(dev, 0x5, 0x0);
		mdelay(50);
	}

	if (priv->backlight)
		backlight_disable(priv->backlight);

	if (plat->delay.disable)
		mdelay(plat->delay.disable);

    if (lt9211_is_connected()) {
        if(plat->pwseq_delay.t3)
            mdelay(plat->pwseq_delay.t3);//backlight power off to stop lvds signal
        lt9211_bridge_disable();
               if(plat->pwseq_delay.t4)
            mdelay(plat->pwseq_delay.t4);//stop lvds signal to turn VCC off
               lt9211_lvds_power_off();
               if(plat->pwseq_delay.t5)
            mdelay(plat->pwseq_delay.t5);//lvds power off to turn on lvds power
    }

	priv->enabled = false;
}

static const struct rockchip_panel_funcs rockchip_panel_funcs = {
	.prepare = panel_simple_prepare,
	.unprepare = panel_simple_unprepare,
	.enable = panel_simple_enable,
	.disable = panel_simple_disable,
};

static int rockchip_panel_ofdata_to_platdata(struct udevice *dev)
{
	struct rockchip_panel_plat *plat = dev_get_platdata(dev);
	const void *data;
	int len = 0;
	int ret;

	plat->power_invert = dev_read_bool(dev, "power-invert");

	plat->delay.prepare = dev_read_u32_default(dev, "prepare-delay-ms", 0);
	plat->delay.unprepare = dev_read_u32_default(dev, "unprepare-delay-ms", 0);
	plat->delay.enable = dev_read_u32_default(dev, "enable-delay-ms", 0);
	plat->delay.disable = dev_read_u32_default(dev, "disable-delay-ms", 0);
	plat->delay.init = dev_read_u32_default(dev, "init-delay-ms", 0);
	plat->delay.reset = dev_read_u32_default(dev, "reset-delay-ms", 0);

	plat->bus_format = dev_read_u32_default(dev, "bus-format",
						MEDIA_BUS_FMT_RBG888_1X24);
	plat->bpc = dev_read_u32_default(dev, "bpc", 8);

	data = dev_read_prop(dev, "panel-init-sequence", &len);
	if (data) {
		plat->on_cmds = calloc(1, sizeof(*plat->on_cmds));
		if (!plat->on_cmds)
			return -ENOMEM;

		ret = rockchip_panel_parse_cmds(data, len, plat->on_cmds);
		if (ret) {
			printf("failed to parse panel init sequence\n");
			goto free_on_cmds;
		}
	}

	data = dev_read_prop(dev, "panel-exit-sequence", &len);
	if (data) {
		plat->off_cmds = calloc(1, sizeof(*plat->off_cmds));
		if (!plat->off_cmds) {
			ret = -ENOMEM;
			goto free_on_cmds;
		}

		ret = rockchip_panel_parse_cmds(data, len, plat->off_cmds);
		if (ret) {
			printf("failed to parse panel exit sequence\n");
			goto free_cmds;
		}
	}

	return 0;

free_cmds:
	free(plat->off_cmds);
free_on_cmds:
	free(plat->on_cmds);
	return ret;
}

static int rockchip_panel_probe(struct udevice *dev)
{
	struct rockchip_panel_priv *priv = dev_get_priv(dev);
	struct rockchip_panel_plat *plat = dev_get_platdata(dev);
	struct rockchip_panel *panel;
	int ret;
	const char *cmd_type;
	const void *data;
	int len = 0;

	printf("rockchip_panel_probe\n");
	ret = gpio_request_by_name(dev, "enable-gpios", 0,
				   &priv->enable_gpio, GPIOD_IS_OUT);
	if (ret && ret != -ENOENT) {
		printf("%s: Cannot get enable GPIO: %d\n", __func__, ret);
		return ret;
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0,
				   &priv->reset_gpio, GPIOD_IS_OUT);
	if (ret && ret != -ENOENT) {
		printf("%s: Cannot get reset GPIO: %d\n", __func__, ret);
		return ret;
	}

	if ( !rpi_panel_connected && !powertip_panel_connected) {
		ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
						"backlight", &priv->backlight);
		if (ret && ret != -ENOENT) {
			printf("%s: Cannot get backlight: %d\n", __func__, ret);
			return ret;
		}
	}

	ret = uclass_get_device_by_phandle(UCLASS_REGULATOR, dev,
					   "power-supply", &priv->power_supply);
	if (ret && ret != -ENOENT) {
		printf("%s: Cannot get power supply: %d\n", __func__, ret);
		return ret;
	}

	ret = dev_read_string_index(dev, "rockchip,cmd-type", 0, &cmd_type);
	if (ret)
		priv->cmd_type = CMD_TYPE_DEFAULT;
	else
		priv->cmd_type = get_panel_cmd_type(cmd_type);

	if (priv->cmd_type == CMD_TYPE_SPI) {
		ret = gpio_request_by_name(dev, "spi-sdi-gpios", 0,
					   &priv->spi_sdi_gpio, GPIOD_IS_OUT);
		if (ret && ret != -ENOENT) {
			printf("%s: Cannot get spi sdi GPIO: %d\n",
			       __func__, ret);
			return ret;
		}
		ret = gpio_request_by_name(dev, "spi-scl-gpios", 0,
					   &priv->spi_scl_gpio, GPIOD_IS_OUT);
		if (ret && ret != -ENOENT) {
			printf("%s: Cannot get spi scl GPIO: %d\n",
			       __func__, ret);
			return ret;
		}
		ret = gpio_request_by_name(dev, "spi-cs-gpios", 0,
					   &priv->spi_cs_gpio, GPIOD_IS_OUT);
		if (ret && ret != -ENOENT) {
			printf("%s: Cannot get spi cs GPIO: %d\n",
			       __func__, ret);
			return ret;
		}
		dm_gpio_set_value(&priv->spi_sdi_gpio, 1);
		dm_gpio_set_value(&priv->spi_scl_gpio, 1);
		dm_gpio_set_value(&priv->spi_cs_gpio, 1);
		dm_gpio_set_value(&priv->reset_gpio, 0);
	}

	if (rpi_panel_connected) {
		plat->power_invert = 0;
		plat->delay.prepare = 0;
		plat->delay.unprepare = 0;
		plat->delay.enable = 0;
		plat->delay.disable = 0;
		plat->delay.init = 0;
		plat->delay.reset = 0;
		plat->bus_format = MEDIA_BUS_FMT_RBG888_1X24;
		plat->bpc = 8;
		data = dev_read_prop(dev, "rpi-init-sequence", &len);
		if (data) {
			plat->on_cmds = calloc(1, sizeof(*plat->on_cmds));
			if (plat->on_cmds) {
				ret = rockchip_panel_parse_cmds(data, len, plat->on_cmds);
				if (ret) {
					printf("failed to parse panel init sequence\n");
					free(plat->on_cmds);
				}
			}
		}
	} else if (powertip_panel_connected) {
		plat->power_invert = 0;
		plat->delay.prepare = 0;
		plat->delay.unprepare = 0;
		plat->delay.enable = 0;
		plat->delay.disable = 0;
		plat->delay.init = 0;
		plat->delay.reset = 0;
		plat->bus_format = MEDIA_BUS_FMT_RBG888_1X24;
		plat->bpc = 8;

		if (powertip_id == 0x86) {
			data = dev_read_prop(dev, "powertip-rev-b-init-sequence", &len);
		} else {
			data = dev_read_prop(dev, "powertip-rev-a-init-sequence", &len);
		}
		if (data) {
			plat->on_cmds = calloc(1, sizeof(*plat->on_cmds));
			if (plat->on_cmds) {
				ret = rockchip_panel_parse_cmds(data, len, plat->on_cmds);
				if (ret) {
					printf("failed to parse panel init sequence\n");
					free(plat->on_cmds);
				}
			}
		}

		data = dev_read_prop(dev, "powertip-exit-sequence", &len);
		if (data) {
			plat->off_cmds = calloc(1, sizeof(*plat->off_cmds));
			if (plat->off_cmds) {
				ret = rockchip_panel_parse_cmds(data, len, plat->off_cmds);
				if (ret) {
					printf("failed to parse panel exit sequence\n");
					free(plat->off_cmds);
				}
			}
		}
	}

	#ifdef CONFIG_DRM_I2C_LT9211
    if (lt9211_is_connected()) {
               plat->pwseq_delay.t1 = dev_read_u32_default(dev, "t1", 0);
               plat->pwseq_delay.t2 = dev_read_u32_default(dev, "t2", 0);
               plat->pwseq_delay.t3 = dev_read_u32_default(dev, "t3", 0);
               plat->pwseq_delay.t4 = dev_read_u32_default(dev, "t4", 0);
               plat->pwseq_delay.t5 = dev_read_u32_default(dev, "t5", 0);

        printk("panel_simple_dsi_of_get_desc_data t1=%d t2=%d t3=%d t4=%d t5=%d\n", plat->pwseq_delay.t1,plat->pwseq_delay.t2,plat->pwseq_delay.t3,plat->pwseq_delay.t4,plat->pwseq_delay.t5);
    }
#endif

	panel = calloc(1, sizeof(*panel));
	if (!panel)
		return -ENOMEM;

	dev->driver_data = (ulong)panel;
	panel->dev = dev;
	panel->bus_format = plat->bus_format;
	panel->bpc = plat->bpc;
	panel->funcs = &rockchip_panel_funcs;

	return 0;
}

static const struct udevice_id rockchip_panel_ids[] = {
	{ .compatible = "simple-panel", },
	{ .compatible = "simple-panel-dsi", },
	{}
};

U_BOOT_DRIVER(rockchip_panel) = {
	.name = "rockchip_panel",
	.id = UCLASS_PANEL,
	.of_match = rockchip_panel_ids,
	.ofdata_to_platdata = rockchip_panel_ofdata_to_platdata,
	.probe = rockchip_panel_probe,
	.priv_auto_alloc_size = sizeof(struct rockchip_panel_priv),
	.platdata_auto_alloc_size = sizeof(struct rockchip_panel_plat),
};
