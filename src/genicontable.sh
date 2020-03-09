#!/bin/bash

set -e

for file in "$@"; do
    label=$(echo ${file} | sed -e 's|.*/||' -e 's|\.svg$||' -e 's|-|_|g')
    sed -e 's|"|\\"|g' -e "s|^|static const char ${label}_svg[] = \"|" -e 's|$|";|' < ${file}
    echo ''
    echo ''
done

cat <<EOF

static struct {
  const char *filename;
  const char *data;
} icon_table[] = {
EOF

for file in "$@"; do
    icon=$(echo ${file} | sed -e 's|.*/||')
    label=$(echo ${icon} | sed -e 's|\.svg$||' -e 's|-|_|g')
    echo "  { \"${icon}\", ${label}_svg }, "
done

cat <<EOF
};
EOF
