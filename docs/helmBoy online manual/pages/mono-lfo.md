# Mono LFO

## Overview

![Mono LFO section example](../pictures/Mono%20LFO.png)

HelmBoy has four Mono LFOs. Each provides a modulation signal shared across the playing voices.

## Parameters

### Frequency/Rate

- Choose a free-running rate or a tempo-synchronized division.
- Tempo modes include straight, dotted, and triplet divisions.

### Waveform

- Sine, triangle, and square
- Up and down sawtooth
- 3-, 4-, and 8-step shapes
- 3-, 5-, and 9-level pyramid shapes
- Sample and hold, and sample and glide

### Amplitude

Modulation depth from the LFO.

### Phase Stretch

The **PS** control changes the displayed LFO waveform's phase shape. The waveform viewer previews the selected waveform and phase stretch.

### Retrigger

- **Free**: LFO runs continuously
- **Retrigger**: Restarts with a new note
- **Sync to Playhead**: Follows the host playhead

## Mono vs Poly

**Mono LFOs**:

- Single LFO shared by all voices
- All notes modulated identically
- Good for synchronized effects

**Poly LFOs**:

- Independent LFO per voice
- Each note has its own modulation
- Good for evolving, complex textures

## Applications

- Vibrato (pitch modulation)
- Tremolo (volume modulation)
- Filter sweeps
- Panning effects
- Rhythmic modulation

## See Also

- [Poly LFO](poly-lfo.md)
- [Modulation](modulation.md)
- [Step Sequencer](step-sequencer.md)
