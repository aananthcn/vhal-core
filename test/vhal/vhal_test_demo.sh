#!/bin/bash

# --- Initial Sequence ---
python vhal_test_client.py --park-brake on --speed 0 --rpm 0 --server 192.168.10.10 && sleep 1
python vhal_test_client.py --fuel 50000 --server 192.168.10.10 && sleep 1
python vhal_test_client.py --rpm 1000 --fuel 49000 --server 192.168.10.10 && sleep 1
python vhal_test_client.py --park-brake off --fuel 48000 --server 192.168.10.10 && sleep 1
python vhal_test_client.py --rpm 2000 --gear 1 --speed 8 --fuel 45000 --server 192.168.10.10 && sleep 1
python vhal_test_client.py --rpm 4000 --gear drive --speed 15 --fuel 42000 --server 192.168.10.10 && sleep 1

# --- 10-Step Increment/Decrement Loop ---
# Starting RPM: 4000 | Target RPM: 8000 | Step: +400
# Starting Speed: 15 | Target Speed: 55.5 | Step: +4.05
# Starting Fuel: 42000 | Target Fuel: -100 per step

for i in {1..10}
do
    CURRENT_RPM=$((4000 + (i * 400)))
    CURRENT_FUEL=$((42000 - (i * 2000)))
    # bc handles the decimal math for speed
    CURRENT_SPEED=$(echo "scale=2; 15 + ($i * 4.05)" | bc)
    
    echo "Step $i: RPM $CURRENT_RPM, Speed $CURRENT_SPEED, Fuel $CURRENT_FUEL"
    
    python vhal_test_client.py --rpm $CURRENT_RPM --speed $CURRENT_SPEED --fuel $CURRENT_FUEL --server 192.168.10.10
    sleep 1
done

# --- Final State ---
# This ensures we hit your final desired values exactly
python vhal_test_client.py --rpm 8000 --speed 55.5 --fuel 8000 --server 192.168.10.10