########################################################################
#
#              --- CAEN SpA - Front End Division ---
#
#   Created  :  November 2007      (Rel. 1.0)
#
#   Author: L. Colombini
#
########################################################################
ADDON_DIR = /home/jesse/Analysis/TimeTagger/V1190Readout/src/AddOn

EXE	=	V1190Readout

CC	=	g++

COPTS	=	-fPIC -DLINUX -Wall
#COPTS	=	-g -fPIC -DLINUX -Wall

FLAGS	=	-Wall -s
#FLAGS	=	-Wall

DEPLIBS	=       -l CAENVME -lncurses -lm -lstdc++

LIBS	=

INCLUDEDIR =	-I.

ROOT_LIBS   = $(shell root-config --libs)
CFLAGS = $(shell root-config --cflags --glibs) -std=c++11 -I${ADDON_DIR}

OBJS	=	src/$(EXE).o src/keyb.o ${ADDON_DIR}/TimeTaggerEvent.o ${ADDON_DIR}/TimeTaggerEvent.so rootdict.o

INCLUDES =	src/keyb.h src/V1190.h

#########################################################################

all	:	$(EXE) runAnalyzeCfs

clean	:
		/bin/rm -f $(OBJS) $(EXE) rootdict_rdict.pcm rootdict.cxx runAnalyzeCfs

$(EXE)	:	$(OBJS)
		/bin/rm -f $(EXE)
		$(CC) $(FLAGS) -o $(EXE) $(OBJS) $(DEPLIBS) $(CFLAGS) ${ROOT_LIBS}

$(OBJS)	:	$(INCLUDES)

%.o	:	%.c
		$(CC) $(COPTS) $(INCLUDEDIR) $(CFLAGS) -c -o $@ $<
		
		
rootdict.cxx: ${ADDON_DIR}/TimeTaggerEvent.h
		rootcling -f $@ -c $(CFLAGS) -p $^		
	
${ADDON_DIR}/%.o: ${ADDON_DIR}/%.cxx ${ADDON_DIR}/%.h
		g++ -c $< $(CFLAGS) -o $@

%.o: %.cxx
		g++ -c $< $(CFLAGS) -o $@

${ADDON_DIR}/%.so: ${ADDON_DIR}/%.cxx
		g++ -shared -o$@ $(CFLAGS) -fPIC $^

runAnalyzeCfs: ${ADDON_DIR}/TimeTaggerEvent.o ${ADDON_DIR}/TimeTaggerEvent.so rootdict.o AnalyzeCorrelations.C
		g++ AnalyzeCorrelations.C ${ADDON_DIR}/TimeTaggerEvent.o ${ADDON_DIR}/TimeTaggerEvent.so rootdict.o -o runAnalyzeCfs $(ROOT_LIBS) $(CFLAGS)
		
		
