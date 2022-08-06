all: cproxy sproxy

cproxy: cproxy.c
	gcc -Wall -o cproxy cproxy.c
sproxy: sproxy.c
	gcc -Wall -o sproxy sproxy.c
