#!/bin/sh
# Mimic vttest section 2 save/restore test
# Expected: 5x4 rectangle of '*' at rows 3-6, cols 10-14
# Each row: save cursor, move away, restore, print '*'

printf '\033[2J\033[H'   # Clear, home

printf '\033[3;10H'      # Move to row 3, col 10
for row in 3 4 5 6; do
    for col in 10 11 12 13 14; do
        printf '\033[%d;%dH' "$row" "$col"  # Move to position
        printf '\0337'                        # Save cursor (ESC 7)
        printf '\033[1;1H'                   # Move away to top-left
        printf 'X'                           # Print something elsewhere
        printf '\0338'                        # Restore cursor
        printf '*'                           # Should print at (row, col)
    done
done

printf '\033[10;1H'      # Move below the rectangle
printf 'Expected: 5 stars per row for rows 3-6 at cols 10-14\n'
printf 'If stars are shifted right by 1, save/restore X is off by 1\n'
