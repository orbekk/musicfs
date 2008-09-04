#!/bin/bash

sqlite3 $HOME/.mfs.db < dbschema.sql

cat > $HOME/.mfsrc <<EOF
# This is the musicfs configuration file. Lines starting with "#"s are
# comments.
#
# So far, only musicpaths can be configured. Add one path on its own
# line for every music path you want musicfs to scan. Do not add
# anything on the line exept for the musicpath.
#
# Example:
# /home/orbekk/Music
EOF

echo "Initial configuration finished. Run"
echo
echo "  $ ./musicfs <mountpoint>"
echo
echo "to start musicfs, and follow the instructions in <mountpoint>/.config"
