# Tanmatsu launcher - now with ssh!

## But what does that mean?

This fork takes the [Tanmatsu launcher](https://github.com/Nicolai-Electronics/tanmatsu-launcher) and adds an integrated `ssh` client using the [ESP-IDF component wrapper for libssh2](https://components.espressif.com/components/skuodi/libssh2_esp/) and the [Badge.Team](https://badge.team) software stack. If you don't know what any of this means, don't worry, but this repo probably isn't for you. No vibe coding was involved, just good vibes :-)

## Big Scary Warning

This software is a minimally functioning proof-of-concept. Right now it lacks some key features that you would want in order to be comfortable using it for real work, important things etc. In particular, host keys / fingerprints are not checked at all. Even once the app is feature complete, you should still think very carefully about which systems and accounts you use with it.

OK, that's enough doom and gloom. On with the show...

## Features

### Done!

- [x] Make `ssh` connection over wifi
- [x] Wait until wifi has initialised successfully before trying to connect via `ssh`
- [x] Authenticate via password
- [x] Rudimentary interactive login session - run commands, see output etc
- [x] Save, edit and delete connection details in system settings
- [x] Exit from `ssh` session back into launcher
- [x] Make cursor visible - it will show up as a thin vertical line
- [x] Figure out why the shell prompt is shown twice - turned out we weren't processing private Escape sequences properly
- [x] Close down socket, free up memory etc at end of session
- [x] Font size +/- using the vol up/down keys
- [x] Control keys work now - yay!
- [x] We no longer flash the launcher by default, you have to `make flash`

### TODO

- [ ] Fix it so that insert works when you use the left arrow to go backwards
- [ ] Figure out why a connection added via `badgelink` isn't visible in the connection list
- [ ] Improve ANSI escape character handling so we can run vi, emacs etc
- [ ] More checks and balances - right now it's still a bit too easy to crash
- [ ] Display meaningful error messages to the user when something goes wrong - don't just log it
- [ ] Display meaningful feedback to the user when the app is doing something
- [ ] Display server fingerprint on first connection - `libssh2` has some useful convenience functions
- [ ] Cache server public key - `libssh2` has some useful convenience functions
- [ ] Check cached public key and warn if there is a mismatch - `libssh2` has some useful convenience functions
- [ ] UI for managing cached public keys - if we use the `libssh2` mechanism they are stored in a text file like on a regular Unix box
- [ ] UI for user to pick their preferred auth method - skeleton code is done for this
- [ ] Support keyboard_interactive auth - or at least prompt the user if they don't have a saved password?
- [ ] Support public key auth - `libssh2` has some useful convenience functions, will need to prompt for passphrase or use the stored "password" to decrypt
- [ ] Let user set terminal type?
- [ ] Light/Dark mode - maybe use a function key to toggle through several presets?
- [ ] Tidy up unused code carried over from Tanmatsu launcher, Nicolai wifi manager etc
- [ ] Find a better icon, or at least scale up the one we're using now. It appears to be a little smaller than the other icons
- [ ] Defaults for the `ssh` app, or on a per connection basis - e.g. font and size multiplier

### Stretch Goals

- [ ] Support for captive portals - coffee shop wifi, trains, hotels etc
- [ ] Support for themes - fg/bg colours, fonts, cursor types, text size etc. Tanmatsu codebase has hooks for this, so we ought to be able to pick up system level themes
- [ ] Encryption at rest for secrets saved in Tanmatsu system config
- [ ] UI for user to pick their preferred / permitted encryption algorithms
- [ ] Support for ssh agent authentication
- [ ] Decouple from the launcher and make into an app in its own right?
- [ ] Port to other devices?

### Testing

- [x] Test with IPv4 host by IP address
- [ ] Test with IPv6 host by IP address
- [ ] Test with DNS entries
- [ ] Test TUI apps with TERM xterm-color, xterm-256color etc, once fancy login session working
- [ ] Check we are disconnecting cleanly from server side, e.g. not leaving phantom sessions behind
- [ ] Check we are freeing memory etc on the Tanmatsu when no longer required
- [ ] Check whether `libssh2_channel_shell` is required
- [ ] Check that I haven't inadvertently committed Licensing Crimes by mixing and matching code from different sources

### Not Doing This

This app is all about interactive remote login, but you can do other stuff with ssh like `scp` and `sftp` file transfer and batch mode remote command execution. This app isn't doing that stuff, at least for now. I think it would be better to have dedicated apps for these other functions. And some sort of shell / REPL for that matter...

## Building

You should be able to build it in the same way as the regular launcher, and it should pull in the `skuodi/libssh2_esp` component at build time. I've had good results using ESP-IDF 5.5 and Python 3.12.7 in case relevant.

## Terminal icon

You'll need to copy the new ICON_TERMINAL image to your FAT filesystem, e.g. using `badgelink`:

```
python3 ./badgelink.py fs upload /int/icons/menu/terminal.png ~/src/tanmatsu-ssh/fat/tanmatsu/icons/menu/terminal.png
```

The terminal icon is [Inca icon Tambo](https://commons.wikimedia.org/wiki/File:Inca_icon_Tambo_32x32.svg), which is public domain (CC0) from Wikimedia Commons. This is resized to 32x32 pixels using ImageMagick's convert tool:

```
convert ~/Downloads/terminal.png -resize 32x32\! -depth 8 -type TrueColor ~/src/tanmatsu-ssh/fat/tanmatsu/icons/menu/terminal.png
```

## SSH session configuration and testing

It's a bit of a pain typing in system connection details using the Tanmatsu keyboard, but you can always use `badgelink` and save yourself some trouble:

```
python3 ./badgelink.py nvs write ssh s00.conn_name str test-server
python3 ./badgelink.py nvs write ssh s00.dest_host str 10.0.0.1
python3 ./badgelink.py nvs write ssh s00.dest_port str 2025
python3 ./badgelink.py nvs write ssh s00.username str tanmatsu-test
python3 ./badgelink.py nvs write ssh s00.password str lol-jk-etc
```

(however new entries that are added entirely via `badgelink` don't seem to show up - so you might find you need to add a new connection using the default values and then change the details via `badgelink`)

You don't have to use your real credentials, ssh server etc. Maybe consider running an sshd on a non-standard port, on a sacrificial VM, creating a test user rather than using your real credentials. Something like this can be handy:

```
sshd -f sshd_config -p 2025
```

One thing that might be useful if you are out and about - you can test on Android by installing `openssh` under Termux, setting a password for the Termux user, creating a wifi hotspot for your Tanmatsu to connect to, and running `sshd` as per above.

# Original Tanmatsu README

[![Build](https://github.com/Nicolai-Electronics/tanmatsu-launcher/actions/workflows/build.yml/badge.svg)](https://github.com/Nicolai-Electronics/tanmatsu-launcher/actions/workflows/build.yml)

A launcher firmware for ESP32 based devices which allows users to configure WiFi, browse apps from an online repository and download and run apps on their devices.

This application supports the following boards:

 - Tanmatsu (including Konsool and Hackerhotel 2026 variants)
 - MCH2022 badge
 - Hackerhotel 2024 badge
 - ESP32-P4 function EV board


## License

This project is made available under the terms of the [MIT license](LICENSE).
