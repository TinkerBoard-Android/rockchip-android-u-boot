menu "Boot images"

config ANDROID_BOOT_IMAGE
        bool "Enable support for Android Boot Images"
        default y if FASTBOOT
        help
          This enables support for booting images which use the Android
          image format header.

config FIT
        bool "Support Flattened Image Tree"
        help
          This option allows you to boot the new uImage structure,
          Flattened Image Tree.  FIT is formally a FDT, which can include
          images of various types (kernel, FDT blob, ramdisk, etc.)
          in a single blob.  To boot this new uImage structure,
          pass the address of the blob to the "bootm" command.
          FIT is very flexible, supporting compression, multiple images,
          multiple configurations, verification through hashing and also
          verified boot (secure boot using RSA).

if FIT

config FIT_ENABLE_SHA256_SUPPORT
        bool "Support SHA256 checksum of FIT image contents"
        select SHA256 if !DM_CRYPTO
        default y
        help
          Enable this to support SHA256 checksum of FIT image contents. A
          SHA256 checksum is a 256-bit (32-byte) hash value used to check that
          the image contents have not been corrupted. SHA256 is recommended
          for use in secure applications since (as at 2016) there is no known
          feasible attack that could produce a 'collision' with differing
          input data. Use this for the highest security. Note that only the
          SHA256 variant is supported: SHA512 and others are not currently
          supported in U-Boot.

config FIT_SIGNATURE
        bool "Enable signature verification of FIT uImages"
        depends on DM
        select RSA
        select CONSOLE_DISABLE_CLI
        help
          This option enables signature verification of FIT uImages,
          using a hash signed and verified using RSA. If
          CONFIG_SHA_PROG_HW_ACCEL is defined, i.e support for progressive
          hashing is available using hardware, then the RSA library will use
          it. See doc/uImage.FIT/signature.txt for more details.

          WARNING: When relying on signed FIT images with a required signature
          check the legacy image format is disabled by default, so that
          unsigned images cannot be loaded. If a board needs the legacy image
          format support in this case, enable it using
          CONFIG_IMAGE_FORMAT_LEGACY.

config FIT_ROLLBACK_PROTECT
        bool "Enable rollback-index protection for FIT image"
        depends on FIT_SIGNATURE
        help
          Enable rollback-index protection for FIT image

