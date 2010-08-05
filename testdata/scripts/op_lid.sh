#!/bin/bash
#actions done when the lid is closed
#only argument is 0 for open 1 for closed

if [ ! -e /tmp/powerstate ]; then #do nothing when in powersave mode
  if [ "$1" = "1" ]; then #lid was closed
    #/usr/pandora/scripts/op_bright.sh 0
    cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/brightness > /tmp/oldbright
    echo 0 > /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/brightness
    echo 1 > /sys/devices/platform/omapfb/graphics/fb0/blank
  elif [ "$1" = "0" ]; then # lid was opend
    echo 0 > /sys/devices/platform/omapfb/graphics/fb0/blank
    sleep 0.1s # looks cleaner, could flicker without
    maxbright=$(cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/max_brightness)
    oldbright=$(cat /tmp/oldbright)
     if [ $oldbright -ge 3 ] && [ $oldbright -le $maxbright ]; then 
      /usr/pandora/scripts/op_bright.sh $oldbright 
     else
      /usr/pandora/scripts/op_bright.sh $maxbright
     fi
  fi
fi
