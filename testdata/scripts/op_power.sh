#!/bin/bash
#actions done when the power button is pressed
#only argument is the time the button was pressed in  seconds

if [ "$1" -le "3" ]; then # button was pressed 1-3sec, "suspend"
  if [ -e /tmp/powerstate ]; then 
    powerstate=$(cat /tmp/powerstate)
  else
    powerstate=0
  fi
  if [ $powerstate -eq "1" ]; then
    #in lowpower mode
    echo 0 > /tmp/powerstate
    oldbright=$(cat /tmp/oldbright)
    maxbright=$(cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/max_brightness)
    oldspeed=$(cat /tmp/oldspeed)
    if [ $oldbright -ge 3 ] && [ $oldbright -le $maxbright ]; then 
      /usr/pandora/scripts/op_bright.sh $oldbright 
    else
      /usr/pandora/scripts/op_bright.sh $maxbright
    fi
    if [ $oldspeed -ge 14 ] && [ $oldspeed -le 1000 ]; then 
     echo $oldspeed > /proc/pandora/cpu_mhz_max 
    else
      echo 500 > /proc/pandora/cpu_mhz_max
    fi
    hciconfig hci0 up
    /etc/init.d/wl1251-init start
    pidlist=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*(\([0-9]\+\))/\1/p')
    for PID in $pidlist
    do
      kill -18 $PID #send SIGCONT
    done
  else
    #in normal mode
    echo 1 > /tmp/powerstate
    cat /proc/pandora/cpu_mhz_max > /tmp/oldspeed
    cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/brightness > /tmp/oldbright
    pidlist=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*(\([0-9]\+\))/\1/p')
    for PID in $pidlist
    do
      kill -19 $PID #send SIGSTOP
    done
    hciconfig hci0 down
    /etc/init.d/wl1251-init stop
    echo 0 > /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/brightness
    echo 14 > /proc/pandora/cpu_mhz_max
  fi
elif [ "$1" -ge "4" ]; then #button was pressed 4 sec or longer, shutdown
  shutdown -h now
fi

