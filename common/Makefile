#
# (C) Copyright 2004-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# SPDX-License-Identifier:	GPL-2.0+
#

# core
ifndef CONFIG_SPL_BUILD
obj-y += init/
obj-$(CONFIG_SUPPORT_USBPLUG) += usbplug/

obj-y += main.o
obj-y += exports.o
obj-$(CONFIG_HASH) += hash.o
obj-$(CONFIG_HUSH_PARSER) += cli_hush.o
obj-$(CONFIG_AUTOBOOT) += autoboot.o

# This option is not just y/n - it can have a numeric value
ifdef CONFIG_BOOT_RETRY_TIME
obj-y += bootretry.o
endif

# # boards
obj-y += board_f.o
ifndef CONFIG_SUPPORT_USBPLUG
obj-y += board_r.o
endif
obj-$(CONFIG_DISPLAY_BOARDINFO) += board_info.o
obj-$(CONFIG_DISPLAY_BOARDINFO_LATE) += board_info.o

obj-$(CONFIG_CMD_BOOTM) += bootm.o bootm_os.o
obj-$(CONFIG_CMD_BOOTZ) += bootm.o bootm_os.o
obj-$(CONFIG_CMD_BOOTI) += bootm.o bootm_os.o

obj-$(CONFIG_CMD_BEDBUG) += bedbug.o
obj-$(CONFIG_$(SPL_TPL_)OF_LIBFDT) += fdt_support.o

obj-$(CONFIG_MII) += miiphyutil.o
obj-$(CONFIG_CMD_MII) += miiphyutil.o
obj-$(CONFIG_PHYLIB) += miiphyutil.o

ifdef CONFIG_USB
obj-y += usb.o usb_hub.o
obj-$(CONFIG_USB_STORAGE) += usb_storage.o
endif

# others
obj-$(CONFIG_CONSOLE_MUX) += iomux.o
obj-$(CONFIG_MTD_NOR_FLASH) += flash.o
obj-$(CONFIG_CMD_KGDB) += kgdb.o kgdb_stubs.o
obj-$(CONFIG_I2C_EDID) += edid.o
obj-$(CONFIG_KALLSYMS) += kallsyms.o
obj-y += splash.o
obj-$(CONFIG_SPLASH_SOURCE) += splash_source.o
ifndef CONFIG_DM_VIDEO
obj-$(CONFIG_LCD) += lcd.o lcd_console.o
endif
obj-$(CONFIG_LCD_ROTATION) += lcd_console_rotation.o
obj-$(CONFIG_LCD_DT_SIMPLEFB) += lcd_simplefb.o
obj-$(CONFIG_LYNXKDI) += lynxkdi.o
obj-$(CONFIG_MENU) += menu.o
obj-$(CONFIG_UPDATE_TFTP) += update.o
obj-$(CONFIG_DFU_TFTP) += update.o
obj-$(CONFIG_USB_KEYBOARD) += usb_kbd.o
obj-y += cli_readline.o cli_simple.o

endif # !CONFIG_SPL_BUILD

obj-$(CONFIG_$(SPL_TPL_)BOOTSTAGE) += bootstage.o

ifdef CONFIG_SPL_BUILD
ifndef CONFIG_TPL_BUILD
ifdef CONFIG_SPL_DFU
obj-$(CONFIG_DFU_OVER_USB) += dfu.o
endif
obj-$(CONFIG_SPL_DFU) += cli_hush.o
obj-$(CONFIG_SPL_HASH_SUPPORT) += hash.o
obj-$(CONFIG_SPL_YMODEM_SUPPORT) += xyzModem.o
obj-$(CONFIG_SPL_LOAD_FIT) += common_fit.o
obj-$(CONFIG_SPL_NET_SUPPORT) += miiphyutil.o
endif
obj-$(CONFIG_$(SPL_TPL_)OF_LIBFDT) += fdt_support.o
ifdef CONFIG_SPL_USB_HOST_SUPPORT
obj-$(CONFIG_SPL_USB_SUPPORT) += usb.o usb_hub.o
obj-$(CONFIG_USB_STORAGE) += usb_storage.o
endif
endif

