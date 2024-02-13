#!/usr/bin/env bash

srcdirs="panels shell subprojects/gvc"
uidirs=$srcdirs
desktopdirs=$srcdirs
policydirs=$srcdirs
xmldirs=$srcdirs

# find source files that contain gettext keywords
files=$(grep -lRs --include='*.c' --include='*.h' '\(gettext\|[^I_)]_\)(' $srcdirs)

# find ui files that contain translatable string
files="$files "$(grep -lRis --include='*.ui' 'translatable="[ty1]' $uidirs)

# find .desktop files
files="$files "$(find $desktopdirs -name '*.desktop*')

# find .policy files
files="$files "$(find $policydirs -name '*.policy*')

# find .xml.in and .gschema.xml files
files="$files "$(find $xmldirs -not -name '*.gresource.xml*' \
                               -name '*.xml.in*' -o \
                               -name '*.gschema.xml')

# filter out excluded files
if [ -f po/POTFILES.skip ]; then
  files=$(for f in $files; do
            ! grep -q "^$f" po/POTFILES.skip && echo "$f"
          done)
fi

# find those that aren't listed in POTFILES.in
missing=$(for f in $files; do
            ! grep -q "^$f" po/POTFILES.in && echo "$f"
          done)

# find those that are listed in POTFILES.in but do not contain translatable strings
invalid=$(while IFS= read -r f; do
            ! grep -q "$f" <<< "$files" && echo "$f"
          done < <(grep '^[^#]' po/POTFILES.in))

if [ ${#missing} -eq 0 ] && [ ${#invalid} -eq 0 ]; then
  exit 0
fi

if [ ${#missing} -ne 0 ]; then
  cat >&2 << EOT

The following files are missing from po/POTFILES.in or po/POTFILES.skip:

EOT
  for f in $missing; do
    echo "  $f" >&2
  done
fi

if [ ${#invalid} -ne 0 ]; then
  cat >&2 << EOT

The following files are in po/POTFILES.in but are missing or not translatable:

EOT
  for f in $invalid; do
    echo "  $f" >&2
  done
fi

echo >&2

exit 1
