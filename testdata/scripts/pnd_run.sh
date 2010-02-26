#!/bin/bash
 
#Usage: pnd_run.sh -p your.pnd -e executeable [-a "(arguments)"] [ -s "cd to folder inside pnd"] [-b UID (name of mountpoint/pandora/appdata)] [-x close x before launching(script needs to be started with nohup for this to work]
# -s startdir
# arguments can be inside -e, -a is optional
 
#/etc/sudoers needs to be adjusted if you touch any of the sudo lines
 
# look at the comments in the nox part, adjust 
#use "lsof /usr/lib/libX11.so.6 | awk '{print $1}'| sort | uniq > whitelist" with nothing running to generate the whitelist
 
#launch the script with nohup for -x to work!
 
#todo - no proper order
#validate params better
#make uid/basename mandatory (and rename var, its confusing!)
#find a clean way of shutting down x without a fixed dm, mabye avoid nohup usage somehow
#add options to just mount iso without union and to mount the union later
#cleanup
#Rewrite! - this sucks
 
# parse arguments
TEMP=`getopt -o p:e:a:b:s:m::u::n::x::j:c: -- "$@"`
 
if [ $? != 0 ] ; then echo "Terminating..." >&2 ; exit 1 ; fi
 
# Note the quotes around `$TEMP': they are essential!
eval set -- "$TEMP"
 
while true ; do
	case "$1" in
		-p) echo "pnd set to \`$2'" ;PND=$2;shift 2;;
		-e) echo "exec set to \`$2'" ;EXENAME=$2;shift 2 ;;
#		-n) echo "n set, no union pls!";NOUNION=1;shift 2;; # we will reuse -n soon,stop using it if you still did!
		-b) echo "BASENAME set to $2";BASENAME=$2;shift 2;;
		-s) echo "startdir set to $2";STARTDIR=$2;shift 2;;
		-m) echo "mount";mount=1;shift 2;;
		-u) echo "umount";umount=1;shift 2;;
		-x) echo "no x";nox=1;shift 2;;
		-j) echo "join/ also mount those folders";append=$2;shift 2;;
		-j) echo "set cpu speed to";cpuspeed=$2;shift 2;;
		-a) 
			case "$2" in
				"") echo "no arguments"; shift 2 ;;
				*)  echo "args set to \`$2'" ;ARGUMENTS=$2;shift 2 ;;
			esac ;;
		--) shift ; break ;;
		*) echo "Error while parsing arguments!" ; exit 1 ;;
	esac
done
 
if [ ! $PND ]; then #check if theres a pnd suplied, need to clean that up a bit more
	echo "Usage: pnd_run.sh -p your.pnd -e executeable [-a \"(arguments)\"] [ -s \"cd to folder inside pnd\"] [-b UID (name of mountpoint/pandora/appdata)] [-x close x before launching(script needs to be started with nohup for this to work]"
	exit 1
fi
if [ ! $EXENAME ]; then
	if [ ! $mount ] && [ ! $umount ]; then
		echo "Usage: pnd_run.sh -p your.pnd -e executeable [-a \"(arguments)\"] [ -s \"cd to folder inside pnd\"] [-b UID (name of mountpoint/pandora/appdata)] [-x close x before launching(script needs to be started with nohup for this to work]"
		echo "Usage for mounting/umounting pnd_run.sh -p your.pnd -b uid -m or -u"
		exit 1
	fi
fi

