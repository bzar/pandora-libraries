#!/bin/bash
 
#/etc/sudoers needs to be adjusted if you touch any of the sudo lines
 
# look at the comments in the CLOSE_X part, adjust 
#use "lsof /usr/lib/libX11.so.6 | awk '{print $1}'| sort | uniq > whitelist" with nothing running to generate the whitelist
 
#todo - no proper order
#validate params better
#make uid/pnd_name mandatory (and rename var, its confusing!)
#find a clean way of shutting down x without a fixed dm, mabye avoid nohup usage somehow
#add options to just mount iso without union and to mount the union later

#SCRIPT_DIR=$(echo $(dirname $(which $0))|sed 's#^\.#'"$(pwd)"'#')
#. $SCRIPT_DIR/pnd_loging
#PND_LogDateFormat=PND_Time

PND_MOUNT_DIR="/mnt/pnd"
UNION_MOUNT_DIR="/mnt/utmp"
CPUSPEEDSCRIPT=/usr/pandora/scripts/op_cpuspeed.sh

#=============================================================================
# Log functions

PND_isInteractive=0
PND_Start() {
	if [ $(ps -o tty,pid 2>/dev/null|grep $$|awk 'BEGIN{R=0}/pts/{R=1}END{print R}') -eq 1 ];then
		PND_isInteractive=1
		exec 3>&1	# duplicate stdout so we can still write to screen after the log redirect
	fi
	{
	echo "======================================================================================="
	for v in $PND_HEADER;do
		printf "%-15s : %s\n" "$v"  "$(eval "echo \${$v:-'<unset>'}")"
	done
	echo "======================================================================================="
	}>"$PND_LOG"
}

PND_checkLog() {
awk 'BEGIN{R=0}END{exit R}
/cannot open display/||/unary operator expected/||/No such file or directory/||/command not found/{R=R+1}
{IGNORECASE=1}
!/gpg-error/&&/ERROR/||/FAILED/{R=R+1}
{IGNORECASE=0}' < "$PND_LOG"
}

PND_Stop() {
	local RC=$?
	PND_checkLog
	RC=$(( $RC + $? ))	# trying to find error in the logs
	{
	echo "======================================================================================="
	echo "Return code is : $RC"
	}>>"$PND_LOG"
	return $RC
}

PND_BeginTask() {
	export PND_TASK_NAME="$*"
	echo "[ START ]--- $PND_TASK_NAME ----------"
	if [ $PND_isInteractive -eq 1 ];then
		printf "$PND_TASK_NAME"  >&3
	fi
}

PND_EndTask() {
	local RC="$?"
	local STATUS=""
	local COLOR=""
	local X=""
	if [ $RC -eq 0 ];then 
		STATUS=SUCCESS
		COLOR="\\033[32m"
	else
		STATUS=FAILED
		COLOR="\\033[31m"
	fi
	
	printf "[%7s]--- $PND_TASK_NAME ----------\n" "$STATUS"
	if [ $PND_isInteractive -eq 1 ];then
		printf "\r%s\033[70G[$COLOR%7s\033[m]\n" "$PND_TASK_NAME" "$STATUS" >&3
	fi
	unset PND_TASK_NAME
	return $RC
}

