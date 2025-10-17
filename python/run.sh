#!/bin/bash
# Simple runner for Franklin Python examples

# Activate venv if it exists, otherwise use system Python
if [ -d ".venv" ]; then
    source .venv/bin/activate
fi

python3 "$@"
