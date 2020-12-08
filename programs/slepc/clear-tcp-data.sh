#!/bin/sh
#kill all currently-running data collection
pgrep tcp | xargs kill
rm -f $ACCEPT_DATA $CONNECT_DATA $CONNLAT_DATA $LIFE_DATA $RETRANS_DATA