PND_WaitFor() {
	[ $# -gt 0 ]||return 1
	local l_test="$1"
	local l_name=${2:-"Wait succes of $1"}
	local l_cnt=${3:-12}
	local l_sleep=${4:-10}
	local C=0
	PND_BeginTask $l_name
	while [ $C -lt $l_cnt ] && ! eval $l_test;do
		sleep $l_sleep;C=$(($C + 1));
	done
	[ $C -lt $l_cnt ]
	PND_EndTask
}

PND_Exec() {
	local CMD="$*"
	{
	if [ $PND_ISEXEC -eq 0 ];then
		PND_ISEXEC=1
		exec 3>&1		# 
	fi
	export PND_INTERACTIVE=2
	$* 2>&1
	RES=$(( $? + $PND_ERRORS ))
	echo "<result>$RES</result>"
	PND_ISEXEC=0
	exec 3>&-
	}|{
	while read line;do
		if echo "$line"|grep -q "<result>.*</result>";then
			return $(( $(echo $line|sed 's/<[^>]*>//g') + $PND_ERRORS ))
		elif ! echo "$line"| $(eval $PND_OUT_CHECK); then
			PND_Error "$line";
		else
			PND_Print "$line";
		fi
	done
	return $PND_ERRORS
	}
}



#=============================================================================
# Utility functions

showHelp() {
	cat <<endHELP
Usage:
  pnd_run.sh -p file.pnd -e cmd [-a args] [-b pndid] [-s path] [-c speed] [-d [path]] [-x] [-m] [-u]
    -p file.pnd	: Specify the pnd file to execute
    -e cmd	: Command to run
    -a args	: Arguments to the command
    -b pndid	: name of the directory mount-point ($UNION_MOUNT_DIR/pndid) (Default: name of the pnd file)
    -s path	: Directory in the union to start the command from
    -o speed	: Set the CPU speed
    -d [path]	: Use path as source of the overlay. (Default: pandora/appdata/pndid)
    -x		: Stop X before starting the apps
    -m		: Only mount the pnd, dont run it (-e become optional)
    -u		: Only umount the pnd, dont run it (-e become optional)
endHELP
}

list_using_fs() {
	for p in $(fuser -m $1 2>/dev/null);do ps hf $p;done
}


#=============================================================================
# CPU speed functions
PND_getCPUSpeed() {
	cat /proc/pandora/cpu_mhz_max
}

PND_setCPUSpeed() {
	unset CURRENTSPEED
	if ! [ -f "$CPUSPEED_FILE" ] && [ ! -z "$PND_CPUSPEED" ]; then
		if [ ${PND_CPUSPEED} -gt $(PND_getCPUSpeed) ]; then 
		   CURRENTSPEED=$(PND_getCPUSpeed)
        	   case "$(zenity --title="set cpu speed" --height=350 --list --column "id" --column "Please select" --hide-column=1 \
			   	  --text="$PND_NAME suggests to set the cpu speed to $PND_CPUSPEED MHz to make it run properly.\n\n Do you want to change the cpu speed? (current speed: $(PND_getCPUSpeed) MHz)\n\nWarning: Setting the clock speed above 600MHz can be unstable and it NOT recommended!" \
				  "yes" "Yes, set it to $PND_CPUSPEED MHz" \
				  "custom" "Yes, select custom value" \
				  "yessave" "Yes, set it to $PND_CPUSPEED MHz and don't ask again" \
				  "customsave" "Yes, set it to custom speed and don't ask again" \
		   		  "no" "No, don't change the speed" \
				  "nosave" "No, don't chage the speed and don't ask again")" in
			"yes")
				sudo $CPUSPEEDSCRIPT $PND_CPUSPEED
				;;
	  	  	"custom")
				sudo $CPUSPEEDSCRIPT
				;;
		  	"customsave")
				sudo $CPUSPEEDSCRIPT
				zenity --info --title="Note" --text="Speed saved.\n\nTo re-enable this dialogue, please delete the file\n$CPUSPEED_FILE"
				PND_getCPUSpeed > $CPUSPEED_FILE
				;;
         	 	"yessave")
				zenity --info --title="Note" --text="Speed saved.\n\nTo re-enable this dialogue, please delete the file\n$CPUSPEED_FILE"
				sudo $CPUSPEEDSCRIPT $PND_CPUSPEED
				PND_getCPUSpeed > $CPUSPEED_FILE
				;;
                 	"nosave")
				unset CURRENTSPEED
				zenity --info --title="Note" --text="Speed will not be changed.\n\nTo re-enable this dialogue, please delete the file\n$CPUSPEED_FILE"
				echo 9999 > $CPUSPEED_FILE
				;;
			*)	unset CURRENTSPEED;;
 	 	  esac
	       fi
	elif [ "$PND_CPUSPEED" -lt "1500" ]; then
		CURRENTSPEED=$(PND_getCPUSpeed)
		echo Setting to CPU-Speed $PND_CPUSPEED MHz
		sudo $CPUSPEEDSCRIPT $PND_CPUSPEED
	fi
}

