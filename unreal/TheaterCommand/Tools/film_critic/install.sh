#!/bin/bash
# Film critic: switches the night run on (or off) on this Mac. A development tool, not part of the game.
#   bash install.sh          switch it on: a LaunchAgent tries night.sh every half hour from 9 PM to 6 AM
#   bash install.sh off      switch it off (the agent is removed; pictures already taken are kept)
#   bash install.sh status   say whether it is on, and how the last night went
# night.sh decides each time whether it may run (idle Mac, lid open, screen unlocked, mains power, Brian's game
# closed, the Mac builder's lock free) and otherwise leaves at once, so the agent costs nothing when it may not run.
set -euo pipefail
KIT="$(cd "$(dirname "$0")" && pwd)"
LABEL="com.theatercommand.filmcritic"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
NIGHTS="$KIT/../nights"
DOMAIN="gui/$(id -u)"

case "${1:-on}" in
  off)
    launchctl bootout "$DOMAIN/$LABEL" 2> /dev/null || true
    rm -f "$PLIST"
    echo "The film critic is off. Its pictures are kept in $NIGHTS."
    ;;
  status)
    if launchctl print "$DOMAIN/$LABEL" > /dev/null 2>&1; then echo "The film critic is on."; else echo "The film critic is off."; fi
    last=$(ls -1d "$NIGHTS"/20??-??-?? 2> /dev/null | sort | tail -1)
    [ -n "$last" ] && { echo "Last night: $(basename "$last")"; tail -5 "$last/attempts.log" 2> /dev/null; }
    ;;
  on)
    mkdir -p "$HOME/Library/LaunchAgents" "$NIGHTS"
    {
      echo '<?xml version="1.0" encoding="UTF-8"?>'
      echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
      echo '<plist version="1.0"><dict>'
      echo "  <key>Label</key><string>$LABEL</string>"
      echo "  <key>ProgramArguments</key><array><string>/bin/bash</string><string>$KIT/night.sh</string></array>"
      echo '  <key>StartCalendarInterval</key><array>'
      for h in 21 22 23 0 1 2 3 4 5; do
        for m in 7 37; do echo "    <dict><key>Hour</key><integer>$h</integer><key>Minute</key><integer>$m</integer></dict>"; done
      done
      echo '  </array>'
      echo "  <key>StandardOutPath</key><string>$NIGHTS/launchd.log</string>"
      echo "  <key>StandardErrorPath</key><string>$NIGHTS/launchd.log</string>"
      echo '  <key>EnvironmentVariables</key><dict><key>PATH</key><string>/usr/bin:/bin:/usr/sbin:/sbin:/opt/homebrew/bin</string></dict>'
      echo '  <key>ProcessType</key><string>Interactive</string>'
      echo '  <key>AbandonProcessGroup</key><true/>'
      echo '</dict></plist>'
    } > "$PLIST"
    plutil -lint "$PLIST" > /dev/null
    launchctl bootout "$DOMAIN/$LABEL" 2> /dev/null || true
    launchctl bootstrap "$DOMAIN" "$PLIST"
    echo "The film critic is on. From 9 PM it looks every half hour for a quiet Mac (lid open, screen unlocked,"
    echo "plugged in, nobody using it for 20 minutes, the game closed) and films the game for about two hours."
    echo "To switch it off: bash \"$KIT/install.sh\" off"
    ;;
  *) echo "usage: bash install.sh [on|off|status]"; exit 2 ;;
esac
