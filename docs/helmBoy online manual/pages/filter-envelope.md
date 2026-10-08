# Filter Envelope

## Overview

![Filter envelope example](../pictures/Filter%20Envelope.png)

The filter envelope modulates the filter cutoff with five stages: Attack, Hold, Decay, Sustain, and Release (AHDSR).

## Parameters

### Attack

Time for the envelope to reach its peak after a note is triggered.

### Hold

Time the envelope stays at its peak after Attack and before Decay.

### Decay

Time for the envelope to fall from peak to sustain level.

### Sustain

Level maintained while a note is held.

### Release

Time for the envelope to return to zero after a note is released.

### Envelope Amount

- **Positive values**: Envelope opens the filter
- **Negative values**: Envelope closes the filter
- **Zero**: No envelope modulation

## Common Settings

### Plucked Sound

- Fast attack
- Medium decay
- Low sustain
- Short release

### Pad Sound

- Slow attack
- Long decay
- High sustain
- Long release

### Percussive Sound

- Fast attack
- Fast decay
- Zero sustain
- Short release

## See Also

- [Filter](filter.md)
- [Amplitude Envelope](amplitude-envelope.md)
- [Mod Envelope](mod-envelope.md)
