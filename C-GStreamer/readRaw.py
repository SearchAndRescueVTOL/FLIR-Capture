import numpy as np
import cv2

# Set dimensions
width, height = 640, 512

# Load raw 16-bit data
with open("1.raw", "rb") as f:
    raw_data = f.read()

image_16bit = np.frombuffer(raw_data, dtype=np.uint16).reshape((height, width))

# Normalize to full 16-bit range (0 to 65535)
min_val = np.min(image_16bit)
max_val = np.max(image_16bit)

normalized_16bit = cv2.normalize(image_16bit, None, 0, 65535, cv2.NORM_MINMAX)

# Save as a proper 16-bit TIFF image
cv2.imwrite("capture_normalized_16bit.tiff", normalized_16bit)

print("Saved as 16-bit TIFF with normalized contrast.")
