#!/bin/sh

RUN_DIR=/var/run
TEMP_DIR=/tmp
#PLUGIN_ROOT=/tmp/cloudPluginRoot
CLOUD_PLUGIN_PID_FILE=$1
PID_FILE=$RUN_DIR/cloudPluginMon.pid
LOGGER=echo
if logger test >/dev/null 2>/dev/null ; then
  LOGGER=logger
fi
echo $$ > ${PID_FILE}
$LOGGER -t cloudPluginMon "cloudPluginMon started"
while true ; do
  sleep 30
  pid=$(cat ${CLOUD_PLUGIN_PID_FILE} 2>/dev/null)
  if [ ! -r ${CLOUD_PLUGIN_PID_FILE} ] || [ "s$pid" == "s" ] || ! kill -0 $pid ; then
    $LOGGER -t cloudPluginMon "cloudPlugin is NOT running, start it now"
    $PLUGIN_ROOT/etc/init.d/cloudPlugin.sh start
  fi
done