PND_resetCPUSpeed() {
	if [ ! -z "$CURRENTSPEED" ]; then
		sudo $CPUSPEEDSCRIPT $CURRENTSPEED
	fi
}

#=============================================================================
# X management functions

PND_CloseX(){
	if [ $CLOSE_X ]; then #the app doesnt want x to run, so we kill it and restart it once the app quits
		if [ ! $(pidof X) ]; then 
			unset $CLOSE_X
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
}

PND_RestartX(){
	if [ $CLOSE_X ]; then #restart x if it was killed
		# We need to wait a bit, doing it nicely ;)
		sleep 5
		sudo /etc/init.d/slim-init start
	fi
}


#=============================================================================
# (u)Mounting functions

show_mounted_info(){
	echo "+++++++"
	echo "Loopback devices :"
	sudo /sbin/losetup -a
	echo "Are mounted on :"
	mount|grep loop
	echo "For these Union :"
	mount|grep aufs
}

is_union_mounted() {
	mount | grep -q "on $UNION_MOUNT_DIR/${PND_NAME} type aufs"
}

is_pnd_mounted() {
	mount |grep -v aufs | grep -q "on $PND_MOUNT_DIR/${PND_NAME} type" || \
	mount |grep -v aufs | grep -q "on $UNION_MOUNT_DIR/${PND_NAME} type"
}

noMoreProcessPnd() {
	[ -z "$(list_using_fs "$PND_MOUNT_DIR/$PND_NAME")" ]
}

noMoreProcessUnion() {
	[ -z "$(list_using_fs "$UNION_MOUNT_DIR/$PND_NAME")" ]
}

mountPnd() {
	MOUNT_TARGET="${1:-$PND_MOUNT_DIR}"
	if ! is_pnd_mounted;then
		#check if pnd is already attached to loop 
		LOOP=$(/sbin/losetup -a | grep "$PND" | tail -n1 | awk -F: '{print $1}')
		#check if the loop device is already mounted
		if ! [ -z "$LOOP" ];then
			echo "Found a loop ($LOOP), using it"
			loopmountedon=$( mount | grep "$(mount | grep "$LOOP" | awk '{print $3}')" | grep utmp | awk '{print $3}' )
		else
			loopmountedon=""
		fi
		if [ ! "$loopmountedon" ]; then #check if the pnd is already attached to some loop device but not used
			FREELOOP=$LOOP 
			#reuse existing loop
			if [ ! "$LOOP" ]; then
				FREELOOP=$(/sbin/losetup -f) #get first free loop device
				if [ ! "$FREELOOP" ]; then  # no free loop device, create a new one
					#find a free loop device and use it 
					usedminor=$(/sbin/losetup -a | tail -n1|sed 's/.*loop\(.*\)\: .*/\1/')
					#usedminor=${usedminor:9:1}
					freeminor=$(($usedminor+1))
					echo "Creating a new device : mknod -m777 /dev/loop$freeminor b 7 $freeminor"
					mknod -m777 /dev/loop$freeminor b 7 $freeminor
					FREELOOP=/dev/loop$freeminor
				fi
			fi

			#detect fs
			case $PND_FSTYPE in
			ISO)
				/sbin/losetup $FREELOOP "$PND" #attach the pnd to the loop device
				mntline="mount" #setup the mountline for later
				mntdev="${FREELOOP}"
				;;
			directory)
				#we bind the folder, now it can be treated in a unified way 
				#ATENTION: -o ro doesnt work for --bind at least on 25, on 26 its possible using remount, may have changed on 27
				mntline="mount --bind -o ro"
				mntdev="${PND}"
				;;
			Squashfs)
				/sbin/losetup $FREELOOP "$PND" #attach the pnd to the loop device
				mntline="mount -t squashfs"
				mntdev="${FREELOOP}"
				;;
			*)
				echo "ERROR Unknown filesystem type : $PND_FSTYPE"
				exit 1;;
			esac
			echo "Mounting : $mntline \"$mntdev\" \"$MOUNT_TARGET/${PND_NAME}\""
			$mntline "$mntdev" "$MOUNT_TARGET/${PND_NAME}" #mount the pnd/folder

			if ! is_pnd_mounted ;then
				sleep 1
				echo "WARNING : mount faild, re-tring"
				sleep 1
				$mntline "$mntdev" "$MOUNT_TARGET/${PND_NAME}" #mount the pnd/folder
				if ! is_pnd_mounted ;then
					echo "ERROR The PND File-system is not mounted !"
					show_mounted_info
					return 2
				fi
			fi
		else #the pnd is already mounted but a mount was requested with a different basename/uid, just link it there
		      echo WARNING $LOOP already mounted on $loopmountedon skipping losetup - putting link to old mount
		      #this is bullshit
		      rmdir "$UNION_MOUNT_DIR/$PND_NAME"
		      ln -s $loopmountedon "$UNION_MOUNT_DIR/$PND_NAME" 
		fi
	fi

	# For backward compatibility
	if [[ "$MOUNT_TARGET" != "$PND_MOUNT_DIR" ]];then
		if [ -d "$PND_MOUNT_DIR/$PND_NAME" ];then
			rmdir "$PND_MOUNT_DIR/$PND_NAME"
		else
			rm "$PND_MOUNT_DIR/$PND_NAME"
		fi
		if [ ! -e "$PND_MOUNT_DIR/$PND_NAME" ];then
			ln -s "$MOUNT_TARGET/$PND_NAME" "$PND_MOUNT_DIR/$PND_NAME"
		fi
	fi
}

