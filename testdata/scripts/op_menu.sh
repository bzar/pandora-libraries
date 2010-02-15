#!/bin/bash
#actions done when the menu button is pressed
#only argument is the time the button was pressed in  seconds

if [ "$1" -ge "3" ]; then #button was pressed 3 sec or longer, show list of apps to kill instead of launcher
  killist=y
fi

xpid=$(pidof X)
if [ $xpid ]; then
  echo "x is running"
  if [ $killist ]; then
    echo "displaying kill list"
    pidlist=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*-\(.*\)(\([0-9]\+\))/\2\n \1/p' | DISPLAY=:0.0 zenity --list --multiple --column "pid" --column "name" --title "kill" --text "which apps should be killed" | sed 's/|/\n/')
    for PID in $pidlist
    do
      kill -9 $PID
    done
  else
  echo "starting appfinder"
    xfceuser=$(ps ua -C xfce4-session | grep /usr/bin/xfce4-session  | tail -n1 | awk '{print $1}')
    sudo -u $xfceuser DISPLAY=:0.0 xfce4-appfinder
  fi
else
  echo "no x, killing all pnd aps so x gets restarted"
  pidlist=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*(\([0-9]\+\))/\1/p')
  for PID in $pidlist
  do
    kill -9 $PID
  done
fi
