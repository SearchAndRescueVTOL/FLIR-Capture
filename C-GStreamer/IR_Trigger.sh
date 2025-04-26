#!/bin/bash
for dev in /dev/video*; do
    device_info=$(udevadm info --query=all --name="$dev")

    if echo "$device_info" | grep -q 'ID_VENDOR_ID=09cb' && echo "$device_info" | grep -q 'ID_MODEL_ID=4007'; then
        info = $(v4l2-ctl --device="$dev" --list-formats-ext)
        if echo "$info" | grep -q 'Planar YUV'; then
            echo "$dev"
            exec ./capture "$dev"
            exit 0
        fi
    fi
done
