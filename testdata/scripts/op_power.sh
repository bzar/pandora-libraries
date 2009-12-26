#!/bin/bash
#actions done when the power button is pressed
#only argument is the time the button was pressed in  seconds

if [ "$1" -le "1" ]; then #button was pressed 1 sec or less, resume
  pidlist=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*(\([0-9]\+\))/\1/p')
  for PID in $pidlist
  do
    kill -18 $PID #send SIGCONT
  done
  /usr/pandora/scripts/op_cpuspeed.sh 500
  /usr/pandora/scripts/op_bright.sh $(cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/max_brightness)
elif [ "$1" -le "3" ]; then # button was pressed 1-3sec, "suspend"
  pidlist=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*(\([0-9]\+\))/\1/p')
  for PID in $pidlist
  do
    kill -19 $PID #send SIGSTOP
  done
  /usr/pandora/scripts/op_bright.sh 0
  /usr/pandora/scripts/op_cpuspeed.sh 14
elif [ "$1" -ge "3" ]; then #button was pressed 3 sec or longer, shutdown
  shutdown -h now
fi

