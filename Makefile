
CFLAGS=-g
#CFLAGS=

SOURCES = mutex.c memory.c sched.c comm.c alt.c timer.c interrupt.c run.c hardware.c dbg.c
OBJS = $(SOURCES:.c=.o)
HDRS = alt.h comm.h hardware.h interrupt.h memory.h mutex.h par_barrier.h run.h sched.h timer.h types.h dbg.h 

all:	os

run:	${_T_}
	-./${_T_}
	
$(_T_):	${_T_}.o ${OBJS} 
	gcc -m32 ${CFLAGS} -o ${_T_} ${_T_}.o ${OBJS} -lpthread -lrt

${_T_}.o:	${_T_}.c
	gcc -m32 ${CFLAGS} -I. -c ${_T_}.c -o ${_T_}.o
		
clean:
	-rm ${OBJS}
	find examples -type f ! -name "*\.c" -exec rm {} \;

lib:	cxp.a

cxp.a:	${OBJS}
	ar rs cxp.a ${OBJS}

os:	os.o ${OBJS}
	gcc -m32 -o os $^ -lpthread -lrt

os.o:		os.c
	gcc -m32 -c $(CFLAGS) os.c

run.o:	run.c par_barrier.m ${HDRS}
	gcc -m32 -c $(CFLAGS) run.c

sched.o:	sched.c ipq.m ${HDRS}
	gcc -m32 -c $(CFLAGS) sched.c

%.o:	%.c ${HDRS}
	gcc -m32 -c $(CFLAGS) $< -o $@