mountUnion() {
	if [ $(id -u) -ne 0 ];then
		sudo /usr/pandora/scripts/pnd_run.sh -m -p "$PND" -b "$PND_NAME"
		if ! is_union_mounted;then
			echo "ERROR: The Union File-system is not mounted !"
			show_mounted_info
			return 1
		fi
		return $RC
	fi
	#create mountpoints, check if they exist already first to avoid annoying error messages
	if ! [ -d "$PND_MOUNT_DIR/${PND_NAME}" ]; then 
		mkdir -p "$PND_MOUNT_DIR/${PND_NAME}"		#mountpoint for iso, ro
	fi 
	#writeable dir for union
	if ! [ -d "${APPDATADIR}" ]; then 
		mkdir -p "${APPDATADIR}"
		chmod -R a+xrw "${APPDATADIR}" 2>/dev/null
	fi
	# create the union mountpoint
	if ! [ -d "$UNION_MOUNT_DIR/${PND_NAME}" ]; then
		mkdir -p "$UNION_MOUNT_DIR/${PND_NAME}"		# union over the two
	fi
	#is the union already mounted? if not mount evrything, else launch the stuff
	if ! is_union_mounted;then
		if ! is_pnd_mounted;then
			mountPnd "$UNION_MOUNT_DIR"|| return 2; # quit mounting the union if the PND first didnt mount
		else
			echo "WARNING The PND is already mounted, using it"
			show_mounted_info
		fi
		RO=0;for o in $(mount|awk '$3=="'$MOUNTPOINT'"{print $6}'|sed 's/.*(//;s/)$//;s/,/ /g');do [[ $o = "ro" ]]&& RO=1;done
		if [ $RO -eq 1 ];then
			echo "WARNING SD-Card is mounted Read-only !! Trying to remount RW"
			mount -oremount,rw $MOUNTPOINT
		fi

		if [[ "$APPDD_FSTYPE" = "vfat" ]]; then # use noplink on fat, dont on other fs's 
			#append is fucking dirty, need to clean that up
			MOUNT_CMD="mount -t aufs -o exec,noplink,dirs=\"${APPDATADIR}=rw+nolwh\":\"$PND_MOUNT_DIR/$PND_NAME=rr$append\" none \"$UNION_MOUNT_DIR/$PND_NAME\""
		else
			MOUNT_CMD="mount -t aufs -o exec,dirs=\"${APPDATADIR}=rw+nolwh\":\"$PND_MOUNT_DIR/$PND_NAME=rr$append\" none \"$UNION_MOUNT_DIR/$PND_NAME\""
		fi
		echo "Mounting the Union FS : $MOUNT_CMD"
		eval $MOUNT_CMD

		if ! is_union_mounted;then
			sleep 1
			echo "WARNING : mount faild, re-tring"
			sleep 1
			eval $MOUNT_CMD
			if ! is_union_mounted;then
				echo "ERROR: The Union File-system is not mounted !"
				show_mounted_info
				return 1
			fi
		fi
	else
		echo "WARNING Union already mounted, using it"
		show_mounted_info
	fi
}

