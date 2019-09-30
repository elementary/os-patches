# This script is based on the one used on dbusmenu project
if [ -z "$DISPLAY" ] && [ -x "$XVFB_PATH" ]; then

	if [ -n "$LOG_PATH" ]; then
		xvfb_log=$LOG_PATH/Xvfb.out
	else
		xvfb_log=/dev/null
	fi

XID=`for id in $(seq 100 150); do test -e /tmp/.X$id-lock || { echo $id; exit 0; }; done; exit 1`
{ $XVFB_PATH :$XID -ac -noreset -screen 0 800x600x16 -nolisten tcp -auth /dev/null > $xvfb_log 2>&1 & trap "kill -15 $! || true" 0 HUP INT QUIT TRAP USR1 PIPE TERM ; } || { echo "Gtk+Tests:ERROR: Failed to start Xvfb environment for X11 target tests."; exit 1; }
DISPLAY=:$XID
export DISPLAY
fi
