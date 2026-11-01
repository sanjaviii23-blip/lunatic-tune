# LUNATIC TUNE

Plugin de corrección de afinación en tiempo real para **Windows** (VST3 + aplicación independiente) y **macOS** (VST3 + AU + aplicación independiente, binario universal Apple Silicon / Intel).

Motor DSP propio (no usa código de Antares ni de Waves), con lo mejor de cada enfoque:

| Idea | Origen de la inspiración | En LUNATIC TUNE |
|---|---|---|
| Escala + tonalidad + notas activables | Auto-Tune Pro | Tonalidad, 12 escalas, 12 teclas on/off |
| Velocidad de retune (0 ms = efecto "hard tune") | Auto-Tune Pro | **Velocidad** 0–400 ms |
| Humanize (más natural en notas largas) | Auto-Tune Pro | **Humanizar** |
| Flex-Tune (zona de tolerancia) | Auto-Tune Pro | **Flex** 0–100 cents |
| Vibrato natural conservado | Auto-Tune Pro | **Vibrato** 0–200 % |
| Notas objetivo por MIDI | Auto-Tune Pro | botón **MIDI** |
| Latencia baja y fija, uso inmediato | Tune Real-Time | selector de **Rango** (65 / 90 / 130 / 220 Hz) |
| Control de formantes | Tune Real-Time | **Formante** ±5 st (los formantes se conservan siempre) |
| Gráfico de tono en vivo | ambos | voz detectada vs. corregida |

Extras: Transponer ±7 st, Fino ±100 cents, Referencia A4 (415–466 Hz), Mezcla, Salida, presets rápidos NATURAL y HARD TUNE.

## Cómo obtener los instaladores (sin instalar nada en tu equipo)

1. Sube esta carpeta a un repositorio de GitHub.
2. Pestaña **Actions → Compilar LUNATIC TUNE → Run workflow** (se ejecuta solo en cada push).
3. Al terminar, descarga los artefactos `LUNATIC-TUNE-Windows` y `LUNATIC-TUNE-macOS`.

## Compilar en tu propio equipo

Requisitos: CMake ≥ 3.22, Git y un compilador (Visual Studio 2022 en Windows; Xcode en macOS).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

La salida queda en `build/LunaticTune_artefacts/`. JUCE 8.0.6 se descarga solo.

Instalación manual: copia `LUNATIC TUNE.vst3` a
- Windows: `C:\Program Files\Common Files\VST3`
- macOS: `/Library/Audio/Plug-Ins/VST3` y el `.component` (AU) a `/Library/Audio/Plug-Ins/Components`

macOS: al no estar firmado ni notarizado, quita la cuarentena con `xattr -cr "LUNATIC TUNE.vst3"` (y lo mismo con el `.component` y la app).

## Probar solo el DSP

```
g++ -O2 -std=c++17 -ISource/DSP Tests/dsp_test.cpp -o dsp_test && ./dsp_test
```

## Estructura

- `Source/DSP/PitchDetector.h` — detector YIN (diezmado) con refinado a tasa completa
- `Source/DSP/PsolaShifter.h` — cambio de tono PSOLA síncrono al tono, con formantes ajustables
- `Source/DSP/AutoTuneEngine.h` — lógica de afinación (escala, retune, humanize, flex, vibrato, MIDI)
- `Source/PluginProcessor.*` — parámetros, MIDI, latencia reportada al DAW
- `Source/PluginEditor.*`, `Source/LunaticLookAndFeel.h` — interfaz minimalista oscura

## Notas importantes

- **Latencia**: depende del rango elegido (≈ 12–40 ms) y se informa al DAW para compensarla. Para monitorizar mientras cantas, usa monitorización directa de la interfaz y deja el plugin para grabar/mezclar.
- **Licencia de JUCE**: JUCE es AGPLv3 o de pago. Si distribuyes LUNATIC TUNE, publica el código bajo AGPLv3 o adquiere una licencia comercial de JUCE.
- Los nombres Auto-Tune (Antares) y Tune Real-Time (Waves) son marcas de sus dueños; aquí solo se mencionan como referencia.
