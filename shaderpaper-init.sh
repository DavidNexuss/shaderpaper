#!/bin/sh

USER_CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/shaderpaper"
SYSTEM_CONFIG_DIR="/usr/share/shaderpaper/config"

# Ensure user config dir exists
if [ ! -d "$USER_CONFIG_DIR" ]; then
  echo "Initializing user config at $USER_CONFIG_DIR"
  mkdir -p "$USER_CONFIG_DIR"

  # Option 1: Symlink (saves space, good for read-only defaults)
  ln -s "$SYSTEM_CONFIG_DIR"/* "$USER_CONFIG_DIR"/ 2>/dev/null || {

    # Fallback: Copy if symlinks fail (e.g. cross-filesystem or permission issues)
    echo "Symlink failed, falling back to copying config..."
    cp -a "$SYSTEM_CONFIG_DIR"/* "$USER_CONFIG_DIR"/
  }
else
  echo "User config already exists at $USER_CONFIG_DIR"
fi
