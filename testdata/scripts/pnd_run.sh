#!/bin/bash
 
#Usage: pnd_run.sh -p your.pnd -e executeable [-a "(arguments)"] [ -s "cd to folder inside pnd"] [-u (skip union)] [-b override BASENAME (name of mountpoint/pandora/appdata)] [-x close x before launching(script needs to be started with nohup for this to work]
# -n to skip union mount, should probably be removed before release
# -s startdir
# arguments can be inside -e, -a is optional
 
#/etc/sudoers needs to be adjusted if you touch any of the sudo lines in the wrong place.
 
# look at the comments in the nox part, adjust 
 
#use "lsof /usr/lib/libX11.so.6 | awk '{print $1}'| sort | uniq > whitelist" with nothing running to generate the whitelist
 
#launch the script with nohup for -x to work!
 
#todo
#make sure to only use free loop devices!
#cleanup
 
# parse arguments
TEMP=`getopt -o p:e:a:b:s:m::u::n::x:: -- "$@"`
 
if [ $? != 0 ] ; then echo "Terminating..." >&2 ; exit 1 ; fi
 
# Note the quotes around `$TEMP': they are essential!
eval set -- "$TEMP"
 
while true ; do
	case "$1" in
		-p) echo "pnd set to \`$2'" ;PND=$2;shift 2;;
		-e) echo "exec set to \`$2'" ;EXENAME=$2;shift 2 ;;
		-n) echo "n set, no union pls!";NOUNION=1;shift 2;;
		-b) echo "BASENAME set to $2";BASENAME=$2;shift 2;;
		-s) echo "startdir set to $2";STARTDIR=$2;shift 2;;
		-m) echo "mount";mount=1;shift 2;;
		-u) echo "umount";umount=1;shift 2;;
		-x) echo "no x";nox=1;shift 2;;
		-a) 
			case "$2" in
				"") echo "no arguments"; shift 2 ;;
				*)  echo "args set to \`$2'" ;ARGUMENTS=$2;shift 2 ;;
			esac ;;
		--) shift ; break ;;
		*) echo "Error while parsing arguments!" ; exit 1 ;;
	esac
done
 
if [ ! $PND ]; then
	echo "Usage: pnd_run.sh -p your.pnd -e executeable [-a \"(arguments)\"] [ -s \"cd to folder inside pnd\"] [-u (skip union)] [-b override BASENAME (name of mountpoint/pandora/appdata)] [-x close x before launching(script needs to be started with nohup for this to work]"
	exit 1
