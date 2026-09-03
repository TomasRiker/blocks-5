#!/bin/bash
# pack.sh - data.zip und die Skin-Archive bauen, ohne Windows.
#
# Dasselbe wie zip_data.bat und zip_skins.bat zusammen, nur mit zip statt
# 7za und mit dem optipng der Distribution statt dem in tools\. Beides sind
# Bauergebnisse und liegen nicht im Git; ohne sie startet das Spiel nicht.
#
#   ./pack.sh                  alles, mit optipng
#   ./pack.sh --no-optipng     ohne den langsamen Schritt
#   ./pack.sh data             nur data.zip
#   ./pack.sh skins            nur die Skins
#
# Gebraucht wird zip; optipng ist freiwillig und wird uebersprungen, wenn es
# fehlt. Unter Debian und Ubuntu:
#
#   sudo apt install zip optipng
#
# Die Passwoerter stehen hier im Klartext, wie in den .bat-Dateien auch. Sie
# halten niemanden auf, der sie sucht - im Spiel stehen sie verschluesselt in
# main.cpp und die Skins tragen ihres als password.txt bei sich. Sie sollen die
# Dateien davor bewahren, versehentlich geoeffnet zu werden, nicht davor,
# absichtlich geoeffnet zu werden.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DATA_PASSWORD=argonhydroxid267
SKIN_PASSWORD=trockeneiskaefer

optimize=1
what=all
for arg in "$@"; do
    case "$arg" in
        --no-optipng) optimize=0 ;;
        data|skins|all) what=$arg ;;
        *) echo "unbekannt: $arg"; exit 2 ;;
    esac
done

command -v zip >/dev/null 2>&1 || { echo "zip fehlt - sudo apt install zip"; exit 2; }
if [ $optimize -eq 1 ] && ! command -v optipng >/dev/null 2>&1; then
    echo "(optipng fehlt, die PNGs bleiben wie sie sind)"
    optimize=0
fi

# optipng -o 7 ist langsam und arbeitet an Ort und Stelle. Es aendert nur die
# Kodierung, nie ein Pixel - die Bilder im Baum sind danach dieselben.
runOptipng() {
    [ $optimize -eq 1 ] || return 0
    optipng -o 7 -quiet -- *.png
}

# zip haengt an ein vorhandenes Archiv an, statt es zu ersetzen. Ohne das hier
# wuechse jedes Archiv bei jedem Lauf.
packInto() { # $1=Ziel  $2=Passwort ("" fuer keins)  Rest=Muster
    local target=$1 password=$2; shift 2
    local files=()
    local pattern f
    shopt -s nullglob
    for pattern in "$@"; do
        for f in $pattern; do files+=("$f"); done
    done
    shopt -u nullglob
    [ ${#files[@]} -gt 0 ] || { echo "  nichts zu packen fuer $target"; return 1; }
    if [ -n "$password" ]; then
        zip -q -9 -P "$password" "$target" "${files[@]}"
    else
        zip -q -9 "$target" "${files[@]}"
    fi
}

packData() {
    echo "data.zip ..."
    ( cd "$HERE/data" || exit 1
      rm -f ../data.zip
      runOptipng
      packInto ../data.zip "$DATA_PASSWORD" '*.xml' '*.png' '*.ogg' '*.txt' '*.dat' ) || return 1
    echo "  $(unzip -l "$HERE/data.zip" | tail -1 | tr -s ' ')"
}

packSkin() { # $1=Name  $2=Passwort ("" fuer keins)
    local name=$1 password=$2
    local dir="$HERE/levels/skins/$name"
    [ -d "$dir" ] || { echo "  $name fehlt"; return 1; }
    echo "$name.zip ..."
    ( cd "$dir" || exit 1
      rm -f "../$name.zip"
      runOptipng
      packInto "../$name.zip" "$password" '*.xml' '*.png' || exit 1
      # password.txt kommt unverschluesselt dazu, in einem zweiten Aufruf: das
      # Spiel liest es aus jedem Skin-Archiv heraus, um an das Passwort der
      # uebrigen Mitglieder zu kommen. Verschluesselt waere es fuer sich selbst
      # der Schluessel und damit nutzlos.
      [ -n "$password" ] && [ -f password.txt ] && packInto "../$name.zip" "" 'password.txt'
      true ) || return 1
}

fail=0
[ "$what" = all ] || [ "$what" = data ]  && { packData || fail=1; }
if [ "$what" = all ] || [ "$what" = skins ]; then
    packSkin blocks_01 "$SKIN_PASSWORD" || fail=1
    packSkin blocks_02 "$SKIN_PASSWORD" || fail=1
    packSkin blocks_03 "$SKIN_PASSWORD" || fail=1
    # Der vierte ist mit Absicht offen: er ist der, an dem sich abschauen
    # laesst, wie ein Skin gebaut wird.
    packSkin space "" || fail=1
fi

[ $fail -eq 0 ] && echo "fertig" || echo "### FEHLER ###"
exit $fail
