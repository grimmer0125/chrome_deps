#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Version = @@VERSION@@

HELPERTOOLS=/Library/PrivilegedHelperTools
NAME=org.chromium.chromoting
CONFIG_FILE="$HELPERTOOLS/$NAME.json"
PLIST=/Library/LaunchAgents/org.chromium.chromoting.plist
PAM_CONFIG=/etc/pam.d/chrome-remote-desktop
ENABLED_FILE="$HELPERTOOLS/$NAME.me2me_enabled"
ENABLED_FILE_BACKUP="$ENABLED_FILE.backup"

KSADMIN=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
KSUPDATE=https://tools.google.com/service/update2
KSPID=com.google.chrome_remote_desktop
KSPVERSION=@@VERSION@@

trap onexit ERR

function onexit {
  # Log an error but don't report an install failure if this script has errors.
  logger An error occurred while launching the service
  exit 0
}

logger Running Chrome Remote Desktop postflight script @@VERSION@@

# If there is a backup _enabled file, re-enable the service.
if [[ -f "$ENABLED_FILE_BACKUP" ]]; then
  mv "$ENABLED_FILE_BACKUP" "$ENABLED_FILE"
fi

# Create the PAM configuration unless it already exists and has been edited.
update_pam=1
CONTROL_LINE="# If you edit this file, please delete this line."
if [[ -f "$PAM_CONFIG" ]] && ! grep -qF "$CONTROL_LINE" "$PAM_CONFIG"; then
  update_pam=0
fi

if [[ "$update_pam" == "1" ]]; then
  logger Creating PAM config.
  cat > "$PAM_CONFIG" <<EOF
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

auth       required   pam_deny.so
account    required   pam_permit.so
password   required   pam_deny.so
session    required   pam_deny.so

# This file is auto-updated by the Chrome Remote Desktop installer.
$CONTROL_LINE
EOF
else
  logger PAM config has local edits. Not updating.
fi

# Load the service.
# The launchctl command we'd like to run:
#   launchctl load -w -S Aqua $PLIST
# However, since we're installing as an admin, the launchctl command is run
# as if it had a sudo prefix, which means it tries to load the service in
# system rather than user space.
# To launch the service in user space, we need to get the current user (using
# ps and grepping for the loginwindow.app) and run the launchctl cmd as that
# user (using launchctl bsexec).
set `ps aux | grep loginwindow.app | grep -v grep`
USERNAME=$1
USERID=$2
if [[ -n "$USERNAME" && -n "$USERID" ]]; then
  launchctl bsexec "$USERID" sudo -u "$USERNAME" launchctl load -w -S Aqua "$PLIST"
fi

# Register a ticket with Keystone so we're updated.
$KSADMIN --register --productid "$KSPID" --version "$KSPVERSION" --xcpath "$PLIST" --url "$KSUPDATE"
