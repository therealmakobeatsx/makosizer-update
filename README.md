# MakosizerX

MakosizerX is a JUCE-based stereo lo-fi audio effect. It builds as a VST3 on
Windows, macOS, and Linux; macOS builds also include an Audio Unit
(`.component`).

## Build

JUCE 8.0.6 is fetched by CMake. Install a C++17 compiler and CMake 3.22 or
newer, then run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target MakosizerX_VST3
```

On macOS, also build the Audio Unit with
`cmake --build build --config Release --target MakosizerX_AU`. The plugin
bundles are written under `build/MakosizerX_artefacts/Release/`.
Linux users need the development packages for ALSA, OpenGL, GTK 3, and X11.
The GitHub Actions workflow builds VST3 bundles on all three platforms and an
AU bundle on macOS.

## DSP oversampling

The 1x mode bypasses oversampling. The 2x, 4x, and 8x modes use pre-initialized
JUCE `dsp::Oversampling<float>` processors with FIR equiripple anti-aliasing
filters. Drive, Destroy saturation, Vinyl saturation, and bit reduction run at
the selected internal sample rate; filters, noise, dust, modulation, width,
compression, and limiting remain at the base rate. All modes are prepared in
`prepareToPlay()` and switched without allocating on the audio thread. The
reported plugin latency is fixed to the 8x chain's latency so mode changes and
dry/wet paths remain compensated. Higher oversampling factors increase CPU
usage, with the highest cost at 8x.

## Presets

Factory presets are available in the editor. Use **LOAD** and **SAVE** to
import or export `.makopreset` files. The **CUSTOM** slot retains its in-memory
settings while another factory preset is selected. Factory presets leave
Destroy at zero; add it manually when you want extra degradation.