cleanups() {
	#delete folders created by aufs if empty
	rmdir -rf "${APPDATADIR}/.wh..wh.plnk" 2>/dev/null
	rmdir -rf "${APPDATADIR}/.wh..wh..tmp" 2>/dev/null
	rmdir "${APPDATADIR}/.wh..wh.orph" 2>/dev/null
	rm "${APPDATADIR}/.aufs.xino" 2>/dev/null

	#delete appdata folder and ancestors if _empty_
	rmdir -p "${APPDATADIR}" 2>/dev/null

	# Clean the loopback device
	if [ $PND_FSTYPE = ISO ] || [ $PND_FSTYPE = Squashfs ]; then # check if we where running an iso, clean up loop device if we did
		LOOP=$(/sbin/losetup -a | grep "$(basename $PND)" | tail -n1 | awk -F: '{print $1}')
		/sbin/losetup -d $LOOP
		#rm $LOOP
	fi
	/sbin/losetup -a|cut -d':' -f 1|while read l;do
		if ! mount|grep -q $l;then
			echo "WARNING Found $l loop as unused. flushing"
			/sbin/losetup -d $l
		fi
	done

	echo cleanup done
}

umountPnd() {
	MOUNT_TARGET="${1:-$PND_MOUNT_DIR}"
	if is_pnd_mounted;then
		PND_WaitFor noMoreProcessPnd "Waiting the PND mount dir to be free"
		umount "$MOUNT_TARGET/$PND_NAME"
	fi
	if is_pnd_mounted; then
		echo WARNING umount PND failed, didnt clean up. Process still using this FS :
		list_using_fs "$MOUNT_TARGET/$PND_NAME"
		show_mounted_info
	else
		# removing the now useless mountpoint
		if [ -d "$MOUNT_TARGET/$PND_NAME" ];then
			rmdir "$MOUNT_TARGET/$PND_NAME"
		fi
		if [ -h "$PND_MOUNT_DIR/$PND_NAME" ];then
			rm "$PND_MOUNT_DIR/$PND_NAME"
		fi

		# All went well, cleaning
		cleanups
	fi
}

umountUnion() {
	# Are we root yet ?
	if [ $(id -u) -ne 0 ];then
		sudo /usr/pandora/scripts/pnd_run.sh -u -p "$PND" -b "$PND_NAME"
		return $?
	fi

	# Make sure the Union FS is unmounted
	#PND_INTERACTIVE=2
	if is_union_mounted;then
		PND_WaitFor noMoreProcessUnion "Waiting the Union to be available"
		umount "$UNION_MOUNT_DIR/$PND_NAME" #umount union
	fi
	if is_union_mounted; then
		echo "WARNING umount UNION failed, didnt clean up. Process still using this FS :"
		list_using_fs "$UNION_MOUNT_DIR/$PND_NAME"
		show_mounted_info
	else
		# the Union is umounted, removing the now empty mountpoint
		if [[ "$PND_MOUNT_DIR" != "$UNION_MOUNT_DIR" ]];then
			if [ -d "$UNION_MOUNT_DIR/$PND_NAME" ];then
				rmdir "$UNION_MOUNT_DIR/$PND_NAME"
			elif [ -e "$UNION_MOUNT_DIR/$PND_NAME" ];then
				rm "$UNION_MOUNT_DIR/$PND_NAME" >/dev/null 2>&1 # as it might be a symlink
			fi
		fi
		# Try umounting the PND
		umountPnd $UNION_MOUNT_DIR
	fi
}



