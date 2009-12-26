#!/bin/bash
#actions done when the menu button is pressed
#only argument is the time the button was pressed in  seconds

if [ "$1" -le "1" ]; then #button was pressed 1 sec or less, resume
  pndrunning=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*(\([0-9]\+\))/\1/p')
  for PID in $pidlist
  do
    kill -18 $PID #send SIGCONT
  done
  /usr/pandora/scripts/op_cpuspeed 500
elif [ "$1" -le "3" ]; then # button was pressed 1-3sec, "suspend"
  pndrunning=$(pstree -lpA | grep pnd_run.sh | sed -ne 's/.*(\([0-9]\+\))/\1/p')
  for PID in $pidlist
  do
    kill -19 $PID #send SIGSTOP
  done
  /usr/pandora/scripts/op_cpuspeed 13
elif [ "$1" -ge "3" ]; then #button was pressed 3 sec or longer, shutdown
  shutdown -h now
fi

