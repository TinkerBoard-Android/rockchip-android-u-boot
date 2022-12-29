#ifndef INTERFACE_OVERLAY_H
#define INTERFACE_OVERLAY_H

struct hw_config
{
	int valid;

#ifdef CONFIG_ROCKCHIP_RK3288
	int fiq_debugger;
	int i2c1, i2c4;
	int spi0, spi2;
	int pwm2, pwm3;
	int uart1, uart2, uart3, uart4;
	int pcm_i2s;
#endif

#ifdef CONFIG_ROCKCHIP_RK3399
	int i2c6, i2c7;
	int uart0, uart4;
	int i2s0;
	int spi1, spi5;
	int pwm0, pwm1, pwm3a;
	int spdif;
	int test_clkout2;
	int gmac;
#endif

#ifdef CONFIG_ROCKCHIP_RK3568
	int uart4, uart9;
	int i2c5, i2s3_2ch, spi3, spdif_8ch;
	int pwm12, pwm13, pwm14, pwm15;
#endif
	int auto_ums;

	int overlay_count;
	char **overlay_file;
};

void parse_cmdline(void);

void parse_hw_config(struct hw_config *);

struct fdt_header *resize_working_fdt(void);

void handle_hw_conf(cmd_tbl_t *, struct fdt_header *, struct hw_config *);

#endif
