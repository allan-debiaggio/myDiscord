#!/bin/bash

# Fix potential missing #endif directive
grep -n "#ifdef USE_DATABASE" src/server_mock_db_fixed.c > ifdef_lines.txt
grep -n "#endif" src/server_mock_db_fixed.c > endif_lines.txt

ifdef_count=$(wc -l < ifdef_lines.txt)
endif_count=$(wc -l < endif_lines.txt)

if [ "$ifdef_count" -gt "$endif_count" ]; then
    echo "Found $ifdef_count #ifdef directives but only $endif_count #endif directives"
    echo "Adding missing #endif at the end of the file"
    echo "#endif" >> src/server_mock_db_fixed.c
fi

rm ifdef_lines.txt endif_lines.txt

# Recompile the server
echo "Recompiling server..."
make server

echo "Done!" 