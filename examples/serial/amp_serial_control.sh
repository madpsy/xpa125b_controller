#!/bin/bash

HOST=$1
PORT=$2
TTY=$3

FREQ=0
PTT=0
PREV_FREQ=0
PREV_PTT=0

echo "setmode serial" > $TTY

while true ; do
FREQ=$(./get_rigctl.sh $HOST $PORT f)
PTT=$(./get_rigctl.sh $HOST $PORT t)

if [ $FREQ != $PREV_FREQ ] ; then
	echo "setfreq $FREQ" > $TTY
	PREV_FREQ=$FREQ
fi

if [ $PTT != $PREV_PTT ] ; then
	if [ $PTT -eq 1 ] ; then
	   echo "setstate tx" > $TTY
	else
	  echo "setstate rx" > $TTY
	fi
	PREV_PTT=$PTT
fi

sleep 0.1 # dont go full throttle
done
