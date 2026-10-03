#!/bin/bash
# dialogs.sh - the Manager's file dialogs under Linux, through a zenity of this
# script's own: what a dialog is handed of the game's, an export started while
# the import's dialog is still open, the import taking the file it was given,
# and file names with umlauts the way Windows takes them under its UTF-8 code
# page.
#
#   LinuxBuild/build.sh hooks && LinuxBuild/test/dialogs.sh
#
# The import's dialog runs alongside the game (Transfer::pollImport) and the
# Manager stays usable meanwhile, so an export can start a second dialog while
# the first is open. A program the game starts must be handed nothing of the
# game's but its standard three descriptors: the import's pipe, handed to the
# export's dialog, would stay open in it and in whatever that started, for as
# long as they ran. Each dialog here writes down what it was handed.
#
# The second start sets B5_UTF8_NAMES, with which a hooks build converts file
# names as Windows does where UTF-8 is its code page (FileSystem::namesAreUtf8):
# the game's Latin-1 goes to the file system as UTF-8 and comes back as
# Latin-1, and a folder the platform named itself is used as it stands. Its
# user directory lies under a folder with an umlaut, as a Documents folder
# under a player's name can. The script itself is ASCII, so the umlauts are
# written as their UTF-8 bytes.
set -u
B5_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SHOTS="${B5_SHOTS:-/tmp/blocks5-dialogs}"
WORK="$SHOTS/work"
export B5_DISPLAY="${B5_DISPLAY:-:89}"
A=$'\xc3\xa4'; O=$'\xc3\xb6'; U=$'\xc3\xbc'

source "$B5_HERE/harness.sh"

# Before anything is deleted: SHOTS is another run's hook directory as much as
# this one's, and a refusal after it is gone has refused nothing.
b5_clearDisplay || exit 2
rm -rf "$SHOTS"
mkdir -p "$WORK/stubs" "$WORK/${U}ber"

# The zenity the game finds first on PATH. As the import's dialog it writes
# down its descriptors and waits until the script puts a path into "pick"; as
# the export's (--save) it writes them down and its arguments, and answers
# with the path in "save", or cancels where there is none. A subshell lists
# the shell's descriptors from outside: dash redirects a command's output in
# the shell itself and keeps the stdout it replaced above 10, so a plain
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
if [ \$kind = export ]; then
	printf '%s\n' "\$*" > "$WORK/export-args"
	[ -s "$WORK/save" ] || exit 1
	cat "$WORK/save"
	exit 0
fi
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
	rm -f "$WORK/import-fds" "$WORK/export-fds" "$WORK/export-args" "$WORK/pick" "$WORK/save"
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

# --- 2. names with umlauts, as Windows takes them under UTF-8 -----------------
# A user directory under "home-bloecke", with a level "Baer.xml" waiting in it.
export XDG_DATA_HOME="$SHOTS/home-bl${O}cke"
HOME_DIR="$XDG_DATA_HOME/blocks5"
mkdir -p "$HOME_DIR/levels"
cp "$B5_GAME/levels/example01.xml" "$HOME_DIR/levels/B${A}r.xml"
cp "$B5_GAME/levels/example02.xml" "$WORK/${U}ber/B${O}r.xml"
export B5_UTF8_NAMES=1
start utf8

# The game lists the level under its own name for it, a-umlaut one Latin-1
# byte, which the dump writes as \u00e4; sorted first, it is selected.
b5_dump
[ "$(b5_json "el('Menu.ManagerPane.Manager.Items')['selectedText'] == 'B\u00e4r.xml'")" = True ] \
	&& b5_ok "the level with an umlaut is listed by the game's name for it" \
	|| b5_note "the list's first entry is $(b5_json "repr(el('Menu.ManagerPane.Manager.Items')['selectedText'])")"

# Exported into a folder with an umlaut, under a name with one: the dialog is
# offered the name in UTF-8 and answers in UTF-8.
printf '%s\n' "$WORK/${U}ber/B${A}r-copy.xml" > "$WORK/save"
b5_click Menu.ManagerPane.Manager.Export
if waitFor "$WORK/export-args" 10; then
	grep -qF -- "--filename=$HOME/B${A}r.xml" "$WORK/export-args" \
		&& b5_ok "the export's dialog is offered the name in UTF-8" \
		|| b5_note "the export's dialog was offered: $(grep -o -- '--filename=[^ ]*' "$WORK/export-args")"
else
	b5_note "the export's dialog did not start"
fi
if waitFor "$WORK/${U}ber/B${A}r-copy.xml" 10; then
	cmp -s "$HOME_DIR/levels/B${A}r.xml" "$WORK/${U}ber/B${A}r-copy.xml" \
		&& b5_ok "the export wrote the level where the dialog said" \
		|| b5_note "the exported level differs from the one in the user directory"
else
	b5_note "nothing was exported where the dialog said: $(ls "$WORK" | tr '\n' ' ')"
fi

# Imported from that folder: the dialog's UTF-8 path is read where it points,
# and the level installed under the stem the game makes of "Boer", one
# character for the o-umlaut.
printf '%s\n' "$WORK/${U}ber/B${O}r.xml" > "$WORK/pick"
b5_click Menu.ManagerPane.Manager.Import
if waitFor "$HOME_DIR/levels/B_r.xml" 10; then
	cmp -s "$WORK/${U}ber/B${O}r.xml" "$HOME_DIR/levels/B_r.xml" \
		&& b5_ok "the import read the dialog's path and installed B_r.xml" \
		|| b5_note "the installed level differs from the one given"
else
	b5_note "no levels/B_r.xml after the import; the levels are $(ls "$HOME_DIR/levels" | tr '\n' ' ')"
fi
quit

# The user directory was used as it stands: config.xml was written into it,
# and no second folder appeared under another spelling of its name.
[ -f "$HOME_DIR/config.xml" ] && b5_ok "config.xml was written into the user directory" \
	|| b5_note "no config.xml in the user directory"
[ "$(ls -d "$SHOTS"/home-bl* | wc -l)" -eq 1 ] && b5_ok "no second user directory under another spelling" \
	|| b5_note "user directories: $(ls -d "$SHOTS"/home-bl* | tr '\n' ' ')"
unset B5_UTF8_NAMES

b5_finish
