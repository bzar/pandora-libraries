#pandora-libraries.pro - The qmake master project for pandora-libraries
TEMPLATE = subdirs
SUBDIRS = pnd \
    pndevmapperd \
    pndnotifyd \
    pndrun \
    pndvalidator

pnd.subdir = pnd

pndevmapperd.subdir = pndevmapperd
pndevmapperd.depends = pnd

pndnotifyd.subdir = pndnotifyd
pndnotifyd.depends = pnd

pndrun.subdir = pndrun
pndrun.depends = pnd

pndvalidator.subdir = pndvalidator
pndvalidator.depends = pnd
