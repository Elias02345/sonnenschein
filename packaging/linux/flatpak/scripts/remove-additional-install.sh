#!/bin/sh

# User Service
systemctl --user stop sonnenschein
rm "$HOME/.config/systemd/user/sonnenschein.service"
systemctl --user daemon-reload
echo "Sonnenschein User Service has been removed."

# Remove rules
flatpak-spawn --host pkexec sh -c "rm /etc/modules-load.d/60-sonnenschein.conf"
flatpak-spawn --host pkexec sh -c "rm /etc/udev/rules.d/60-sonnenschein.rules"
echo "Input rules removed. Restart computer to take effect."
