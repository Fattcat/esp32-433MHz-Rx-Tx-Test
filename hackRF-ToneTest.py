import numpy as np
import time
from hackrf import HackRF

# Konštanty
FREQ = 433.9e6  # Frekvencia 433,9 MHz
SAMPLE_RATE = 8e6  # Vzorkovacia frekvencia HackRF (musí byť v rozsahu HackRF)
AMPLITUDE = 127  # Amplitúda signálu (0 - 127)
DURATION = 10  # Dĺžka vysielania v sekundách
TONE_FREQ = 1000  # Frekvencia tónu (1 kHz)

# Generovanie signálu
def generate_tone(sample_rate, tone_freq, amplitude, duration):
    t = np.arange(0, duration, 1 / sample_rate)
    wave = amplitude * np.sin(2 * np.pi * tone_freq * t)  # Generujeme sinusový signál
    iq_data = np.array(wave, dtype=np.int8)
    iq_data = np.repeat(iq_data, 2)  # Pre I/Q signál opakujeme hodnoty
    return iq_data

# Inicializácia HackRF
def main():
    hackrf = HackRF()

    try:
        hackrf.open()  # Otvorenie zariadenia
        hackrf.set_sample_rate(SAMPLE_RATE)  # Nastavenie vzorkovacej frekvencie
        hackrf.set_center_freq(FREQ)  # Nastavenie stredovej frekvencie
        hackrf.set_txvga_gain(20)  # Nastavenie TX zisku (0 až 47 dB)
        hackrf.start_tx_mode()  # Spustenie TX módu

        # Generovanie tónu
        iq_data = generate_tone(SAMPLE_RATE, TONE_FREQ, AMPLITUDE, DURATION)

        print(f"Vysielam tón na frekvencii {FREQ / 1e6} MHz po dobu {DURATION} sekúnd...")
        hackrf.start_tx(lambda: iq_data.tobytes())  # Spustenie vysielania dát

        time.sleep(DURATION)  # Počkať počas vysielania
        print("Vysielanie ukončené.")

    finally:
        hackrf.stop_tx_mode()  # Zastavenie TX módu
        hackrf.close()  # Zatvorenie zariadenia

if __name__ == "__main__":
    main()
