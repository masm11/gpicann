#!/bin/bash

set -e

for icon in $*; do
    cat $icon.inc
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
