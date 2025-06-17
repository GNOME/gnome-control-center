#!/usr/bin/env bash

srcdirs="global-shortcuts-provider panels shell subprojects/gvc"
uidirs=$srcdirs
desktopdirs=$srcdirs
policydirs=$srcdirs
xmldirs=$srcdirs

# find source files that contain gettext functions with literal or GETTEXT_PACKAGE argument
files=$(grep -ElRs --include='*.c' 'gettext2? ?\(("|GETTEXT_PACKAGE,)' $srcdirs)

# find source files that contain gettext macros
files="$files "$(grep -lRs --include='*.c' --include='*.h' '[^I_)]_(' $srcdirs)

# find ui files that contain translatable string
files="$files "$(grep -lRis --include='*.ui' 'translatable="[ty1]' $uidirs)

# find blp files that contain translatable string
files="$files "$(grep -lRis --include='*.blp' '_(' $uidirs)

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

# find out if POTFILES.in is sorted correctly, ignoring empty lines
sorted=$(grep '^[^#]' po/POTFILES.in | \
         LC_ALL=en_US.UTF-8 sort -cu 2>&1 >/dev/null | \
         awk -F ': *' '{print $5}')

if [ ${#missing} -eq 0 ] && [ ${#invalid} -eq 0 ] && [ ${#sorted} -eq 0 ]; then
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

The following files are in po/POTFILES.in but are missing, skipped or not translatable:

EOT
  for f in $invalid; do
    echo "  $f" >&2
  done
fi

if [ ${#sorted} -ne 0 ]; then
  cat >&2 << EOT

The following file is not sorted properly in po/POTFILES.in:

EOT
  echo "  $sorted" >&2
fi

echo >&2

exit 1
