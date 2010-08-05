#!/bin/bash
 
#Usage: pnd_run.sh -p your.pnd -e executeable [-a "(arguments)"] [ -s "cd to folder inside pnd"] [-b UID (name of mountpoint/pandora/appdata)] [-x close x before launching(script needs to be started with nohup for this to work]
# -s startdir
# arguments can be inside -e, -a is optional
 
#/etc/sudoers needs to be adjusted if you touch any of the sudo lines
 
# look at the comments in the nox part, adjust 
#use "lsof /usr/lib/libX11.so.6 | awk '{print $1}'| sort | uniq > whitelist" with nothing running to generate the whitelist
 
#todo - no proper order
#validate params better
#make uid/pnd_name mandatory (and rename var, its confusing!)
#find a clean way of shutting down x without a fixed dm, mabye avoid nohup usage somehow
#add options to just mount iso without union and to mount the union later
#cleanup
#Rewrite! - this sucks

showHelp() {
	cat <<endHELP
Usage: pnd_run.sh -p your.pnd -e executeable [-a "(arguments)"] [ -s "cd to folder inside pnd"] [-b UID (name of mountpoint/pandora/appdata)] [-x close x before launching(script needs to be started with nohup for this to work]
Usage for mounting/umounting pnd_run.sh -p your.pnd -b uid -m or -u
endHELP
}

list_using_fs() {
	for p in $(fuser -m $1 2>/dev/null);do ps hf $p;done
}

runApp() {
	cd "/mnt/utmp/$PND_NAME"		# cd to union mount
	if [ "$STARTDIR" ] && [ -d "$STARTDIR" ]; then
		cd "$STARTDIR";			# cd to folder specified by the optional arg -s
	fi
	echo "[------------------------------]{ App start }[---------------------------------]"
	LD_LIBRARY_PATH="/mnt/utmp/$PND_NAME" "./$EXENAME" $ARGUMENTS 
						# execute app with ld_lib_path set to the union mount, a bit evil but i think its a good solution

	#the app could have exited now, OR it went into bg, we still need to wait in that case till it really quits!
	PID=$(pidof -o %PPID -x \"$EXENAME\")	# get pid of app
	while [ "$PID" ];do			# wait till we get no pid back for tha app, again a bit ugly, but it works
		sleep 10s
		PID=`pidof -o %PPID -x \"$EXENAME\"`
	done
	echo "[-------------------------------]{ App end }[----------------------------------]"
}

mountPnd() {
	#create mountpoints, check if they exist already first to avoid annoying error messages
	if ! [ -d "/mnt/pnd/${PND_NAME}" ]; then 
		sudo mkdir -p "/mnt/pnd/${PND_NAME}"		#mountpoint for iso, ro
	fi 
	#writeable dir for union
	if ! [ -d "${MOUNTPOINT}/pandora/appdata/${PND_NAME}" ]; then 
		sudo mkdir -p "${MOUNTPOINT}/pandora/appdata/${PND_NAME}"
		sudo chmod -R a+xrw "${MOUNTPOINT}/pandora/appdata/${PND_NAME}" 2>/dev/null
	fi
	if ! [ -d "/mnt/utmp/${PND_NAME}" ]; then
		sudo mkdir -p "/mnt/utmp/${PND_NAME}"		# union over the two
	fi
	rm  /tmp/cpuspeed
	if [ ${cpuspeed:-$(cat /proc/pandora/cpu_mhz_max)} -gt $(cat /proc/pandora/cpu_mhz_max) ]; then 
           cpuselection=$(zenity --title="set cpu speed" --height=240 --list --column "id" --column "Please select" --hide-column=1 --text="$PND_NAME suggests to set the cpu speed to $cpuspeed MHz to make it run properly.\n\n Do you want to change the cpu speed? (current speed: $(cat /proc/pandora/cpu_mhz_max) MHz)" "yes" "Yes, set it to that value" "custom" "Yes, select custom value" "no" "No, don't change the speed")
	  if [ ${cpuselection} = "yes" ]; then	
		cat /proc/pandora/cpu_mhz_max > /tmp/cpuspeed	
		sudo /usr/pandora/scripts/op_cpuspeed.sh $cpuspeed
  	  elif [ ${cpuselection} = "custom" ]; then	
		cat /proc/pandora/cpu_mhz_max > /tmp/cpuspeed		
		sudo /usr/pandora/scripts/op_cpuspeed.sh
  	fi
	#gksudo --message ", enter your password to allow" "echo $cpuspeed>/proc/pandora/cpu_mhz_max"
	fi

	#is the union already mounted? if not mount evrything, else launch the stuff
	mount | grep "on /mnt/utmp/${PND_NAME} type"
	if [ $? -ne 0 ];then
		echo not mounted on loop yet, doing so
		#check if pnd is already attached to loop 
		LOOP=$(sudo losetup -a | grep "$PND" | tail -n1 | awk -F: '{print $1}')
		#check if the loop device is already mounted
		if ! [ -z "$LOOP" ];then
			loopmountedon=$( mount | grep "$(mount | grep "$LOOP" | awk '{print $3}')" | grep utmp | awk '{print $3}' )
		else
			loopmountedon=""
		fi
		echo "LoopMountedon: $loopmountedon"
		if [ ! "$loopmountedon" ]; then #check if the pnd is already attached to some loop device but not used
			FREELOOP=$LOOP 
			#reuse existing loop
			if [ ! "$LOOP" ]; then
				FREELOOP=$(sudo /sbin/losetup -f) #get first free loop device
				echo $FREELOOP
				if [ ! "$FREELOOP" ]; then  # no free loop device, create a new one
					    #find a free loop device and use it 
					    usedminor=$(sudo /sbin/losetup -a | tail -n1)
					    usedminor=${usedminor:9:1}
					    echo usedminor $usedminor
					    freeminor=$(($usedminor+1))
					    echo freeminor $freeminor
					    sudo mknod -m777 /dev/loop$freeminor b 7 $freeminor
					    FREELOOP=/dev/loop$freeminor
				fi
			fi
			#detect fs

			case $PND_FSTYPE in
			ISO)
				sudo /sbin/losetup $FREELOOP "$PND" #attach the pnd to the loop device
				mntline="sudo mount ${FREELOOP}" #setup the mountline for later
				#mntline="sudo mount -o loop,mode=777 $PND /mnt/pnd/$PND_NAME"
				echo "Filetype is $PND_FSTYPE";;
			directory)
				#we bind the folder, now it can be treated in a unified way 
				#ATENTION: -o ro doesnt work for --bind at least on 25, on 26 its possible using remount, may have changed on 27
				mntline="sudo mount --bind -o ro \"${PND}\" "
				echo "Filetype is $PND_FSTYPE";;
			Squashfs)
				sudo /sbin/losetup $FREELOOP "$PND" #attach the pnd to the loop device
				mntline="sudo mount -t squashfs  ${FREELOOP}"
				echo "Filetype is $PND_FSTYPE";;
			*)
				echo "error determining fs, output was $PND_FSTYPE"
				exit 1;;
			esac

			echo "$mntline"
			$mntline "/mnt/pnd/${PND_NAME}" #mount the pnd/folder
			echo "mounting union!"
			FILESYSTEM=$(mount | grep "on $MOUNTPOINT " | grep -v rootfs | awk '{print $5}' | tail -n1) #get filesystem appdata is on to determine aufs options
			echo "Filesystem is $FILESYSTEM"
			if [[ "$FILESYSTEM" = "vfat" ]]; then # use noplink on fat, dont on other fs's 
				#append is fucking dirty, need to clean that up
				sudo mount -t aufs -o exec,noplink,dirs="$MOUNTPOINT/pandora/appdata/$PND_NAME=rw+nolwh":"/mnt/pnd/$PND_NAME=rr$append" none "/mnt/utmp/$PND_NAME"
				# put union on top
			else
				sudo mount -t aufs -o exec,dirs="$MOUNTPOINT/pandora/appdata/$PND_NAME=rw+nolwh":"/mnt/pnd/$PND_NAME=rr$append" none "/mnt/utmp/$PND_NAME" 
				# put union on top
			fi
		else #the pnd is already mounted but a mount was requested with a different basename/uid, just link it there
			      echo $LOOP already mounted on $loopmountedon skipping losetup - putting link to old mount
			      #this is bullshit
			      sudo rmdir "/mnt/utmp/$PND_NAME"
			      sudo ln -s $loopmountedon "/mnt/utmp/$PND_NAME" 
		fi
	
	else
		echo "Union already mounted"
	fi
}

