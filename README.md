# MakosizerX

MakosizerX is a JUCE-based stereo lo-fi audio effect. It builds as a VST3 and
Standalone application on Windows, macOS, and Linux; macOS builds also include
an Audio Unit (`.component`).

## Build

JUCE 8.0.6 is fetched by CMake. Install a C++17 compiler and CMake 3.22 or
newer, then build the Release configuration:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release \
  --target MakosizerX_VST3 MakosizerX_Standalone \
  MakosizerXDSPTests MakosizerXUITests
ctest --test-dir build --output-on-failure
```

On macOS, also build the Audio Unit with
`cmake --build build --config Release --target MakosizerX_AU`. Build outputs
are written under `build/MakosizerX_artefacts/Release/`. Linux users need the
development packages for ALSA, OpenGL, GTK 3, and X11.

## Automated releases

The `MakosizerX Release` GitHub Actions workflow builds and tests Release
configurations on all three platforms for pushes to `main`, version tags, and
manual dispatches. A manual dispatch defaults to building only Windows and
macOS, so existing Linux release packages are not rebuilt; select `all` only
when a new Linux package is also required. Manual builds upload
`MakosizerX-Windows-VST3.zip`, `MakosizerX-Setup.exe`,
`MakosizerX-macOS-AU.zip`, `MakosizerX-macOS-VST3.zip`, and `MakosizerX.pkg`
as downloadable Actions artifacts without publishing a GitHub Release. A push
to `main` automatically creates and publishes the next stable semantic
version, packages all platform downloads, and generates release notes and a
downloadable changelog. For an artifact-only Windows/macOS push build without
rebuilding Linux packages, include `[cross-platform-artifacts]` in the commit
message; that run does not publish a GitHub Release. The raw VST3 bundles and
macOS AU component are also available in the corresponding Actions build
artifacts.
Commit messages use Conventional
Commits for release increments: `feat:` advances the minor version,
`BREAKING CHANGE:` or `type!:` advances the major version, and other changes
advance the patch version. `VERSION.txt` provides the initial version baseline.

The release is published unsigned when signing credentials are not configured.
Optional GitHub Actions secrets are `WINDOWS_CERTIFICATE_BASE64` and
`WINDOWS_CERTIFICATE_PASSWORD` for Windows signing, and
`MACOS_CERTIFICATE_P12_BASE64`, `MACOS_CERTIFICATE_PASSWORD`, and
`MACOS_SIGNING_IDENTITY` for macOS app signing; set
`MACOS_INSTALLER_SIGNING_IDENTITY` to sign the PKG installer. Set `APPLE_ID`,
`APPLE_APP_SPECIFIC_PASSWORD`, and `APPLE_TEAM_ID` as well to enable macOS
notarization. None of these secrets is required to build or publish a release.

## DSP oversampling

The 1x mode bypasses oversampling. The 2x, 4x, and 8x modes use pre-initialized
JUCE `dsp::Oversampling<float>` processors with FIR equiripple anti-aliasing
filters. Drive, Destroy, Vinyl, compressor glue, bit reduction, and the final
look-ahead limiter run at the selected internal sample rate. The Vinyl control
combines filtered surface noise, sparse crackles and pops, low-level turntable
rumble, and subtle wow/flutter at progressively higher settings; Noise remains
a separate white-noise control. All oversampling modes are prepared in
`prepareToPlay()` and switched without allocating on the audio thread. The
reported plugin latency is fixed to the 8x processing path so mode changes and
dry/wet paths remain compensated. Higher oversampling factors increase CPU
usage, with the highest cost at 8x.

## Presets

Factory presets are available in the editor. Use **LOAD** and **SAVE** to
import or export `.makopreset` files. The **CUSTOM** slot retains its in-memory
settings while another factory preset is selected. Plugin state saves the
current parameter values and preset name, so a custom sound is restored after a
DAW reload or plugin restart. Factory presets leave Destroy at zero; add it
manually when you want extra degradation.
