#!/bin/bash
if [ $1 ]; then cd $1; fi
 
for x in $(pwd)/*
do
if [ -x $x ] && [ ! -d $x ]; then exe=$x; break; fi
done
BASENAMEnoex=$(basename "$exe" | cut -d'.' -f1)
BASENAME=$(basename "$exe")
 
rnd=$(dd if=/dev/random count=10 bs=1 | hexdump  | cut -d \  -f 2-| head -n 1 | tr -d " ")
 
echo '<?xml version="1.0"?>
<PXML>
<title>
	<en>'$BASENAMEnoex'</en>
</title>
 
<unique_id>'$rnd'</unique_id>
 
<standalone>NO</standalone>
 
<description>
	<en>Automatically generated pxml from'$(pwd)' exe='$BASENAME'</en>
</description>
 
<exec>'$BASENAME'</exec>
 
<category>
	<main>Main category</main>
	<subcategory1>Subcategory 1</subcategory1>
	<subcategory2>Subcategory 2</subcategory2>
</category>
 
</PXML>
'