unmountPnd() {
	sudo umount "/mnt/utmp/$PND_NAME" #umount union
	if [ -f /tmp/cpuspeed ]; then
		cpuspeed=$(cat /tmp/cpuspeed)
		sudo /usr/pandora/scripts/op_cpuspeed.sh $cpuspeed
		rm /tmp/cpuspeed
	fi
	if [ -z "$(mount |grep utmp/$PND_NAME|cut -f3 -d' ')" ]; then
		# check if the umount was successfull, if it wasnt it would mean that theres still something running so we skip this stuff, 
		# this WILL lead to clutter if it happens, so we should make damn sure it never happens
		# umount the actual pnd
		sudo umount "/mnt/pnd/$PND_NAME"
		if [ -z "$(mount |grep pnd/$PND_NAME|cut -f3 -d' ')" ]; then
			#delete folders created by aufs if empty
			sudo rmdir "$MOUNTPOINT/pandora/appdata/$PND_NAME/.wh..wh.plnk" 2>/dev/null
			sudo rmdir "$MOUNTPOINT/pandora/appdata/$PND_NAME/.wh..wh..tmp" 2>/dev/null
			#delete appdata folder and ancestors if empty
			sudo rmdir -p "$MOUNTPOINT/pandora/appdata/$PND_NAME/" 2>/dev/null
			#delete tmp mountpoint
			if [ -d "/mnt/utmp/$PND_NAME" ];then
				sudo rmdir "/mnt/utmp/$PND_NAME"
			else
				sudo rm "/mnt/utmp/$PND_NAME" >/dev/null 2>&1
			fi
			if [ $PND_FSTYPE = ISO ] || [ $PND_FSTYPE = Squashfs ]; then # check if we where running an iso, clean up loop device if we did
				LOOP=$(sudo losetup -a | grep "$(basename $PND)" | tail -n1 | awk -F: '{print $1}')
				sudo /sbin/losetup -d $LOOP
				sudo rm $LOOP
			fi
			if [ -d /mnt/pnd/$PND_NAME ];then
				sudo rmdir "/mnt/pnd/$PND_NAME" #delete pnd mountpoint
			fi

			echo cleanup done
		else
			echo umount failed, didnt clean up. Process still using this FS :
			list_using_fs "/mnt/pnd/$PND_NAME"
		fi
	else
		echo umount failed, didnt clean up. Process still using this FS :
		list_using_fs "/mnt/utmp/$PND_NAME"
	fi
}

