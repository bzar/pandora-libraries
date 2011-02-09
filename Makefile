
#
# libpnd Makefile
#

# tools
CC = ${CROSSCOMPILE}gcc
CXX = ${CROSSCOMPILE}g++
AR = ${CROSSCOMPILE}ar
RANLIB = ${CROSSCOMPILE}ranlib
RM = rm

# environment
VPATH = lib test apps minimenu
CFLAG_SO = -fPIC #-fPIC not always needed, but good to have
CFLAGS = -Wall -I./include -g ${CFLAG_SO} -I${PNDSTUFF}/usr/include/SDL
CXXFLAGS = -Wall -I./include -g ${CFLAG_SO}

# code
LIB = libpnd.a 
SOLIB = libpnd.so.1         # canonicle name
SOLIB1 = libpnd.so.1.0.1    # versioned name
XMLOBJ = lib/tinyxml/tinystr.o lib/tinyxml/tinyxml.o lib/tinyxml/tinyxmlerror.o lib/tinyxml/tinyxmlparser.o
ALLOBJ = pnd_conf.o pnd_container.o pnd_discovery.o pnd_pxml.o pnd_notify.o pnd_locate.o pnd_tinyxml.o pnd_pndfiles.o pnd_apps.o pnd_utility.o pnd_desktop.o pnd_io_gpio.o pnd_io_ioctl.o pnd_io_evdev.o pnd_logger.o pnd_dbusnotify.o pnd_device.o

all: ${SOLIB} ${LIB} conftest discotest evdevtest notifytest pndnotifyd rawpxmltest pndvalidator loggertest dbusnotifytest pnd_run pndevmapperd pnd_info evtest mmenu mmwrapper

