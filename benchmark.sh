#!/usr/bin/env bash

BINARY="./build/tsp"
DATASETS=("data/berlin52.tsp" "data/kroA200.tsp" "data/pr1002.tsp")
THREADS=(1 2 4 8 10)

if [ ! -f "$BINARY" ]; then
    echo "Error: binario no encontrado, ejecuta 'make' primero"
    exit 1
fi

for dataset in "${DATASETS[@]}"; do
    echo ""
    echo "============================================================"
    echo "  Dataset: $dataset"
    echo "============================================================"
    for t in "${THREADS[@]}"; do
        echo ""
        echo "  --- $t hilo(s) ---"
        "$BINARY" "$dataset" "$t"
    done
done
