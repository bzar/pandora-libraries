#!/bin/bash
#usage op_shutdown.sh time in seconds
xfceuser=$(ps u -C xfce4-session | tail -n1 | awk '{print $1}')
time=$1
countdown () {
  for i in $(seq $time); do
    precentage=$(echo $i $time | awk '{ printf("%f\n", $1/$2*100) }')
    echo $precentage
    echo "# Low power, shutdown in $i"
    sleep 1
  done
}
countdown  | su -c 'DISPLAY=:0.0  zenity --progress --auto-close --text "Shutdown" --title "Shutdown"' $xfceuser
if [ $? -eq 0 ]; then
shutdown -h now
else
su -c 'DISPLAY=:0.0  zenity --error --text "`printf "Shutdown aborted! \n
Pleas plug in the charger ASAP or shutdown manually, the System will crash in a few minuts"`"' $xfceuser
fi