clean:
	${RM} -f ${ALLOBJ} ${XMLOBJ} ${LIB} ${SOLIB1} locatetest.o bin/locatetest conftest.o bin/conftest discotest.o evdevtest.o bin/evdevtest bin/discotest dbusnotifytest.o bin/dbusnotifytest loggertest.o bin/loggertest bin/notifytest notifytest.o bin/rawpxmltest rawpxmltest.o bin/pnd_run pnd_run.o pnd_info.o bin/pnd_info bin/pndevmapperd pndevmapperd.o bin/pndnotifyd pndnotifyd.o ${SOLIB} testdata/dotdesktop/*.desktop testdata/menu/*.desktop testdata/apps/*.pnd testdata/dotdesktop/*.png deployment/usr/lib/libpnd* deployment/usr/bin/pndnotifyd deployment/usr/bin/pnd_run deployment/usr/bin/pnd_info deployment/usr/pandora/scripts/* deployment/etc/sudoers deployment/etc/init.d/pndnotifyd bin/pndvalidator pndvalidator.o deployment/usr/bin/pndevmapperd testdata/menuicons/* evtest.o bin/evtest bin/mmenu bin/mmwrapper mmenu.o mmwrapper.o deployment/usr/bin/mmenu deployment/usr/bin/mmwrapper mmcache.o mmui.o mmcat.o mmconf.o freedsktop_cats.o freedesktop_cats.o mmcustom_cats.o
	${RM} -rf deployment/media deployment/etc/pandora/mmenu
	find . -name "*~*" -exec rm {} \; -print

# component targets
#

libpnd.a:	${ALLOBJ} ${XMLOBJ}
	${AR} r ${LIB} ${ALLOBJ} ${XMLOBJ}
	${RANLIB} ${LIB}

libpnd.so.1:	${ALLOBJ} ${XMLOBJ}
	${CC} -shared -Wl,-soname,${SOLIB} -o ${SOLIB1} ${ALLOBJ} ${XMLOBJ}
	ln -f -s ${SOLIB1} ${SOLIB}

${SOLIB1}:	${ALLOBJ} ${XMLOBJ}
	${CC} -shared -Wl,-soname,${SOLIB} -o ${SOLIB1} ${ALLOBJ} ${XMLOBJ}
	ln -f -s ${SOLIB1} ${SOLIB}

pndnotifyd:	pndnotifyd.o ${SOLIB1}
	${CC} -lstdc++ -o bin/pndnotifyd pndnotifyd.o ${SOLIB1}

pndvalidator:	pndvalidator.o ${SOLIB1}
	${CC} -lstdc++ -o bin/pndvalidator pndvalidator.o ${SOLIB1}

pnd_run:	pnd_run.o ${SOLIB1}
	${CC} -lstdc++ -o bin/pnd_run pnd_run.o ${SOLIB1}

pnd_info:	pnd_info.o ${SOLIB1}
	${CC} -lstdc++ -o bin/pnd_info pnd_info.o ${SOLIB1}

pndevmapperd:	pndevmapperd.o ${SOLIB1}
	${CC} -lstdc++ -o bin/pndevmapperd pndevmapperd.o ${SOLIB1}

mmenu:	mmenu.o mmui.o mmcache.o mmcat.o mmconf.o freedesktop_cats.o mmcustom_cats.o ${SOLIB1}
	${CC} -lstdc++ -o bin/mmenu mmenu.o mmui.o mmcache.o mmcat.o mmconf.o freedesktop_cats.o mmcustom_cats.o ${SOLIB1} -L${PNDSTUFF}/usr/lib -lSDL -lSDL_image -lSDL_ttf -lSDL_gfx
mmwrapper:	mmwrapper.o ${SOLIB1}
	${CC} -lstdc++ -o bin/mmwrapper mmwrapper.o ${SOLIB1}

# deployment and assembly components
#

pnd:
	# build x86_ls with icon
	cd testdata/pndsample; ../scripts/pnd_make.sh -p x86_ls.pnd -d x86_ls -i x86_ls/zeldaicon.png -x x86_ls/PXML.xml
	# build x86_echo with no icon
	cd testdata/pndsample; ../scripts/pnd_make.sh -p x86_echo.pnd -d x86_echo -x x86_echo/PXML.xml

deploy: 
	# populate deployment directory for copying into image bakes
	# make dirs
	mkdir -p deployment/etc/pandora/conf
	mkdir -p deployment/etc/pandora/mmenu/skins/default
	mkdir -p deployment/etc/pandora/mmenu/skins/perty
	mkdir -p deployment/usr/lib
	mkdir -p deployment/usr/bin
	mkdir -p deployment/usr/pandora/apps
	mkdir -p deployment/usr/pandora/scripts
	mkdir -p deployment/etc/init.d/
	# premake the directories that SD's mount onto; makes pndnotifyd life easier
	mkdir -p deployment/media/mmcblk0p1
	mkdir -p deployment/media/mmcblk1p1
	# copy in goodies
	cp libpnd* deployment/usr/lib
	cp bin/pndnotifyd deployment/usr/bin
	cp bin/pnd_run deployment/usr/bin
	cp bin/pnd_info deployment/usr/bin
	cp testdata/scripts/* deployment/usr/pandora/scripts
	cp bin/pndevmapperd deployment/usr/bin
	cp bin/mmenu deployment/usr/bin
	cp bin/mmwrapper deployment/usr/bin
	# copy in freebee .pnd apps to /usr/pandora/apps
	# add pndnotify to etc/rc/startup-whatever
	cp testdata/sh/pndnotifyd deployment/etc/init.d/pndnotifyd
	cp testdata/sh/sudoers deployment/etc/sudoers
	# minimenu
	cp bin/mmenu deployment/usr/bin
	cp bin/mmwrapper deployment/usr/bin
	cp minimenu/skin/default/* deployment/etc/pandora/mmenu/skins/default
	cp minimenu/skin/perty/* deployment/etc/pandora/mmenu/skins/perty

# test tool targets
#

conftest:	conftest.o ${LIB}
	${CC} -lstdc++ -o bin/conftest conftest.o libpnd.a

discotest:	discotest.o ${LIB}
	${CC} -lstdc++ -o bin/discotest discotest.o libpnd.a

evdevtest:	evdevtest.o ${LIB}
	${CC} -lstdc++ -o bin/evdevtest evdevtest.o -static libpnd.a

notifytest:	notifytest.o ${LIB}
	${CC} -lstdc++ -o bin/notifytest notifytest.o libpnd.a

locatetest:	locatetest.o ${SOLIB1}
	${CC} -lstdc++ -o bin/locatetest locatetest.o ${SOLIB1}

rawpxmltest:    rawpxmltest.o ${LIB}
	${CC} -lstdc++ -o bin/rawpxmltest rawpxmltest.o ${LIB}

loggertest:	loggertest.o ${LIB}
	${CC} -lstdc++ -o bin/loggertest loggertest.o libpnd.a

dbusnotifytest:	dbusnotifytest.o ${LIB}
	${CC} -lstdc++ -o bin/dbusnotifytest dbusnotifytest.o libpnd.a

evtest:	evtest.o
	${CC} -o bin/evtest evtest.o