main() {
	if [ $nox ]; then #the app doesnt want x to run, so we kill it and restart it once the app quits
		if [ ! $(pidof X) ]; then 
			unset $nox
		else
			applist=$(lsof /usr/lib/libX11.so.6 | awk '{print $1}'| sort | uniq)
			whitelist=$(cat ~/pndtest/whitelist) #adjust this to a fixed whitelist, maybe in the config dir
			filteredlist=$(echo -e "$applist\n\n$whitelist\n\n$whitelist" | sort | uniq -u) #whitelist appended two times so those items are always removed
			if [ ${#filteredlist} -ge 1 ]; then
				message=$(echo -e "The following applications are still running, are you sure you want to close x? \n$filteredlist")
				echo -e "?ae[34me[30m?"
				xmessage -center "$message", -buttons yes,no
				if [ $? = 102 ]; then
					exit 1
				fi
				sudo /etc/init.d/slim-init stop
				sleep 5s
			else
				echo -e "?ae[34me[30m?"
				xmessage -center "killing x, nothing of value will be lost", -buttons ok,cancel
				if [ $? = 102 ]; then
					exit 1
				fi
				# close x now, do we want to use slim stop or just kill x?
				sudo /etc/init.d/slim-init stop
				sleep 5s
			fi
		fi
	fi

	case $ACTION in
	mount)	mountPnd;;
	umount)	unmountPnd;;
	run)
		mountPnd
		oPWD=$(pwd)
		runApp
		cd $oPWD
		unmountPnd;;
	esac


	if [ $nox ]; then #restart x if it was killed
		echo "starting x in 5s"
		sleep 5
		sudo /etc/init.d/slim-init start
	fi
}

######################################################################################
####	Parse arguments
##

TEMP=`getopt -o p:e:a:b:s:m::u::n::x::j:c: -- "$@"`
 
if [ $? != 0 ] ; then echo "Terminating..." >&2 ; exit 1 ; fi
 
# Note the quotes around `$TEMP': they are essential!
eval set -- "$TEMP"
 
ACTION=run
while true ; do
	case "$1" in
		-p) PND=$2;shift 2;;
		-e) EXENAME=$2;shift 2 ;;
		-b) PND_NAME=$2;shift 2;;
		-s) STARTDIR=$2;shift 2;;
		-m) ACTION=mount;shift 2;;
		-u) ACTION=umount;shift 2;;
		-x) nox=1;shift 2;;
		-j) append=$2;shift 2;;
		-c) cpuspeed=$2;shift 2;;
		-a) 
			case "$2" in
				"") echo "no arguments"; shift 2 ;;
				*)  echo "args set to \`$2'" ;ARGUMENTS=$2;shift 2 ;;
			esac ;;
		--) shift ; break ;;
		*) echo "Error while parsing arguments!" ; exit 1 ;;
	esac
done
if [ ! -e "$PND" ]; then #check if theres a pnd suplied, need to clean that up a bit more
	echo "ERROR: selected PND file does not exist!"
	showHelp
	exit 1
fi
if [ ! "$EXENAME" ] && [[ "$ACTION" = "run" ]]; then
	echo "ERROR: no executable name provided!"
	showHelp
	exit 1
fi


PND_FSTYPE=$(file -b "$PND" | awk '{ print $1 }')	# is -p a zip/iso or folder?
MOUNTPOINT=$(df "$PND" | tail -1|awk '{print $6}')	# find out on which mountpoint the pnd is
if [ ! -d "$MOUNTPOINT" ] || [ $MOUNTPOINT = "/" ]; then 
	MOUNTPOINT="";
fi
 
#PND_NAME really should be something sensible and somewhat unique
#if -b is set use that as pnd_name, else generate it from PND
#get basename (strip extension if file) for union mountpoints etc, maybe  this should be changed to something specified inside the xml
#this should probably be changed to .... something more sensible
#currently only everything up to the first '.' inside the filenames is used.
PND_NAME=${PND_NAME:-"$(basename $PND | cut -d'.' -f1)"}

if [ $nox ]; then
	main > "/tmp/pndrun${PND_NAME}_$ACTION.out" 2>&1 & 
	disown
else
	main > "/tmp/pndrun${PND_NAME}_$ACTION.out" 2>&1
fi

	