fork () {
echo in fork!
if [ $nox ]; then #the app doesnt want x to run, so we kill it and restart it once the app quits
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
		sudo /etc/init.d/slim-init stop
		sleep 5s
	else
		echo -e “ae[34me[30m”
		xmessage -center "killing x, nothing of value will be lost", -buttons ok,cancel
		if [ $? = 102 ]; then
		exit 1
		fi
		# close x now, do we want to use slim stop or just kill x?
		sudo /etc/init.d/slim-init stop
		sleep 5s
	fi
fi
 
#vars
DFS=$(file -b $PND | awk '{ print $1 }') #is -p a zip/iso or folder?
MOUNTPOINT=$(df $PND | sed -ne 's/.*\% \(\S*\)/\1/p' | tail -n1) #find out on which mountpoint the pnd is
if [ ! -d "$MOUNTPOINT" ]; then MOUNTPOINT="/"; fi #make sure folder exists, if it doesnt assume rootfs
FILESYSTEM=$(mount | grep "on $MOUNTPOINT " | grep -v rootfs | awk '{print $5}' | tail -n1) #get filesystem appdata is on to determine aufs options
echo "Filesystem is $FILESYSTEM"
#if the pnd is on / set mountpoint to "" so we dont and up with // at the start,
#this is to make sure sudo doesnt get confused
if [ $MOUNTPOINT = "/" ]; then MOUNTPOINT=""; fi
echo "mountpoint: $MOUNTPOINT"
 
#BASENAME really should be something sensible and somewhat unique
#if -b is set use that as basename, else generate it from PND
#get basename (strip extension if file) for union mountpoints etc, maybe  this should be changed to something specified inside the xml
#this should probably be changed to .... something more sensible
if [ ! $BASENAME ]; then BASENAME=$(basename "$PND" | cut -d'.' -f1) ; fi
 
 
 
oCWD=$(pwd)


 
#create mountpoints, check if they exist already first to avoid annoying error messages
if [ ! -d /mnt/pnd/$BASENAME ]; then sudo mkdir -p /mnt/pnd/$BASENAME ; fi #mountpoint for iso, ro
#writeable dir for union
if [ ! -d $MOUNTPOINT/pandora/appdata/$BASENAME ]; then sudo mkdir -p $MOUNTPOINT/pandora/appdata/$BASENAME; sudo chmod -R a+xrw $MOUNTPOINT/pandora/appdata/$BASENAME; fi
if [ ! -d /mnt/utmp/$BASENAME ]; then sudo mkdir -p /mnt/utmp/$BASENAME; fi #union over the two

#mount
 
if [ ! $umount ]; then
	if [ ! $cpuspeed -eq $(cat /sys/devices/platform/twl4030-pwm0-bl/backlight/twl4030-pwm0-bl/max_brightness) ]; then 
	  gksu --message "$BASENAME wants to set the cpu speed to $cpuspeed, enter root password to allow" echo $cpuspeed > /proc/pandora/cpu_mhz_max
	fi
	#is the union already mounted? if not mount evrything, else launch the stuff
	mount | grep "on /mnt/utmp/$BASENAME type" # > /dev/null
	if [ ! $? -eq 0 ]; then 

	  FREELOOP=$(sudo /sbin/losetup -f) #get first free loop device
	  if [ ! $FREELOOP  ]; then  # no free loop device, create a new one
		  #find a free loop device and use it 
		  usedminor=$(sudo /sbin/losetup -a | tail -n1)
		  usedminor=${usedminor:9:1}
		  echo usedminor $usedminor
		  freeminor=$(($usedminor+1))
		  echo freeminor $freeminor
		  sudo mknod -m777 /dev/loop$freeminor b 7 $freeminor
		  FREELOOP=/dev/loop$freeminor
	  fi
	  
	  #detect fs
	  if [ $DFS = ISO ]; then
		  sudo /sbin/losetup $FREELOOP $PND #attach the pnd to the loop device
		  mntline="sudo mount $FREELOOP /mnt/pnd/$BASENAME/" #setup the mountline for later
	  #	mntline="sudo mount -o loop,mode=777 $PND /mnt/pnd/$BASENAME"
		  echo "Filetype is $DFS"
	  elif [ $DFS = directory ]; then
		  mntline="sudo mount --bind -o ro $PND /mnt/pnd/$BASENAME"
	  #we bind the folder, now it can be treated in a unified way ATENTION: -o ro doesnt work for --bind at least on 25, on 26 its possible using remount, may have changed on 27
		  echo "Filetype is $DFS"
	  elif [ $DFS = Squashfs ]; then
		  sudo /sbin/losetup $FREELOOP $PND #attach the pnd to the loop device
		  mntline="sudo mount -t squashfs  $FREELOOP /mnt/pnd/$BASENAME"
		  echo "Filetype is $DFS"
	  else
		  echo "error determining fs, output was $DFS"
		  exit 1;
	  fi


		echo "$mntline"
		$mntline #mount the pnd/folder
		echo "mounting union!"
		if [ $FILESYSTEM = vfat ]; then # use noplink on fat, dont on other fs's 
		#append is fucking dirty, need to clean that up
		sudo mount -t aufs -o exec,noplink,dirs=$MOUNTPOINT/pandora/appdata/$BASENAME=rw+nolwh:/mnt/pnd/$BASENAME=rr$append none /mnt/utmp/$BASENAME # put union on top
		else
		sudo mount -t aufs -o exec,dirs=$MOUNTPOINT/pandora/appdata/$BASENAME=rw+nolwh:/mnt/pnd/$BASENAME=rr$append none /mnt/utmp/$BASENAME # put union on top
		fi
 
	else
		echo "Union already mounted"
	fi
 
	if [ $mount ]; then echo "mounted /mnt/utmp/$BASENAME"; exit 1; fi; #mount only, die here
	
	cd /mnt/utmp/$BASENAME # cd to union mount
	if [ $STARTDIR ]; then cd $STARTDIR; fi #cd to folder specified by the optional arg -s
	LD_LIBRARY_PATH=/mnt/utmp/$BASENAME ./$EXENAME $ARGUMENTS # execute app with ld_lib_path set to the union mount, a bit evil but i think its a good solution
	#the app could have exited now, OR it went into bg, we still need to wait in that case till it really quits!
	PID=`pidof -o %PPID -x $EXENAME` #get pid of app
	while [ "$PID" ] #wait till we get no pid back for tha app, again a bit ugly, but it works
	do
	sleep 10s
	PID=`pidof -o %PPID -x $EXENAME`
	done
	echo app exited
 
	#app exited
	cd $oCWD #cd out of the mountpoint so we can umount, doesnt really matter to where...
else
echo "-u set, nothing to do here"
fi
 
 
#clean up
sudo umount /mnt/utmp/$BASENAME #umount union
if [ $? -eq 0 ]; then # check if the umount was successfull, if it wasnt it would mean that theres still something running so we skip this stuff, this WILL lead to clutter if it happens, so we should make damn sure it never happens
	#umount the actual pnd
	sudo umount /mnt/pnd/$BASENAME
	#delete folders created by aufs if empty
	sudo rmdir $MOUNTPOINT/pandora/appdata/$BASENAME/.wh..wh.plnk
	sudo rmdir $MOUNTPOINT/pandora/appdata/$BASENAME/.wh..wh..tmp
	#delete appdata folder and ancestors if empty
	sudo rmdir -p $MOUNTPOINT/pandora/appdata/$BASENAME/
	#delete tmp mountpoint
	sudo rmdir /mnt/utmp/$BASENAME;
	if [ $DFS = ISO ] || [ $DFS = Squashfs ]; then # check if we where running an iso, clean up loop device if we did
		sudo /sbin/losetup -d $FREELOOP
		sudo rm $FREELOOP
	fi
	sudo rmdir /mnt/pnd/$BASENAME #delete pnd mountpoint
else
echo umount failed, didnt clean up
fi

if [ $nox ]; then #restart x if it was killed
echo "starting x in 5s"
sleep 5
sudo /etc/init.d/slim-init start
fi
}
echo forking now!
fork &> /tmp/pndrun.out & 
disown