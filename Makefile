
#
# libpnd Makefile
#

# tools
CC = gcc
AR = ar
RANLIB = ranlib
RM = rm

# environment
VPATH = lib test apps
CFLAG_SO = -fPIC #-fPIC not always needed, but good to have
CFLAGS = -Wall -I./include -g ${CFLAG_SO}
CXXFLAGS = -Wall -I./include -g ${CFLAG_SO}

# code
LIB = libpnd.a 
SOLIB = libpnd.so.1         # canonicle name
SOLIB1 = libpnd.so.1.0.1    # versioned name
XMLOBJ = lib/tinyxml/tinystr.o lib/tinyxml/tinyxml.o lib/tinyxml/tinyxmlerror.o lib/tinyxml/tinyxmlparser.o
ALLOBJ = pnd_conf.o pnd_container.o pnd_discovery.o pnd_pxml.o pnd_notify.o pnd_locate.o pnd_tinyxml.o pnd_pndfiles.o

all: ${SOLIB} ${LIB} conftest discotest notifytest locatetest pndnotifyd

clean:
	${RM} -f ${ALLOBJ} ${XMLOBJ} ${LIB} ${SOLIB1} locatetest.o bin/locatetest conftest.o bin/conftest discotest.o bin/discotest bin/notifytest notifytest.o bin/pndnotifyd pndnotifyd.o ${SOLIB} testdata/dotdesktop/*.desktop

libpnd.a:	${ALLOBJ} ${XMLOBJ}
	${AR} r ${LIB} ${ALLOBJ} ${XMLOBJ}
	${RANLIB} ${LIB}

libpnd.so.1:	${ALLOBJ} ${XMLOBJ}
	${CC} -shared -Wl,-soname,${SOLIB} -o ${SOLIB1} ${ALLOBJ} ${XMLOBJ}
	ln -f -s ${SOLIB1} ${SOLIB}

conftest:	conftest.o ${LIB}
	${CC} -lstdc++ -o bin/conftest conftest.o libpnd.a

discotest:	discotest.o ${LIB}
	${CC} -lstdc++ -o bin/discotest discotest.o libpnd.a

notifytest:	notifytest.o ${LIB}
	${CC} -lstdc++ -o bin/notifytest notifytest.o libpnd.a

locatetest:	locatetest.o ${SOLIB1}
	${CC} -lstdc++ -o bin/locatetest locatetest.o ${SOLIB1}

pndnotifyd:	pndnotifyd.o ${SOLIB1}
	${CC} -lstdc++ -o bin/pndnotifyd pndnotifyd.o ${SOLIB1}
