import math
import matplotlib.pyplot as plt

def generate_gamma_lut(gamma):
    return [
        int(round((i / 255.0) ** gamma * 255.0))
        for i in range(256)
    ]

def print_c_array(lut, varname="gamma_lut"):
    print(f"static const uint8_t {varname}[256] = {{")
    for i in range(0, 256, 16):
        line = lut[i:i+16]
        line_str = ', '.join(f"{v:3d}" for v in line)
        print(f"    {line_str},")
    print("};")

def plot_lut(lut, gamma):
    plt.plot(range(256), lut, label=f"gamma={gamma}")
    plt.xlabel("Input")
    plt.ylabel("Output")
    plt.title("Gamma Correction LUT")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.show()

# === CONFIG ===
gamma = 0.5

lut = generate_gamma_lut(gamma)
print_c_array(lut)
plot_lut(lut, gamma)

