#!/bin/bash

# Quick ROM Testing Script - Simple version
# Usage: ./quick_test.sh [ROM_DIRECTORY]

set -euo pipefail

ROM_DIR="${1:-../tests/roms}"
GBENDO="../gbendo"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}🎮 Quick GBendo ROM Test${NC}"
echo "Testing ROMs in: $ROM_DIR"
echo ""

if [[ ! -f "$GBENDO" ]]; then
    echo -e "${RED}Error: GBendo not found. Run 'make' first.${NC}"
    exit 1
fi

if [[ ! -d "$ROM_DIR" ]]; then
    echo -e "${RED}Error: ROM directory not found: $ROM_DIR${NC}"
    exit 1
fi

TOTAL=0
SUCCESS=0

for rom in "$ROM_DIR"/*.gb "$ROM_DIR"/*.gbc "$ROM_DIR"/*.rom; do
    [[ -f "$rom" ]] || continue
    
    TOTAL=$((TOTAL + 1))
    rom_name=$(basename "$rom")
    
    echo -n "Testing $rom_name... "
    
    if timeout 5s "$GBENDO" --profile "$rom" >/dev/null 2>&1; then
        echo -e "${GREEN}✓ PASS${NC}"
        SUCCESS=$((SUCCESS + 1))
    else
        echo -e "${RED}✗ FAIL${NC}"
    fi
done

echo ""
echo -e "${BLUE}Results: $SUCCESS/$TOTAL ROMs passed${NC}"

if [[ $SUCCESS -eq $TOTAL ]] && [[ $TOTAL -gt 0 ]]; then
    echo -e "${GREEN}🎉 All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}⚠️  Some tests failed${NC}"
    exit 1
fi
