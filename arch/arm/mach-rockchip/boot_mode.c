/*
 * (C) Copyright 2016 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <boot_rkimg.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/boot_mode.h>
#include <asm/arch-rockchip/gpio.h>
#include <interface_overlay.h>

#define CONFIG_GRF_SOC_STATUS3_REG 0xff77e2ac

#if defined(CONFIG_ROCKCHIP_RK3568)
#define CONFIG_USBPHY_U3_GRF_STATUS_REG  0xfdca00c0
#endif

DECLARE_GLOBAL_DATA_PTR;

enum {
	PH = 0,	/* P: Priority, H: high, M: middle, L: low*/
	PM,
	PL,
};

#ifdef CONFIG_ROCKCHIP_RK3288
/*
*
* usb current limit : GPIO6_A6 (H:unlock, L:lock)
*
*/
void usb_current_limit_unlock(bool unlock_current)
{
	int tmp;

	printf("%s: unlock_current = %d\n", __func__, unlock_current);
	tmp = readl(RKIO_GPIO6_PHYS + GPIO_SWPORT_DR);
	if(unlock_current == true)
		writel(tmp | 0x40, RKIO_GPIO6_PHYS + GPIO_SWPORT_DR);
	else
		writel(tmp & ~0x40, RKIO_GPIO6_PHYS + GPIO_SWPORT_DR);

	tmp = readl(RKIO_GPIO6_PHYS + GPIO_SWPORT_DDR);
	writel(tmp | 0x40, RKIO_GPIO6_PHYS + GPIO_SWPORT_DDR);
}

/*
*
* project id        : GPIO2_A3 GPIO2_A2 GPIO2_A1
* pcb id            : GPIO2_B2 GPIO2_B1 GPIO2_B0
* SDP/CDP           : GPIO6_A5 (H:SDP, L:CDP)
* usb current limit : GPIO6_A6 (H:unlock, L:lock)
* eMMC maskrom mode : GPIO6_A7 (H:disable maskrom, L:enable maskrom)
*
* Please check TRM V1.2 part1 page 152 for the following register settings
*
*/
int check_force_enter_ums_mode(void)
{
	int tmp;

	// GPIO6_A5 pull up/down disable
	tmp = readl(RKIO_GRF_PHYS + GRF_GPIO6A_P);
	writel((tmp&~(0x03<<10)) | 0x03<<(16 + 10), RKIO_GRF_PHYS + GRF_GPIO6A_P);

	// GPIO6_A5 set to input
	tmp = readl(RKIO_GPIO6_PHYS + GPIO_SWPORT_DDR);
	writel(tmp & ~(0x20), RKIO_GPIO6_PHYS + GPIO_SWPORT_DDR);

	mdelay(10);

	printf("PC event = 0x%x\n", readl(RKIO_GPIO6_PHYS + GPIO_EXT_PORT)&0x20);
	if((readl(RKIO_GPIO6_PHYS + GPIO_EXT_PORT)&0x20)==0x20) {
		// SDP detected
		printf("usb connected to SDP, should enter ums mode\n");
		return 1;
	}
	return 0;
}
#endif

static int misc_require_recovery(u32 bcb_offset, int *bcb_recovery_msg)
{
	struct bootloader_message *bmsg;
	struct blk_desc *dev_desc;
	disk_partition_t part;
	int cnt, recovery = 0;

	dev_desc = rockchip_get_bootdev();
	if (!dev_desc) {
		printf("dev_desc is NULL!\n");
		goto out;
	}

	if (part_get_info_by_name(dev_desc, PART_MISC, &part) < 0) {
		printf("No misc partition\n");
		goto out;
	}

	cnt = DIV_ROUND_UP(sizeof(struct bootloader_message), dev_desc->blksz);
	bmsg = memalign(ARCH_DMA_MINALIGN, cnt * dev_desc->blksz);
	if (blk_dread(dev_desc, part.start + bcb_offset, cnt, bmsg) != cnt) {
		recovery = 0;
	} else {
		recovery = !strcmp(bmsg->command, "boot-recovery");
		if (bcb_recovery_msg) {
			if (!strcmp(bmsg->recovery, "recovery\n--rk_fwupdate\n"))
				*bcb_recovery_msg = BCB_MSG_RECOVERY_RK_FWUPDATE;
			else if (!strcmp(bmsg->recovery, "recovery\n--factory_mode=whole") ||
				 !strcmp(bmsg->recovery, "recovery\n--factory_mode=small"))
				*bcb_recovery_msg = BCB_MSG_RECOVERY_PCBA;
		}
	}

	free(bmsg);
out:
	return recovery;
}

