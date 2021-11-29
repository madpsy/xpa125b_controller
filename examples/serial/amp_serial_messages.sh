#!/bin/bash

while true ; do read INPUT <$1 ; test -n "$INPUT" && printf "%s\n" "$INPUT" ; done
