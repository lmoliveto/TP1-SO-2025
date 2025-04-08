#!/bin/bash

# Name of your script that outputs the player lines
RUNS=50
TMP_DIR=$(mktemp -d)

# Run script sequentially (to prevent shared memory issues)
for i in $(seq $RUNS); do
    ./ChompChamps -p ./player ./player_up ./player_alpha ./player_neighbor | tail -n 4 >> "$TMP_DIR/out_$i.txt"
done

# Wait for all background jobs to finish
wait

# Combine all output into one file
cat "$TMP_DIR"/out_*.txt > all_results.txt

# Process and average using awk
awk '
    /Player/ {
        name = $2

        # Extract the first score manually
        for (i = 1; i <= NF; i++) {
            if ($i == "score" && $(i+1) == "of") {
                score = $(i+2)
                break
            }
        }

        # Remove any non-numeric characters (like /)
        gsub(/[^0-9]/, "", score)

        player[name] += score
        count[name]++
    }
    END {
        printf "%-15s %-10s\n", "Player", "AvgScore"
        for (name in player) {
            avg = player[name] / count[name]
            printf "%-15s %-10.2f\n", name, avg
        }
    }
' all_results.txt

# Optional: cleanup
rm -r "$TMP_DIR"