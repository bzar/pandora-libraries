#!/bin/bash
#actions done when the lid is closed
#only argument is 0 for open 1 for closed

if [ "$1" = "1" ]; then #lid was closed
  /usr/pandora/scripts/op_bright.sh 0
elif [ "$1" = "0" ]; then # lid was opend
  /usr/pandora/scripts/op_bright.sh $(cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/max_brightness)
fi