config FIT_ENABLE_RSASSA_PSS_SUPPORT
        bool "Support rsassa-pss signature scheme of FIT image contents"
        depends on FIT_SIGNATURE
        default y
        help
          Enable this to support the pss padding algorithm as described
          in the rfc8017 (https://tools.ietf.org/html/rfc8017).

config FIT_SIGNATURE_MAX_SIZE
        hex "Max size of signed FIT structures"
        depends on FIT_SIGNATURE
        default 0x10000000
        help
          This option sets a max size in bytes for verified FIT uImages.
          A sane value of 256MB protects corrupted DTB structures from overlapping
          device memory. Assure this size does not extend past expected storage
          space.

config FIT_CIPHER
        bool "Enable ciphering data in a FIT uImages"
        depends on DM
        select AES
        help
          Enable the feature of data ciphering/unciphering in the tool mkimage
          and in the u-boot support of the FIT image.

config FIT_VERBOSE
        bool "Show verbose messages when FIT images fail"
        help
          Generally a system will have valid FIT images so debug messages
          are a waste of code space. If you are debugging your images then
          you can enable this option to get more verbose information about
          failures.

config FIT_BEST_MATCH
        bool "Select the best match for the kernel device tree"
        help
          When no configuration is explicitly selected, default to the
          one whose fdt's compatibility field best matches that of
          U-Boot itself. A match is considered "best" if it matches the
          most specific compatibility entry of U-Boot's fdt's root node.
          The order of entries in the configuration's fdt is ignored.

config FIT_IMAGE_POST_PROCESS
        bool "Enable post-processing of FIT artifacts after loading by U-Boot"
        help
          Allows doing any sort of manipulation to blobs after they got extracted
          from FIT images like stripping off headers or modifying the size of the
          blob, verification, authentication, decryption etc. in a platform or
          board specific way. In order to use this feature a platform or board-
          specific implementation of board_fit_image_post_process() must be
          provided. Also, anything done during this post-processing step would
          need to be comprehended in how the images were prepared before being
          injected into the FIT creation (i.e. the blobs would have been pre-
          processed before being added to the FIT image).

config FIT_HW_CRYPTO
        bool "Enable hardware crypto for FIT image checksum and rsa verify"
        depends on DM_CRYPTO
        help
          Enable hardware crypto for FIT image checksum and rsa verify.

config FIT_PRINT
        bool "Enable fit image structure and data print"
        default y
        help
          Say y here if you want to enable fit image structure and data
          print.

if SPL

config SPL_FIT
        bool "Support Flattened Image Tree within SPL"
        depends on SPL
        select SPL_OF_LIBFDT

config SPL_FIT_SIGNATURE
        bool "Enable signature verification of FIT firmware within SPL"
        depends on SPL_DM
        select SPL_FIT
        select SPL_RSA

config SPL_FIT_ROLLBACK_PROTECT
        bool "Enable SPL rollback-index protection for FIT image"
        depends on SPL_FIT_SIGNATURE
        help
          Enable SPL rollback-index protection for FIT image

config SPL_LOAD_FIT
        bool "Enable SPL loading U-Boot as a FIT"
        select SPL_FIT
        help
          Normally with the SPL framework a legacy image is generated as part
          of the build. This contains U-Boot along with information as to
          where it should be loaded. This option instead enables generation
          of a FIT (Flat Image Tree) which provides more flexibility. In
          particular it can handle selecting from multiple device tree
          and passing the correct one to U-Boot.

config SPL_FIT_IMAGE_POST_PROCESS
        bool "Enable post-processing of FIT artifacts after loading by the SPL"
        depends on SPL_LOAD_FIT
        help
          Allows doing any sort of manipulation to blobs after they got extracted
          from the U-Boot FIT image like stripping off headers or modifying the
          size of the blob, verification, authentication, decryption etc. in a
          platform or board specific way. In order to use this feature a platform
          or board-specific implementation of board_fit_image_post_process() must
          be provided. Also, anything done during this post-processing step would
          need to be comprehended in how the images were prepared before being
          injected into the FIT creation (i.e. the blobs would have been pre-
          processed before being added to the FIT image).

config SPL_FIT_HW_CRYPTO
        bool "Enable SPL hardware crypto for FIT image checksum and rsa verify"
        depends on SPL_DM_CRYPTO
        help
          Enable SPL hardware crypto for FIT image checksum and rsa verify.

config SPL_SYS_DCACHE_OFF
        bool "Disable SPL dcache"
        default y
        help
          Disable SPL dcache. Please make sure CONFIG_SPL_SYS_MALLOC_F_LEN
          is large enough to malloc TLB and bd_t buffer while enabling dcache.

config SPL_FIT_PRINT
        bool "Enable fit image structure and data print in SPL"
        default n
        help
          Say y here if you want to enable fit image structure and data
          print in SPL.

endif # SPL

config SPL_FIT_SOURCE
        string ".its source file for U-Boot FIT image"
        depends on SPL_FIT || FIT
        help
          Specifies a (platform specific) FIT source file to generate the
          U-Boot FIT image. This could specify further image to load and/or
          execute.

config SPL_FIT_GENERATOR
        string ".its file generator script for U-Boot FIT image"
        depends on SPL_FIT || FIT
        default "board/sunxi/mksunxi_fit_atf.sh" if SPL_LOAD_FIT && ARCH_SUNXI
        help
          Specifies a (platform specific) script file to generate the FIT
          source file used to build the U-Boot FIT image file. This gets
          passed a list of supported device tree file stub names to
          include in the generated image.

config SPL_FIT_IMAGE_KB
        int "SPL FIT image size in KiB"
        depends on SPL_FIT || FIT
        default 2048
        help
          SPL FIT image size in KiB, default 2048KB = 1024KB(u-boot) + 1024KB(tee/atf + others).

config SPL_FIT_IMAGE_MULTIPLE
        int "SPL FIT image multiple number"
        depends on SPL_FIT || FIT
        default 2
        help
          SPL FIT image multiple number.

endif # FIT

config OF_BOARD_SETUP
        bool "Set up board-specific details in device tree before boot"
        depends on OF_LIBFDT
        help
          This causes U-Boot to call ft_board_setup() before booting into
          the Operating System. This function can set up various
          board-specific information in the device tree for use by the OS.
          The device tree is then passed to the OS.

config OF_SYSTEM_SETUP
        bool "Set up system-specific details in device tree before boot"
        depends on OF_LIBFDT
        help
          This causes U-Boot to call ft_system_setup() before booting into
          the Operating System. This function can set up various
          system-specific information in the device tree for use by the OS.
          The device tree is then passed to the OS.

config OF_STDOUT_VIA_ALIAS
        bool "Update the device-tree stdout alias from U-Boot"
        depends on OF_LIBFDT
        help
          This uses U-Boot's serial alias from the aliases node to update
          the device tree passed to the OS. The "linux,stdout-path" property
          in the chosen node is set to point to the correct serial node.
          This option currently references CONFIG_CONS_INDEX, which is
          incorrect when used with device tree as this option does not
          exist / should not be used.

config SYS_EXTRA_OPTIONS
        string "Extra Options (DEPRECATED)"
        help
          The old configuration infrastructure (= mkconfig + boards.cfg)
          provided the extra options field. If you have something like
          "HAS_BAR,BAZ=64", the optional options
            #define CONFIG_HAS
            #define CONFIG_BAZ  64
          will be defined in include/config.h.
          This option was prepared for the smooth migration from the old
          configuration to Kconfig. Since this option will be removed sometime,
          new boards should not use this option.

config SYS_TEXT_BASE
        depends on ARC || X86 || ARCH_UNIPHIER || ARCH_ZYNQMP || \
                (M68K && !TARGET_ASTRO_MCF5373L) || MICROBLAZE || MIPS || \
                ARCH_ZYNQ || ARCH_KEYSTONE || ARCH_OMAP2PLUS
        depends on !EFI_APP
        hex "Text Base"
        help
          TODO: Move CONFIG_SYS_TEXT_BASE for all the architecture

        default 0x80800000 if ARCH_OMAP2PLUS


config SYS_CLK_FREQ
        depends on ARC || ARCH_SUNXI
        int "CPU clock frequency"
        help
          TODO: Move CONFIG_SYS_CLK_FREQ for all the architecture

config ARCH_FIXUP_FDT_MEMORY
        bool "Enable arch_fixup_memory_banks() call"
        default y
        help
          Enable FDT memory map syncup before OS boot. This feature can be
          used for booting OS with different memory setup where the part of
          the memory location should be used for different purpose.

endmenu         # Boot images
