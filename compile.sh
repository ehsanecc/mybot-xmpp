#!/bin/bash

REQUIREMENTS="libexpat;libxml libm libpthread libresolv libstrophe"

echo "checking for requirements..."
for rq in $REQUIREMENTS; do
	if [ "$(locate $(echo $rq | sed s/\;/\ /g))" == "" ]; then
		echo "$rq required but not located."
		error=1
		nstrophe=1
	fi
done

if [ "$error" == "1" ]; then
	echo please install requirements then try again.
	exit
fi

function compile {
	echo "compiling..."
	if [ $(gcc mybot-xmpp.c mybot-functions.c datatypes.c ./modules/responser.c mybot-xmpp.h strophe.h datatypes.h ./modules/responser.h  -lstrophe -lexpat -lssl -lpcre -lpthread -lresolv -o ./build/mybot-xmpp) ]; then
		echo "done"
	else
		echo "please contact the author for any problem"
	fi
}

if [ -d build ]; then
	compile
else
	mkdir build
	compile
fi
