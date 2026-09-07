#!/bin/bash
# pack.sh - build data.zip and the skin archives, without Windows.
#
# The same as zip_data.bat and zip_skins.bat together, only with the
# distribution's 7za and optipng instead of the ones in tools\. data.zip and
# the skin archives are build products that are not in Git; without them the
# game will not start.
#
#   ./pack.sh                  everything, with optipng
#   ./pack.sh --no-optipng     without the slow step
#   ./pack.sh data             data.zip only
#   ./pack.sh skins            the skins only
#
# 7za is required; optipng is optional and is skipped where it is missing. On
# Debian and Ubuntu:
#
#   sudo apt install p7zip-full optipng
#
# 7za and not the more obvious "zip": the same tool packs here as under
# Windows, and the same thing comes out. Info-ZIP writes a different format:
# for an encrypted entry it sets bit 3 of the general purpose flags, the data
# descriptor, and takes the time of day rather than the CRC for the check byte
# of the encryption header. Both versions are valid and the native build reads
# both; do not rely on that.
#
# The passwords are here in plain text, as they are in the .bat files. They
# stop nobody who goes looking - in the game they sit encrypted in main.cpp and
# the skins carry theirs along as password.txt. They are there to keep the
# files from being opened by accident, not from being opened deliberately.
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
        *) echo "unknown: $arg"; exit 2 ;;
    esac
done

command -v 7za >/dev/null 2>&1 || { echo "7za is missing - sudo apt install p7zip-full"; exit 2; }
# Without Python the XML files come straight out of data/ and carry their
# comments with them. A note, not a stopped build - the same handling as
# optipng.
strip=1
if ! command -v python3 >/dev/null 2>&1; then
    echo "(python3 is missing, the comments stay in the XML files)"
    strip=0
fi
if [ $optimize -eq 1 ] && ! command -v optipng >/dev/null 2>&1; then
    echo "(optipng is missing, the PNGs stay as they are)"
    optimize=0
fi

# optipng -o 7 is slow and works in place. It changes only the encoding, never
# a pixel - the images in the tree are the same afterwards.
runOptipng() {
    [ $optimize -eq 1 ] || return 0
    optipng -o 7 -quiet -- *.png
}

# 7za appends to an existing archive rather than replacing it. Without the rm -f
# at the caller every archive would grow with every run - which is why it is in
# the .bat files too.
packInto() { # $1=target  $2=password ("" for none)  rest=patterns
    local target=$1 password=$2; shift 2
    local files=()
    local pattern f
    shopt -s nullglob
    for pattern in "$@"; do
        for f in $pattern; do files+=("$f"); done
    done
    shopt -u nullglob
    [ ${#files[@]} -gt 0 ] || { echo "  nothing to pack for $target"; return 1; }
    if [ -n "$password" ]; then
        7za a -tzip -mx=9 -p"$password" "$target" "${files[@]}" > /dev/null
    else
        7za a -tzip -mx=9 "$target" "${files[@]}" > /dev/null
    fi
}

# The XML files come from a staging directory in which
# Tools/strip_xml_comments.py has removed their comments: the notes in the
# dialogs are to stay in the source files but not in the archive. Hence two
# calls to 7za - the second appends, as with the skins.
packData() {
    echo "data.zip ..."
    local staged=""
    if [ $strip -eq 1 ]; then
        staged=$(mktemp -d) || return 1
        python3 "$HERE/../Tools/strip_xml_comments.py" --out "$staged" "$HERE/data" || {
            rm -rf "$staged"; return 1; }
    fi
    ( cd "$HERE/data" || exit 1
      rm -f ../data.zip
      runOptipng
      packInto ../data.zip "$DATA_PASSWORD" '*.png' '*.ogg' '*.txt' '*.dat'
      cd "${staged:-$HERE/data}" || exit 1
      packInto "$HERE/data.zip" "$DATA_PASSWORD" '*.xml' ) || {
        [ -n "$staged" ] && rm -rf "$staged"
        return 1; }
    if [ -n "$staged" ]; then rm -rf "$staged"; fi
    echo "  $(unzip -l "$HERE/data.zip" | tail -1 | tr -s ' ')"
}

packSkin() { # $1=name  $2=password ("" for none)
    local name=$1 password=$2
    local dir="$HERE/levels/skins/$name"
    [ -d "$dir" ] || { echo "  $name is missing"; return 1; }
    echo "$name.zip ..."
    ( cd "$dir" || exit 1
      rm -f "../$name.zip"
      runOptipng
      # hintscroll.txt by name and not as *.txt: password.txt must stay
      # unencrypted and comes further down on its own. And the test before it,
      # because packInto drops only patterns that match nothing - a plain
      # filename survives an empty glob and 7za then fails on it.
      local marker=hintscroll.txt
      [ -f "$marker" ] || marker=
      packInto "../$name.zip" "$password" '*.xml' '*.png' $marker || exit 1
      # password.txt is added unencrypted, in a second call: the game reads it
      # out of any skin archive to get at the password of the remaining
      # members. Encrypted it would be the key to itself and therefore useless.
      [ -n "$password" ] && [ -f password.txt ] && packInto "../$name.zip" "" 'password.txt'
      true ) || return 1
}

fail=0
[ "$what" = all ] || [ "$what" = data ]  && { packData || fail=1; }
if [ "$what" = all ] || [ "$what" = skins ]; then
    packSkin blocks_01 "$SKIN_PASSWORD" || fail=1
    packSkin blocks_02 "$SKIN_PASSWORD" || fail=1
    packSkin blocks_03 "$SKIN_PASSWORD" || fail=1
    # The fourth is open on purpose: it is the one to look at to learn how a
    # skin is built.
    packSkin space "" || fail=1
fi

[ $fail -eq 0 ] && echo "done" || echo "### ERROR ###"
exit $fail
