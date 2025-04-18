import sys
import numpy as np
import matplotlib.pyplot as plt

def read_values(filename):
    with open(filename, 'r') as f:
        return [float(line.strip()) for line in f if line.strip()]

def main():
    if len(sys.argv) != 2:
        print("Usage: python analyze_values.py <file_with_values.txt>")
        sys.exit(1)

    filename = sys.argv[1]
    values = read_values(filename)

    mean = np.mean(values)
    std_dev = np.std(values)

    print(f"Mean: {mean}")
    print(f"Standard Deviation: {std_dev}")

    # Plotting
    plt.figure(figsize=(10, 5))
    plt.plot(values, marker='o', linestyle='-', label='Values')
    plt.axhline(mean, color='green', linestyle='--', label=f'Mean ({mean:.2f})')
    plt.axhline(mean + std_dev, color='red', linestyle='--', label=f'+1 STD ({mean + std_dev:.2f})')
    plt.axhline(mean - std_dev, color='red', linestyle='--', label=f'-1 STD ({mean - std_dev:.2f})')
    plt.title("Values with Mean and Standard Deviation")
    plt.xlabel("Index")
    plt.ylabel("Value")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
