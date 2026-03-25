#!/bin/bash
# Example bash script for building a tree using the 4dg CLI

CLI="../build/4dg"

# Delete the previous version
$CLI clear tree.json

# Create blue trunk (stretched hypercube)
$CLI put hypercube trunk.json
$CLI stretch trunk.json 1 2 1 1

# Create green leaves (scaled hypersphere)
$CLI put hypersphere leaves.json
$CLI stretch leaves.json 2 2 2 2

# Combine into tree
$CLI add trunk.json tree.json 0 0 0 0
$CLI add leaves.json tree.json 0 2 0 0

# Clean up intermediates
$CLI clear trunk.json
$CLI clear leaves.json

echo "Tree built: tree.json"
