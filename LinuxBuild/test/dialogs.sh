#!/bin/bash
# dialogs.sh - the Manager's file dialogs under Linux, through a zenity of this
# script's own: what a dialog is handed of the game's, an export started while
# the import's dialog is still open, and the import taking the file it was
# given.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/dialogs.sh
#
# The import's dialog runs alongside the game (Transfer::pollImport) and the
# Manager stays usable meanwhile, so an export can start a second dialog while
# the first is open. A program the game starts must be handed nothing of the
# game's but its standard three descriptors: the import's pipe, handed to the
# export's dialog, would stay open in it and in whatever that started, for as
# long as they ran. Each dialog here writes down what it was handed.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SHOTS="${B5_SHOTS:-/tmp/blocks5-dialogs}"
WORK="$SHOTS/work"
export B5_DISPLAY="${B5_DISPLAY:-:89}"

source "$B5_HERE/harness.sh"

# Before anything is deleted: SHOTS is another run's hook directory as much as
# this one's, and a refusal after it is gone has refused nothing.
b5_clearDisplay || exit 2
rm -rf "$SHOTS"
mkdir -p "$WORK/stubs"

# The zenity the game finds first on PATH. As the import's dialog it writes
# down its descriptors and waits until the script puts a path into "pick"; as
# the export's (--save) it writes them down and cancels at once. A subshell
# lists the shell's descriptors from outside: dash redirects a command's output
# in the shell itself and keeps the stdout it replaced above 10, so a plain
# "ls > file" would list the file and that copy. The shell's hold on this
# script is the one descriptor of its own left over.
cat > "$WORK/stubs/zenity" <<EOF
#!/bin/sh
case " \$* " in
	*" --save "*) kind=export ;;
	*) kind=import ;;
esac
( ls -l /proc/\$\$/fd > "$WORK/\$kind-fds.tmp" )
mv "$WORK/\$kind-fds.tmp" "$WORK/\$kind-fds"
[ \$kind = export ] && exit 1
i=0
while [ ! -s "$WORK/pick" ] && [ \$i -lt 600 ]; do sleep 0.1; i=\$((i + 1)); done
cat "$WORK/pick"
EOF
chmod +x "$WORK/stubs/zenity"
export PATH="$WORK/stubs:$PATH"
trap 'pkill -f "$WORK/stubs/zenity" 2>/dev/null; b5_stop' EXIT

# A dialog's descriptors beyond 0, 1 and 2, as "number target", leaving out the
# script its shell holds.
handed()   # $1 import or export
{
	awk -v self="$WORK/stubs/zenity" \
		'$(NF-1) == "->" && $(NF-2) !~ /^[012]$/ && $NF != self { print $(NF-2) " " $NF }' "$WORK/$1-fds"
}
waitFor()   # $1 a file, $2 seconds
{
	local i
	for i in $(seq 1 $((4 * $2))); do [ -e "$1" ] && return 0; sleep 0.25; done
	return 1
}
start()   # $1 the name of the run, with XDG_DATA_HOME set
{
	echo
	echo "=== $1"
	rm -f "$WORK/import-fds" "$WORK/export-fds" "$WORK/pick"
	B5_OUT="$SHOTS/$1"
	b5_start
	b5_waitForState GS_Menu
	b5_click Menu.Manager
	b5_expectShown Menu.ManagerPane.Manager
}
quit()
{
	# Escape closes the Manager and then quits, as a player does, so that
	# config.xml is written.
	b5_key Escape
	b5_key Escape
	local i
	for i in $(seq 1 30); do b5_alive || break; sleep 0.5; done
	b5_alive && { b5_note "the game did not quit on Escape"; b5_stop; }
	wait "$B5_GAME_PID" 2>/dev/null
	b5_stop
}

# --- 1. what the dialogs are handed ------------------------------------------
# A private home, since the import installs a level into it.
export XDG_DATA_HOME="$SHOTS/home"
cp "$B5_GAME/levels/example01.xml" "$WORK/dialogs_import.xml"
start handed

# The import's dialog opens and stays open, its pipe on its stdout.
b5_click Menu.ManagerPane.Manager.Import
if waitFor "$WORK/import-fds" 10; then
	b5_ok "the import's dialog is open"
	[ -z "$(handed import)" ] && b5_ok "the import's dialog was handed nothing of the game's beyond stdin, stdout and stderr" \
		|| b5_note "the import's dialog was handed: $(handed import | tr '\n' ' ')"
	awk '$(NF-2) == "1" { print $NF }' "$WORK/import-fds" | grep -q '^pipe:' \
		&& b5_ok "its stdout is a pipe" || b5_note "its stdout is $(awk '$(NF-2) == "1" { print $NF }' "$WORK/import-fds")"
else
	b5_note "the import's dialog did not start"
fi

# The list has selected its first level of itself, so Export is ready, and its
# dialog starts while the import's is still open.
b5_dump
[ "$(b5_json "el('Menu.ManagerPane.Manager.Export')['active']")" = True ] \
	&& b5_ok "Export is active" || b5_note "Export is not active"
b5_click Menu.ManagerPane.Manager.Export
if waitFor "$WORK/export-fds" 10; then
	b5_ok "the export's dialog opened while the import's was open"
	[ -z "$(handed export)" ] && b5_ok "the export's dialog was handed nothing of the game's beyond stdin, stdout and stderr" \
		|| b5_note "the export's dialog was handed: $(handed export | tr '\n' ' ')"
else
	b5_note "the export's dialog did not start"
fi

# The import's dialog answers, and the level is installed as it was.
echo "$WORK/dialogs_import.xml" > "$WORK/pick"
if waitFor "$XDG_DATA_HOME/blocks5/levels/dialogs_import.xml" 10; then
	cmp -s "$WORK/dialogs_import.xml" "$XDG_DATA_HOME/blocks5/levels/dialogs_import.xml" \
		&& b5_ok "the import installed the level it was given" \
		|| b5_note "the installed level differs from the one given"
else
	b5_note "no levels/dialogs_import.xml after the import's dialog answered"
fi
quit

b5_finish
