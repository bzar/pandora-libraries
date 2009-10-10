#!/bin/bash
 
######adjust path of genpxml.sh if you want to use that "feture"#####
# \!/ black magic ahead
 
TEMP=`getopt -o p:d:x:i: -- "$@"`
 
if [ $? != 0 ] ; then echo "Terminating..." >&2 ; exit 1 ; fi
 
eval set -- "$TEMP"
while true ; do
	case "$1" in
		-p) echo "PNDNAME set to $2" ;PNDNAME=$2;shift 2;;
		-d) echo "FOLDER set to $2" ;FOLDER=$2;shift 2 ;;
		-x) echo "PXML set to $2" ;PXML=$2;shift 2 ;;
		-i) echo "ICON set to $2" ;ICON=$2;shift 2 ;;
		--) shift ; break ;;
		*) echo "Error while parsing arguments! $2" ; exit 1 ;;
	esac
done
 
rnd=$RANDOM; # random number for genpxml and index$rnd.xml
 
if [ $PXML = "guess" ] && [  $PNDNAME ] && [ $FOLDER ];  then
	PXMLtxt=$(~/pndtest/genxml.sh $FOLDER $ICON)
	PXML=tmp$rnd.pxml
	echo "$PXMLtxt" > tmp$rnd.pxml
fi
 
if [ ! $PNDNAME ] || [ ! $FOLDER ] || [ ! $PXML ]; then
	echo " Usage: pnd_make.sh -p your.pnd -d folder/containing/your/app/ -x 
	your.pxml (or \"guess\" to try to generate it from the folder) -i icon.png"
	exit 1
fi
 
if [ ! -d $FOLDER ]; then echo "$FOLDER doesnt exist"; exit 1; fi
if [ ! -f $PXML ]; then echo "$PXML doesnt exist"; exit 1; fi
 
 
mkisofs -o $PNDNAME.iso -R $FOLDER
#pxmlstart=$(stat -c%s "$PNDNAME.iso")
 
cat $PNDNAME.iso $PXML >  $PNDNAME
rm $PNDNAME.iso
 
if [ $ICON ]; then # is -i used?
	if [ ! -f $ICON ]; then #does the icon actually exist?
		echo "$ICON doesnt exist"
	else # yes
	mv $PNDNAME $PNDNAME.tmp
	cat $PNDNAME.tmp $ICON > $PNDNAME
	fi
fi
 
if [ $PXML = "guess" ];then rm tmp$rnd.pxml; fi
 
#printf %08d $pxmlstart >> $PNDNAME #append end of iso/start of pxml offset