#=============================================================================
# Create the condition to run an app, run it and wait for it's end
runApp() {
	cd "$UNION_MOUNT_DIR/$PND_NAME"		# cd to union mount
	if [ "$STARTDIR" ] && [ -d "$STARTDIR" ]; then
		cd "$STARTDIR";			# cd to folder specified by the optional arg -s
	fi

	if [ -d "$UNION_MOUNT_DIR/$PND_NAME/lib" ];then
		export LD_LIBRARY_PATH="$UNION_MOUNT_DIR/$PND_NAME/lib:${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"
	else
		export LD_LIBRARY_PATH="$UNION_MOUNT_DIR/$PND_NAME:${LD_LIBRARY_PATH:-"/usr/lib:/lib"}"
	fi

	if [ -d "$UNION_MOUNT_DIR/$PND_NAME/bin" ];then
		export PATH="$UNION_MOUNT_DIR/$PND_NAME/bin:${PATH:-"/usr/bin:/bin:/usr/local/bin"}"
	fi

	if [ -d "$UNION_MOUNT_DIR/$PND_NAME/share" ];then
	        export XDG_DATA_DIRS="$UNION_MOUNT_DIR/$PND_NAME/share:$XDG_DATA_DIRS:/usr/share"
	fi

	export XDG_CONFIG_HOME="$UNION_MOUNT_DIR/$PND_NAME"

	if echo "$EXENAME"|grep -q ^\.\/;then
		"$EXENAME" $ARGUMENTS
	else
		"./$EXENAME" $ARGUMENTS
	fi
	RC=$?

	#the app could have exited now, OR it went into bg, we still need to wait in that case till it really quits!
	PID=$(pidof -o %PPID -x \"$EXENAME\")	# get pid of app
	while [ "$PID" ];do			# wait till we get no pid back for tha app, again a bit ugly, but it works
		sleep 10s
		PID=`pidof -o %PPID -x \"$EXENAME\"`
	done
	return $RC
}


main() {
	case $ACTION in
	mount)	
		mountUnion
		;;
	umount)
		umountUnion
		;;
	run)
		PND_BeginTask "Mount the PND"
		mountUnion
		PND_EndTask
		if [ $? -ne 0 ];then
			zenity --warning --title="Mounting the PND failed" --text="Mounting the PND failed. The application wont start. Please have a look at $PND_LOG"
			return 3
		fi
		if [ -e /proc/pandora/cpu_mhz_max ] && [ ! -z "$PND_CPUSPEED" ];then
			PND_BeginTask "Set CPU speed"
			PND_setCPUSpeed
			PND_EndTask
		fi
		if [ $CLOSE_X ]; then
			PND_BeginTask "Closing X"
			PND_CloseX
			PND_EndTask
		fi
		oPWD=$(pwd)
		if [ -e "${APPDATADIR}/PND_pre_script.sh" ]; then
			PND_BeginTask "Starting user configured pre-script"
			. ${APPDATADIR}/PND_pre_script.sh # Sourcing so it can shared vars with post-script ;)
			PND_EndTask
		fi
		PND_BeginTask "Starting the application ($EXENAME $ARGUMENTS)"
		runApp
		PND_EndTask
		if [ -e "${APPDATADIR}/PND_post_script.sh" ]; then
			PND_BeginTask "Starting user configured post-script"
			. ${APPDATADIR}/PND_post_script.sh
			PND_EndTask
		fi
		cd $oPWD
		if [ $CLOSE_X ]; then
			PND_BeginTask "Restarting X"
			PND_RestartX
			PND_EndTask
		fi
		if [ ! -z "$CURRENTSPEED" ]; then
			PND_BeginTask "Reset CPU speed to $CURRENTSPEED"
			PND_resetCPUSpeed
			PND_EndTask
		fi
		PND_BeginTask "uMount the PND"
		umountUnion
		PND_EndTask
		;;
	esac
}

