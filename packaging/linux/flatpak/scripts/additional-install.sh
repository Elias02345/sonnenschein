#!/bin/sh

# User Service
mkdir -p ~/.config/systemd/user
cp "/app/share/sonnenschein/systemd/user/sonnenschein.service" "$HOME/.config/systemd/user/sonnenschein.service"
echo "Sonnenschein User Service has been installed."
echo "Use [systemctl --user enable sonnenschein] once to autostart Sonnenschein on login."

# Load uhid (DS5 emulation)
UHID=$(cat /app/share/sonnenschein/modules-load.d/60-sonnenschein.conf)
echo "Enabling DS5 emulation."
flatpak-spawn --host pkexec sh -c "echo '$UHID' > /etc/modules-load.d/60-sonnenschein.conf"
flatpak-spawn --host pkexec modprobe uhid

# Udev rule
UDEV=$(cat /app/share/sonnenschein/udev/rules.d/60-sonnenschein.rules)
echo "Configuring mouse permission."
flatpak-spawn --host pkexec sh -c "echo '$UDEV' > /etc/udev/rules.d/60-sonnenschein.rules"
echo "Restart computer for mouse permission to take effect."