fi
if [ $nox ]; then
	applist=$(lsof /usr/lib/libX11.so.6 | awk '{print $1}'| sort | uniq)
	whitelist=$(cat ~/pndtest/whitelist) #adjust this to a fixed whitelist, maybe in the config dir
	filteredlist=$(echo -e "$applist\n\n$whitelist\n\n$whitelist" | sort | uniq -u) #whitelist appended two times so those items are always removed
	if [ ${#filteredlist} -ge 1 ]; then
		message=$(echo -e "The following applications are still running, are you sure you want to close x? \n$filteredlist")
		echo -e “ae[34me[30m”
		xmessage -center "$message", -buttons yes,no
		if [ $? = 102 ]; then
		exit 1
		fi
		sudo /etc/init.d/gdm stop
		sleep 5s
	else
		echo -e “ae[34me[30m”
		xmessage -center "killing x, nothing of value will be lost", -buttons ok,cancel
		if [ $? = 102 ]; then
		exit 1
		fi
		# close x now, do we want to use gdm stop or just kill x?
		sudo /etc/init.d/gdm stop
		sleep 5s
	fi
fi
 
#vars
DFS=$(file -b $PND | awk '{ print $1 }') #is -p a zip/iso or folder?
MOUNTPOINT=$(df $PND | sed -ne 's/.*\% \(\S*\)/\1/p' | tail -n1)
if [ $MOUNTPOINT = "/" ]; then MOUNTPOINT=""; fi
echo "mountpoint: $MOUNTPOINT"
#MOUNTPOINT=$(df -h $PND | grep -E '[1-9]%' | awk '{ print $6  }') #find out which mountpoint the pnd/folder is on, there probably is a better way to do this
 
 
#BASENAME really should be something sensible and somewhat unique
#if -b is set use that as basename, else generate it from PND
#get basename (strip extension if file) for union mountpoints etc, maybe  this should be changed to something specified inside the xml
#this should probably be changed to .... something more sensible
if [ ! $BASENAME ]; then BASENAME=$(basename "$PND" | cut -d'.' -f1) ; fi
 
 
 
oCWD=$(pwd)
 
#detect fs
if [ $DFS = ISO ]; then
 
	usedminor=$( ls -l /dev/loop* | awk '{print $6}')
	freeminor=$( echo -e "$(seq 0 64)\n$usedminor" | sort -rn | uniq -u | tail -n1)
	sudo mknod -m777 /dev/loop$freeminor b 7 $freeminor
	sudo losetup /dev/loop$freeminor $PND
 
	mntline="sudo mount /dev/loop$freeminor /mnt/pnd/$BASENAME/"
#	mntline="sudo mount -o loop,mode=777 $PND /mnt/pnd/$BASENAME"
	echo "Filetype is $DFS"
elif [ $DFS = Zip ]; then
	mntline="fuse-zip $PND /mnt/pnd/$BASENAME -o ro,fmask=000" #TOTALLY untested right now
	echo "Filetype is $DFS"
elif [ $DFS = directory ]; then
	mntline="sudo mount --bind -o ro $PND /mnt/pnd/$BASENAME"
#we bind the folder, now it can be treated in a unified way ATENTION: -o ro doesnt work for --bind at least on 25, on 26 its possible using remount, may have changed on 27
	echo "Filetype is $DFS"
else
	echo "error"
	exit 1;
fi
 
#create mountpoints, check if they exist already first to avoid annoying error messages
if [ ! -d /mnt/pnd/$BASENAME ]; then sudo mkdir -p /mnt/pnd/$BASENAME ; fi
if [ ! -d $MOUNTPOINT/pandora/appdata/$BASENAME ]; then sudo mkdir -p $MOUNTPOINT/pandora/appdata/$BASENAME; sudo chmod -R a+xrw $MOUNTPOINT/pandora/appdata/$BASENAME; fi
if [ ! -d /mnt/utmp/$BASENAME ]; then sudo mkdir -p /mnt/utmp/$BASENAME; fi 
 
#mount
 
if [ ! $NOUNION ] && [ ! $umount ]; then
	#is the union already mounted? if not mount evrything, else launch the stuff
	mount | grep "on /mnt/utmp/$BASENAME type" # > /dev/null
	if [ ! $? -eq 0 ]; then 
		echo "$mntline"
		$mntline #mount the pnd/folder
		echo "mounting union!"
		sudo mount -t aufs -o exec,dirs\=$MOUNTPOINT/pandora/appdata/$BASENAME=rw+nolwh:/mnt/pnd/$BASENAME=rr none /mnt/utmp/$BASENAME # put union on top
 
	else
		echo "Union already mounted"
	fi
 
	if [ $mount ]; then echo "mounted /mnt/utmp/$BASENAME"; exit 1; fi;
 
	#start app
	cd /mnt/utmp/$BASENAME
	if [ $STARTDIR ]; then cd $STARTDIR; fi #cd to folder specified by the optional arg -s
	#echo "/lib/ld-linux.so.2 --library-path /mnt/utmp/$BASENAME/ $EXENAME $ARGUMENTS"
	#/lib/ld-linux.so.2 --library-path /mnt/utmp/$BASENAME/ $EXENAME $ARGUMENTS
	LD_LIBRARY_PATH=/mnt/utmp/$BASENAME ./$EXENAME $ARGUMENTS
	#the app could have exited now, OR it went into bg, we still need to wait in that case till it really quits!
	PID=`pidof -o %PPID -x $EXENAME`
	while [ "$PID" ]
	do
	sleep 10s
	PID=`pidof -o %PPID -x $EXENAME`
	done
	echo end
 
	#app exited
	cd $oCWD #cd out of the mountpoint so we can umount, doesnt really matter to where...
 
elif [ ! $umount ]; then
	$mntline
	if [ $mount ]; then echo "mounted /mnt/pnd/$BASENAME"; exit 1; fi;
	cd /mnt/pnd/$BASENAME
	if [ $STARTDIR ]; then cd $STARTDIR; fi
	echo $(pwd)
	#/lib/ld-linux.so.2 --library-path /mnt/pnd/$BASENAME/ $EXENAME $ARGUMENTS	
	./$EXENAME $ARGUMENTS 
	LD_LIBRARY_PATH=/mnt/pnd/$BASENAME ./$EXENAME $ARGUMENTS
	#the app could have exited now, OR it went into bg, we still need to wait in that case till it really quits!
	PID=`pidof -o %PPID -x $EXENAME`
	while [ $PID ]
	do
	sleep 10s
	PID=`pidof -o %PPID -x $EXENAME`
	done
	echo end
 
	cd $oCWD
else
echo "-u set, nothing to do here"
fi
 
 
#clean up
if [ ! $NOUNION ] ; then sudo umount /mnt/utmp/$BASENAME; fi #umount union if -u wasnt set
if [ $NOUNION ] ; then sudo umount /mnt/pnd/$BASENAME; fi #umount iso if -u WAS set
if [ $? -eq 0 ]; then # check if the umount was successfull, if it wasnt it would mean that theres still something running so we skip this stuff, this WILL lead to clutter if it happens, so we should make damn sure it never happens
	if [ ! $NOUNION ] ; then
		sudo umount /mnt/pnd/$BASENAME
		sudo rmdir $MOUNTPOINT/pandora/appdata/$BASENAME/.wh..wh.plnk
		sudo rmdir $MOUNTPOINT/pandora/appdata/$BASENAME/.wh..wh..tmp 
		sudo rmdir -p $MOUNTPOINT/pandora/appdata/$BASENAME/
		sudo rmdir /mnt/utmp/$BASENAME;
	fi
	if [ $DFS = ISO ]; then
		sudo losetup -d /dev/loop$freeminor
		sudo rm /dev/loop$freeminor
	fi
	sudo rmdir /mnt/pnd/$BASENAME 
fi
if [ $nox ]; then
echo "starting x in 5s"
sleep 5
sudo /etc/init.d/gdm start
fi
