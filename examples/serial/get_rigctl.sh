#!/bin/bash

HOST=$1
PORT=$2
CMD=$3

exec 10<>/dev/tcp/$HOST/$PORT        ## connect to rigctl and use FD 10
echo $CMD >&10                       ## send command
read RESP line <&10                  ## read response
echo $RESP
