#pandora-libraries.pro - The qmake master project for pandora-libraries
TEMPLATE = subdirs
SUBDIRS = pnd \
    pndevmapperd \
    pndnotifyd \
    pndvalidator \
    pnd_run \
	pnd_info

pnd.subdir = pnd

pndevmapperd.subdir = pndevmapperd
pndevmapperd.depends = pnd

pndnotifyd.subdir = pndnotifyd
pndnotifyd.depends = pnd

pndvalidator.subdir = pndvalidator
pndvalidator.depends = pnd

pnd_run.subdir = pnd_run
pnd_run.depends = pnd

pnd_info.subdir = pnd_info
pnd_info.depends = pnd

