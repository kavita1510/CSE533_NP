#!/bin/bash
for KILLPID in `ps ax | grep _6872 | awk ' { print $1;}'`; do 
      kill -9 $KILLPID;
done
