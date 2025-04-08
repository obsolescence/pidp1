#!/bin/bash

# start script for pidp1.  

argc=$#
pidp1="/opt/pidp1/bin/pdp1"
pidp_dir=`dirname $pidp1`
pidp_bin=`basename $pidp1`
cd /opt/pidp1	# superfluous except for autostart

# Requires screen utility for detached pidp1 console functionality.
#
test -x /usr/bin/screen || ( echo "screen not found" && exit 0 )
test -x $pidp1 || ( echo "$pidp1 not found" && exit 0 )

# Check if pidp1 is already runnning under screen.
#
is_running() {
	procs=`screen -ls pidp1 | egrep '[0-9]+\.pidp1' | wc -l`
	return $procs
}

do_stat() {
	is_running
	status=$?
	if [ $status -gt 0 ]; then
	    echo "PiDP-1 is up." >&2
	    return $status
	else
	    echo "PiDP-1 is down." >&2
	    return $status
	fi
}

 

do_start() {
	is_running
	if [ $? -gt 0 ]; then
	    echo "PiDP-1 is already running, not starting again." >&2
	    exit 0
	fi
	
	cd $pidp_dir

	echo start panel driver
	/opt/pidp1/bin/panel_pidp1 &

	sleep 1 # only needed for autoboot at startup

	echo read boot config from sense switches
	/opt/pidp1/bin/scanpf
	sw=$?

	sw="${2:-$sw}"
	echo switches set to $sw

	echo start pidp1 in screen
	screen -dmS pidp1 /opt/pidp1/bin/pdp1
	status=$?

	sleep 1
	echo start p7simES
	/opt/pidp1/bin/p7simES localhost&

	echo "Configuring PiDP-1 for boot number $sw"
	cd /opt/pidp1	# superfluous
	cat "/opt/pidp1/bootcfg/${sw}.cfg" | /opt/pidp1/bin/pdp1ctl


	return $status
}

do_stop() {
	is_running
	if [ $? -eq 0 ]; then
	    echo "PiDP-1 is already stopped." >&2
	    status=1
	else
	    echo "Stopping PiDP-1"
	    #screen -S pidp1 -X quit
	    pkill -2 pdp1
	    status=$?
	fi
    	sleep 1
    	pkill -2 panel_pidp1
    	pkill -2 p7simES
    	sleep 1
    	pkill -2 panel_pidp1
    	pkill -2 p7simES
	return $status
}


case "$1" in
  start)
	do_start $1 $2
	;;

  stop)
	do_stop
	;;

  restart)
	do_stop
	sleep 4
	do_start $1 $2
	;;

  status)
	screen -ls pidp1 | egrep '[0-9]+\.pidp1'
	;;

  stat)
	do_stat
	;;
  ?)
	echo "Usage: pdp1control {start|stop|restart|status|stat}" || true
	exit 1
	;;
  *)
	do_stat
	if [ $status = 0 ]; then
		read -p "(S)tart, Start with boot (number), or (C)ancel? " respx
		#read -p "(S)tart or (C)ancel? " respx
		case $respx in
			[Ss]* )
				do_start
				;;
			[0-9]* )
				#boot_number=$respx
				# convert to decimal
				#boot_number=$((8#$ooot_number))
				set -- "start" "$respx"
				echo reassigned s2 to .$2. and s1 is .$1.
				do_start $1 $2
				;;
			[Cc]* )
				exit 1
				;;
			* )
				echo "Please answer with S or C.";;
				#echo "Please answer with S, a boot number, or C.";;
		esac
	else
		read -p "(S)top or (C)ancel? " respx
		case $respx in
			[Ss]* )
				do_stop
				;;
			[Cc]* )
				exit 1
				;;
			* )
				echo "Please answer with S or C.";;
		esac
	fi
esac
exit 0

