import lilv
import soundfile as sf
import numpy as np

# Carica il mondo LV2
world = lilv.World()
world.load_all()

# Carica il plugin
plugin = world.get_all_plugins().get_by_uri("http://github.com/buzzobuono/spicepedal")
instance = plugin.instantiate(44100)

# Leggi il WAV
data, samplerate = sf.read("input.wav")

# Assumendo che il plugin sia mono/stereo compatibile
# Prepariamo buffer di output
output = np.zeros_like(data)

# Processa il buffer (dipende dal plugin: alcuni hanno api più complesse)
instance.run(data.shape[0], input_ports=[data], output_ports=[output])

# Salva il file processato
sf.write("output.wav", output, samplerate)
