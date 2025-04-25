import subprocess
import os
import sys
import re

def get_udev_info(dev):
    try:
        result = subprocess.check_output(["udevadm", "info", "--query=all", "--name", dev], text=True)
        return result
    except subprocess.CalledProcessError:
        return ""

def has_flir_ids(info):
    return "ID_VENDOR_ID=09cb" in info and "ID_MODEL_ID=4007" in info

def has_planar_yuv(dev):
    try:
        output = subprocess.check_output(["v4l2-ctl", "--device", dev, "--list-formats-ext"], text=True)
        return "Planar YUV" in output
    except subprocess.CalledProcessError:
        return False

def main():
    for i in range(10):  # Check up to /dev/video9
        dev = f"/dev/video{i}"
        if not os.path.exists(dev):
            continue

        udev_info = get_udev_info(dev)
        if has_flir_ids(udev_info) and has_planar_yuv(dev):
            print(f"Found FLIR Boson at {dev}")
            os.execv("./capture", ["./capture", dev])  # Replace process with ./capture
            return

    print("FLIR Boson with Planar YUV not found.")
    sys.exit(1)

if __name__ == "__main__":
    main()
