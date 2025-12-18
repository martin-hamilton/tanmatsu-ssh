idf_component_register(
	SRCS
		# Application
		"main.c"
		"common/display.c"
		"common/theme.c"
		
		# Menus
		"message_dialog.c"
		"terminal.c"
		"textedit.c"
		"menu_ssh.c"
		"menu_ssh_edit.c"
		"util_ssh.c"
		"settings_ssh.c"

		# Fonts
		"chakrapetchmedium.c"

		# Libraries
		"sdcard.c"
		"device_settings.c"
		"icons.c"
		"device_information.c"
		"filesystem_utils.c"
		"ntp.c"

	PRIV_REQUIRES
		esp_lcd
		fatfs
		nvs_flash
		badge-bsp
		gui
		wpa_supplicant
		esp_https_ota
		app_update
		esp_bootloader_format
		custom-certificates
		wifi-manager
		esp-tls
		pax-gfx
		pax-codecs
		efuse
		timezone
		json
		freertos
		tanmatsu-wifi
		lwip
		badgeteam__terminal-emulator
		${extra_requires}
	INCLUDE_DIRS
		"."
		"include"
	EMBED_FILES
		${files}
)

