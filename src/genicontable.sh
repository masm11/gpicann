#!/bin/bash

set -e

for icon in $*; do
    label=$(echo ${icon} | sed -e 's|-|_|g')
    sed -e 's|"|\\"|g' -e "s|^|static const char ${label}_svg[] = \"|" -e 's|$|";|' < ../icons/${icon}.svg
    echo ''
    echo ''
done

cat <<EOF

static struct {
  const char *filename;
  const char *data;
} icon_table[] = {
EOF

for icon in $*; do
    label=$(echo $icon | sed -e 's|-|_|g')
    echo "  { \"$icon\", ${label}_svg }, "
done

cat <<EOF
};
EOF