######################################################################################
####	Parsing the arguments :
##
ACTION=run
while [ "$#" -gt 0 ];do
	if [ "$#" -gt 1 ] && ( [[ "$(echo $2|cut -c 1)" != "-" ]] || [[ "$1" = "-a" ]] );then
        	case "$1" in
                -p) PND="$2";;
                -e) EXENAME="$2";;
                -b) PND_NAME="$2";;
                -s) STARTDIR="$2";;
                -j) append="$2";;
                -c) PND_CPUSPEED="$2";;
                -d) APPDATASET=1;APPDATADIR="$2";;
                -a) ARGUMENTS="$2";;
                *)	echo "ERROR while parsing arguments: \"$1 $2\" is not a valid argument"; 
			echo "Arguments were : $PND_ARGS"
			showHelp;
			exit 1 ;;
        	esac
		shift 2
	else # there's no $2 or it's an argument
        	case "$1" in
                -m) ACTION=mount;;
                -u) ACTION=umount;;
                -x) CLOSE_X=1;;
                -d) APPDATASET=1;;
                *)	echo "ERROR while parsing arguments: \"$1\" is not a valid argument"; 
			echo "Arguments were : $PND_ARGS"
			showHelp;
			exit 1 ;;
        	esac
		shift

	fi
done

# getting the real full path to the file
PND="$(readlink -f $PND)"

#PND_NAME really should be something sensible and somewhat unique
#if -b is set use that as pnd_name, else generate it from PND
#get basename (strip extension if file) for union mountpoints etc, maybe  this should be changed to something specified inside the xml
#this should probably be changed to .... something more sensible
#currently only everything up to the first '.' inside the filenames is used.
PND_NAME="${PND_NAME:-"$(basename $PND | cut -d'.' -f1)"}"

PND_LOG="/tmp/pndrun_${PND_NAME}.out"
PND_HEADER="PND PND_FSTYPE APPDATADIR APPDD_FSTYPE PND_CPUSPEED EXENAME ARGUMENTS"

if [ ! -e "$PND" ]; then #check if theres a pnd suplied, need to clean that up a bit more
	echo "ERROR: selected PND($PND) file does not exist!"
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
if [ $(df "$PND"|wc -l) -eq 1 ];then			# this is actually a bug in busybox
	MOUNTPOINT="/";
elif [ ! -d "$MOUNTPOINT" ]; then 
	MOUNTPOINT="";
fi
[ ! -z $APPDATASET ] || [ -z ${MOUNTPOINT} ] && APPDATADIR=${APPDATADIR:-$(dirname $PND)/$PND_NAME}
APPDATADIR=${APPDATADIR:-${MOUNTPOINT}/pandora/appdata/${PND_NAME}}
APPDD_FSTYPE=$(mount|awk '$3=="'${MOUNTPOINT}'"{print $5}')
CPUSPEED_FILE=${MOUNTPOINT}/pandora/appdata/${PND_NAME}/cpuspeed
if [ -f "$CPUSPEED_FILE" ]; then
	PND_CPUSPEED=$(cat "$CPUSPEED_FILE")
fi
export APPDATADIR PND PND_NAME

#Only logging when running
if [[ "$ACTION" == "run" ]];then
	PND_Start
	{
	if [ $CLOSE_X ]; then
		main 2>&1 & 
		disown
	else
		main 2>&1
	fi
	}>>"$PND_LOG"
	PND_Stop
else
	main
fi
