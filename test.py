import numpy as np
from scipy.fft import fft, fftfreq
import matplotlib.pyplot as plt

# Sample data parameters
num_samples = 1000  # Number of data points
sampling_frequency = 1000  # Sampling frequency in Hz
signal_frequency = 5  # Frequency of the signal in Hz
amplitude = 2  # Amplitude of the signal

# Generate a sample sinusoidal signal
t = np.linspace(0, (num_samples - 1) / sampling_frequency, num_samples)
signal = amplitude * np.sin(2 * np.pi * signal_frequency * t)

# Perform FFT on the signal
fft_result = fft(signal)

# Calculate the associated frequency values using fftfreq
frequencies = fftfreq(num_samples, 1 / sampling_frequency)

# Plot the signal and its FFT
plt.figure(figsize=(12, 6))

plt.subplot(2, 1, 1)
plt.plot(t, signal)
plt.title("Original Signal")
plt.xlabel("Time (s)")
plt.grid(True)

plt.subplot(2, 1, 2)
plt.plot(frequencies, np.abs(fft_result))
plt.title("Magnitude of FFT Coefficients")
plt.xlabel("Frequency (Hz)")
plt.grid(True)

plt.tight_layout()
plt.show()