int get_bcb_recovery_msg(void)
{
	int bcb_recovery_msg = BCB_MSG_RECOVERY_NONE;
#ifdef CONFIG_ANDROID_BOOT_IMAGE
	u32 bcb_offset = android_bcb_msg_sector_offset();
#else
	u32 bcb_offset = BCB_MESSAGE_BLK_OFFSET;
#endif
	misc_require_recovery(bcb_offset, &bcb_recovery_msg);

	return bcb_recovery_msg;
}

/*
 * There are three ways to get reboot-mode:
 *
 * No1. Android BCB which is defined in misc.img (0KB or 16KB offset)
 * No2. CONFIG_ROCKCHIP_BOOT_MODE_REG that supports "reboot xxx" commands
 * No3. Env variable "reboot_mode" which is added by U-Boot
 *
 * Recovery mode from:
 *	- Android BCB in misc.img
 *	- "reboot recovery" command
 *	- recovery key pressed without usb attach
 */
int rockchip_get_boot_mode(void)
{
	static int boot_mode[] =		/* static */
		{ -EINVAL, -EINVAL, -EINVAL };
	static int bcb_offset = -EINVAL;	/* static */
	uint32_t reg_boot_mode;
#ifdef CONFIG_ROCKCHIP_RK3399
	uint32_t reg_soc_status3;
#endif
#ifdef CONFIG_ROCKCHIP_RK3568
        uint32_t reg_usbphy_u3_status;
#endif
	char *env_reboot_mode;
	int clear_boot_reg = 0;
	int recovery_msg = 0;
#ifdef CONFIG_ANDROID_BOOT_IMAGE
	u32 offset = android_bcb_msg_sector_offset();
#else
	u32 offset = BCB_MESSAGE_BLK_OFFSET;
#endif

	/*
	 * Env variable "reboot_mode" which is added by U-Boot, reading ever time.
	 */
	env_reboot_mode = env_get("reboot_mode");
	if (env_reboot_mode) {
		if (!strcmp(env_reboot_mode, "recovery-key")) {
			printf("boot mode: recovery (key)\n");
			return BOOT_MODE_RECOVERY;
		} else if (!strcmp(env_reboot_mode, "recovery-usb")) {
			printf("boot mode: recovery (usb)\n");
			return BOOT_MODE_RECOVERY;
		} else if (!strcmp(env_reboot_mode, "recovery")) {
			printf("boot mode: recovery (env)\n");
			return BOOT_MODE_RECOVERY;
		} else if (!strcmp(env_reboot_mode, "fastboot")) {
			printf("boot mode: fastboot\n");
			return BOOT_MODE_BOOTLOADER;
		} else if (!strcmp(env_reboot_mode, "normal")) {
			printf("boot mode: normal(env)\n");
			return BOOT_MODE_NORMAL;
		}
	}

	/*
	 * Android BCB special handle:
	 *    Once the Android BCB offset changed, reinitalize "boot_mode[PM]".
	 *
	 * Background:
	 *    1. there are two Android BCB at the 0KB(google) and 16KB(rk)
	 *       offset in misc.img
	 *    2. Android image: return 0KB offset if image version >= 10,
	 *	 otherwise 16KB
	 *    3. Not Android image: return 16KB offset, eg: FIT image.
	 *
	 * To handle the cases of 16KB and 0KB, we reinitial boot_mode[PM] once
	 * Android BCB is changed.
	 *
	 * PH and PL is from boot mode register and reading once.
	 * PM is from misc.img and should be updated if BCB offset is changed.
	 * Return the boot mode according to priority: PH > PM > PL.
	 */
	if (bcb_offset != offset) {
		boot_mode[PM] = -EINVAL;
		bcb_offset = offset;
	}

	/* directly return if there is already valid mode */
	if (boot_mode[PH] != -EINVAL)
		return boot_mode[PH];
	else if (boot_mode[PM] != -EINVAL)
		return boot_mode[PM];
	else if (boot_mode[PL] != -EINVAL)
		return boot_mode[PL];

	/*
	 * Boot mode priority
	 *
	 * Anyway, we should set download boot mode as the highest priority, so:
	 * reboot loader/bootloader/fastboot > misc partition "recovery" > reboot xxx.
	 */
	reg_boot_mode = readl((void *)CONFIG_ROCKCHIP_BOOT_MODE_REG);
	if (reg_boot_mode == BOOT_LOADER) {
		printf("boot mode: loader\n");
		boot_mode[PH] = BOOT_MODE_LOADER;
		clear_boot_reg = 1;
	} else if (reg_boot_mode == BOOT_DFU) {
		printf("boot mode: dfu\n");
		boot_mode[PH] = BOOT_MODE_DFU;
		clear_boot_reg = 1;
	} else if (reg_boot_mode == BOOT_FASTBOOT) {
		printf("boot mode: bootloader\n");
		boot_mode[PH] = BOOT_MODE_BOOTLOADER;
		clear_boot_reg = 1;
	} else if (misc_require_recovery(bcb_offset, &recovery_msg)) {
		printf("boot mode: recovery (misc)\n");
		boot_mode[PM] = BOOT_MODE_RECOVERY;
	} else {
		switch (reg_boot_mode) {
		case BOOT_NORMAL:
			printf("boot mode: normal\n");
			boot_mode[PL] = BOOT_MODE_NORMAL;
			clear_boot_reg = 1;
			break;
		case BOOT_RECOVERY:
			printf("boot mode: recovery (cmd)\n");
			boot_mode[PL] = BOOT_MODE_RECOVERY;
			clear_boot_reg = 1;
			break;
		case BOOT_UMS:
			printf("boot mode: ums\n");
			boot_mode[PL] = BOOT_MODE_UMS;
			clear_boot_reg = 1;
			break;
		case BOOT_CHARGING:
			printf("boot mode: charging\n");
			boot_mode[PL] = BOOT_MODE_CHARGING;
			clear_boot_reg = 1;
			break;
		case BOOT_PANIC:
			printf("boot mode: panic\n");
			boot_mode[PL] = BOOT_MODE_PANIC;
			break;
		case BOOT_WATCHDOG:
			printf("boot mode: watchdog\n");
			boot_mode[PL] = BOOT_MODE_WATCHDOG;
			break;
		default:
			{
			struct hw_config hw_conf;
			memset(&hw_conf, 0, sizeof(struct hw_config));
			parse_hw_config(&hw_conf);

#if defined(CONFIG_ROCKCHIP_RK3399)
			reg_soc_status3 = readl((void *)CONFIG_GRF_SOC_STATUS3_REG);
			if ((reg_soc_status3 & (1 << 12)) && hw_conf.auto_ums != -1 ) {
				printf("usbcphy0_otg_utmi_bvalid = 1\n");
#elif defined(CONFIG_ROCKCHIP_RK3288)
			if (check_force_enter_ums_mode() && hw_conf.auto_ums != -1) {
#elif defined(CONFIG_ROCKCHIP_RK3568)
			reg_usbphy_u3_status = readl((void *)CONFIG_USBPHY_U3_GRF_STATUS_REG);
			if ((reg_usbphy_u3_status & (1 << 9)) && hw_conf.auto_ums != -1) {
				printf("usbotg_utmi_bvalid = 1\n");
#else
			if (0) {
#endif
				printf("auto_ums config enable, enter ums mode\n");
				boot_mode[PH] = BOOT_MODE_UMS;
				clear_boot_reg = 1;
			} else {
				printf("boot mode: None\n");
				boot_mode[PL] = BOOT_MODE_UNDEFINE;
			}
			}
		}
	}

	/*
	 * We don't clear boot mode reg when its value stands for the reboot
	 * reason or others(in the future), the kernel will need and clear it.
	 */
	if (clear_boot_reg)
		writel(BOOT_NORMAL, (void *)CONFIG_ROCKCHIP_BOOT_MODE_REG);

	if (boot_mode[PH] != -EINVAL)
		return boot_mode[PH];
	else if (boot_mode[PM] != -EINVAL)
		return boot_mode[PM];
	else
		return boot_mode[PL];
}

int setup_boot_mode(void)
{
	char env_preboot[256] = {0};

	switch (rockchip_get_boot_mode()) {
	case BOOT_MODE_BOOTLOADER:
		printf("enter fastboot!\n");
#if defined(CONFIG_FASTBOOT_FLASH_MMC_DEV)
		snprintf(env_preboot, 256,
				"setenv preboot; mmc dev %x; fastboot usb 0; ",
				CONFIG_FASTBOOT_FLASH_MMC_DEV);
#elif defined(CONFIG_FASTBOOT_FLASH_NAND_DEV)
		snprintf(env_preboot, 256,
				"setenv preboot; fastboot usb 0; ");
#endif
		env_set("preboot", env_preboot);
		break;
	case BOOT_MODE_UMS:
		printf("enter UMS!\n");
		env_set("preboot", "setenv preboot; ums mmc 0");
		break;
#if defined(CONFIG_CMD_DFU)
	case BOOT_MODE_DFU:
		printf("enter DFU!\n");
		env_set("preboot", "setenv preboot; dfu 0 ${devtype} ${devnum}; rbrom");
		break;
#endif
	case BOOT_MODE_LOADER:
		printf("enter Rockusb!\n");
		env_set("preboot", "setenv preboot; download");
		run_command("download", 0);
		break;
	case BOOT_MODE_CHARGING:
		printf("enter charging!\n");
		env_set("preboot", "setenv preboot; charge");
		break;
	}

	return 0;
}
