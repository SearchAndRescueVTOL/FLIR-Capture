import numpy as np
import cv2
import os
import glob

def convert_raw_to_tiff(input_folder, width=640, height=512):
    raw_files = glob.glob(os.path.join(input_folder, '**', '*.raw'), recursive=True)
    
    if not raw_files:
        print("No .raw files found!")
        return

    for raw_path in raw_files:
        with open(raw_path, "rb") as f:
            raw_data = f.read()

        try:
            image_16bit = np.frombuffer(raw_data, dtype=np.uint16).reshape((height, width))
        except ValueError as e:
            print(f"Skipping {raw_path}: {e}")
            continue

        normalized_16bit = cv2.normalize(image_16bit, None, 0, 65535, cv2.NORM_MINMAX)

        # Create output path: same directory, same filename but .tiff
        tiff_path = os.path.splitext(raw_path)[0] + '.tiff'
        cv2.imwrite(tiff_path, normalized_16bit)
        print(f"Saved {tiff_path}")

    print("All files converted.")

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python convert_raw_to_tiff.py <input_folder>")
    else:
        folder = sys.argv[1]
        convert_raw_to_tiff(folder)