#others
obj-$(CONFIG_DDR_SPD) += ddr_spd.o
obj-$(CONFIG_SPD_EEPROM) += ddr_spd.o
obj-$(CONFIG_HWCONFIG) += hwconfig.o
obj-$(CONFIG_BOUNCE_BUFFER) += bouncebuf.o
ifdef CONFIG_SPL_BUILD
ifdef CONFIG_TPL_BUILD
obj-$(CONFIG_TPL_SERIAL_SUPPORT) += console.o
else
obj-$(CONFIG_SPL_SERIAL_SUPPORT) += console.o
endif
else
obj-y += console.o
endif
obj-$(CONFIG_CROS_EC) += cros_ec.o
obj-y += dlmalloc.o
ifdef CONFIG_SYS_MALLOC_F
ifneq ($(CONFIG_$(SPL_)SYS_MALLOC_F_LEN),0)
obj-y += malloc_simple.o
endif
endif

ifndef CONFIG_TPL_BUILD
obj-y += image.o
endif
obj-$(CONFIG_$(SPL_TPL_)ANDROID_AB) += android_ab.o
obj-$(CONFIG_$(SPL_TPL_)ANDROID_BOOT_IMAGE) += image-android.o
obj-$(CONFIG_$(SPL_TPL_)ANDROID_BOOT_IMAGE) += interface_overlay.o
obj-$(CONFIG_$(SPL_TPL_)ANDROID_BOOTLOADER) += android_bootloader.o

obj-$(CONFIG_$(SPL_TPL_)OF_LIBFDT) += image-fdt.o
ifndef CONFIG_TPL_BUILD
obj-$(CONFIG_$(SPL_TPL_)FIT) += image-fit.o
obj-$(CONFIG_$(SPL_)MULTI_DTB_FIT) += boot_fit.o common_fit.o
obj-$(CONFIG_$(SPL_TPL_)FIT_SIGNATURE) += image-sig.o
endif

obj-y += memsize.o
obj-y += stdio.o

ifdef CONFIG_CMD_DTIMG
obj-y += image-android-dt.o
endif

ifndef CONFIG_SPL_BUILD
# This option is not just y/n - it can have a numeric value
ifdef CONFIG_FASTBOOT_FLASH
obj-y += image-sparse.o
ifdef CONFIG_FASTBOOT_FLASH_MMC_DEV
obj-y += fb_mmc.o
endif
ifdef CONFIG_FASTBOOT_FLASH_NAND_DEV
obj-y += fb_nand.o
endif
endif

ifneq ($(or $(CONFIG_USB_FUNCTION_FASTBOOT),$(CONFIG_UDP_FUNCTION_FASTBOOT)),)
obj-y += fb_common.o
endif
endif

ifdef CONFIG_CMD_EEPROM_LAYOUT
obj-y += eeprom/eeprom_field.o eeprom/eeprom_layout.o
endif

obj-y += cli.o
obj-$(CONFIG_FSL_DDR_INTERACTIVE) += cli_simple.o cli_readline.o
obj-$(CONFIG_CMD_DFU) += dfu.o
obj-y += command.o
obj-$(CONFIG_$(SPL_)LOG) += log.o
obj-$(CONFIG_$(SPL_)LOG_CONSOLE) += log_console.o
obj-y += s_record.o

ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_CMD_LOADB) += xyzModem.o rbsb.o zm.o zreadline.o lrz.o
obj-$(CONFIG_IO_TRACE) += iotrace.o
obj-$(CONFIG_ANDROID_WRITE_KEYBOX) += write_keybox.o
obj-$(CONFIG_ANDROID_KEYMASTER_CA) += keymaster.o
obj-$(CONFIG_ANDROID_KEYMASTER_CA) += attestation_key.o
endif
