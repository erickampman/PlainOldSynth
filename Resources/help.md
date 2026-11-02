<div style="display: flex; align-items: center; gap: 0.75rem; margin: 1.5rem 0 0.5rem;">
  <a href="https://www.unlikelyware.com" target="_blank" rel="noopener noreferrer">
    <img src="Unlikelyware.png" alt="Unlikelyware" style="width: 56px; height: auto; border-radius: 10px; margin: 0; display: block;">
  </a>
  <h1 style="margin: 0;">PlainOldSynth</h1>
</div>

<p>
  <em>PlainOldSynth is a CLAP and VST3 plugin on macOS and Linux, and an AUv2 plugin on macOS. Where this page says Cmd, as on the Mac, Linux uses Ctrl.</em>
</p>

::: {.callout .callout-note .card}
## On This Page
[Overview](#overview) · [Getting Started](#getting-started) · [The Panel](#panel) · [Knobs, Menus and Graphs](#controls)

**The voice —** [Oscillator](#oscillator) · [Envelopes](#envelopes) · [LFO](#lfo) · [Filter](#filter) · [Global](#global)

**Modulation and effect —** [Mod 1, Mod 2, Mod 3](#mod) · [Effect](#effect)

**Playing —** [Voices](#voices) · [MIDI 2.0, MPE and Expression](#midi) · [Presets](#presets) · [Tips](#tips) · [Troubleshooting](#troubleshooting)
:::

::: {.callout .callout-note .card}
## Overview {#overview}
PlainOldSynth is a **classic subtractive synthesizer**: one stereo-detuned Tri/Saw/Pulse oscillator into a 24 dB/octave low-pass filter and an amplifier, with two ADSR envelopes, two LFOs, glide, and Mono or Poly voicing. Three modulation sections route the envelopes, LFOs and performance controls to the oscillator and filter, and one effect (flanger, chorus or delay) runs on the whole output.

**The oscillator is really two**, panned hard left and right. **Detune** pulls them apart in opposite directions, so it widens the sound rather than making it beat in the middle.

**It speaks MIDI 2.0.** A MIDI 2.0 note can carry its exact pitch, so microtonal notes play in tune, and per-note pitch bend, pressure and timbre move just the note they belong to. MIDI 1.0, MPE controllers and CLAP / VST3 note expressions work too ([MIDI 2.0, MPE and Expression](#midi)).
:::

::: {.callout .callout-note .card}
## Getting Started {#getting-started}
1. **Load it** in your host as an instrument: CLAP, VST3, or (macOS) AUv2. It has a stereo output.
2. **Play.** The default patch is a bright saw with a short filter sweep, 10 voices.
3. **Try a preset** from the **Preset** menu at the top right ([Presets](#presets)).
4. **Shape it:** **Cutoff** and **Resonance** in the Filter panel, then drag the points of the two envelope graphs.
5. **Add movement** with a modulation section: pick a source in **In A** or **In B**, turn up its **Level**, and choose a **Dest** ([Mod 1, Mod 2, Mod 3](#mod)).

Your host saves the patch with the project. Every control is a host parameter, so all of them can be automated.
:::

::: {.callout .callout-note .card}
## The Panel {#panel}
**Across the top:** the Unlikelyware logo (a click opens unlikelyware.com), the name with "tri / saw / pulse" beside it, and at the right **Save** and **Load** (see [Presets](#presets)), **Help** (the **?** button, which opens this page in your browser) and the **Preset** menu.

**Below it, three rows of panels:**
- **Top:** [Oscillator](#oscillator), [Envelopes](#envelopes) and [LFO](#lfo).
- **Middle:** [Filter](#filter) and [Global](#global).
- **Bottom:** the three modulation sections, [Mod 1, Mod 2 and Mod 3](#mod).

The [Effect](#effect) panel is a column down the right side, beside the middle and bottom rows.

The window opens at 1200 × 750, and hosts that allow it can resize it. Everything scales with the window's width, so it looks best kept near those proportions and no smaller than 900 × 563; the plugin doesn't hold the host to either.
:::

::: {.callout .callout-note .card}
## Knobs, Menus and Graphs {#controls}
**Knobs:**
- **Drag up or down** to turn one. A full drag covers the whole range, whatever the knob.
- **Hold Ctrl or Cmd** while dragging for ten times finer steps.
- **Shift-click** or **double-click** returns a knob to its default.
- **The scroll wheel** turns it too.
- The value shows under the knob, with its unit.

**Menus:** click one to open its list, then click a choice; the current one is highlighted. A click elsewhere, **Esc**, or switching away from the plugin closes it.

**Envelope graphs:** drag a point ([Envelopes](#envelopes)). Hold **Ctrl** for fine steps (Ctrl on the Mac too: Cmd doesn't work here). **Shift-click** or **double-click** a point to reset it.
:::

::: {.callout .callout-note .card}
## Oscillator {#oscillator}
| Control | Range | Default | What it does |
|---|---|---|---|
| **Shape** | Tri/Saw, Pulse | Tri/Saw | Which of the oscillator's two outputs you hear. |
| **Pulse Width** | −0.90 to 0.90 | 0.00 | The pulse's duty cycle. 0 is a square wave; toward either end the pulse narrows. Heard with Shape on Pulse. |
| **Peak** | 0.01 to 0.99 | 0.99 | Where the waveform peaks within its cycle: 0.5 is a symmetric triangle, toward 0.99 (or 0.01) a saw. It changes the pulse too, since the pulse is cut from this shape. |
| **Pitch** | −24 to +24 st | 0 | Tuning in semitones, for both halves of the oscillator. It is continuous, not stepped, so fractions of a semitone are fine tuning: use Ctrl or Cmd drag. |
| **Detune** | 0 to 25 ct | 7 ct | Stereo spread in cents: the right side goes up by this much and the left side down. |
| **Self Mod** | 0 to 0.90 | 0 | Feeds the oscillator's output back into its own phase, adding harmonics: brighter and rougher as it rises. |

There is no separate saw choice in **Shape**: triangle and saw are the same oscillator at different **Peak** settings, so Peak is the triangle-to-saw control.
:::

<details class="disclosure">
<summary class="disclosure-title subtle">More on Self Mod</summary>

::: disclosure-content
A modulation section can also drive **Self Mod**, in either direction. The knob and the modulation are added, and the total is held between −0.9 and +0.9, past which the waveform breaks into noise. On a triangle, negative and positive amounts sound the same; on a saw they sound different.
:::

</details>

::: {.callout .callout-note .card}
## Envelopes {#envelopes}
Two ADSR envelopes, drawn as graphs. The row labelled **1** is **ADSR 1**; the row labelled **2/Amp** is **ADSR 2**, which also sets each note's loudness.

**Drag the points:**
- **The peak:** sideways for **Attack**.
- **The corner into the flat part:** sideways for **Decay**, up or down for **Sustain**.
- **The end:** sideways for **Release**.

Hovering over a point names it and shows its value. The time axis is compressed, so a few milliseconds and ten seconds both fit.

| Control | Range | ADSR 1 default | ADSR 2 default |
|---|---|---|---|
| **Attack** | 1 ms to 10 s | 10 ms | 5 ms |
| **Decay** | 1 ms to 10 s | 400 ms | 1.00 s |
| **Sustain** | 0 to 1 | 0.30 | 0.70 |
| **Release** | 1 ms to 10 s | 400 ms | 300 ms |

**ADSR 1 does nothing on its own.** It's a modulation source: the default patch sends it to the filter's cutoff through **Mod 1**. Point a modulation section at whatever it should move.

Both envelopes have the exponential shape of a classic analog synth.
:::

::: {.callout .callout-note .card}
## LFO {#lfo}
Two low-frequency oscillators, one per row. Like ADSR 1, they move nothing until a [modulation section](#mod) uses them: **LFO 1** is offered in **In A**, **LFO 2** in **In B**.

| Control | Range | Default | What it does |
|---|---|---|---|
| **LFO 1 Rate**, **LFO 2 Rate** | −7 to +7 oct | 0 oct | Speed, in octaves from 1 Hz: 0 is 1 Hz, +1 is 2 Hz, −1 is 0.5 Hz. The range runs from about one cycle every two minutes to 128 Hz. |
| **LFO 1 Wave**, **LFO 2 Wave** | Sine, Tri, Square | Sine (LFO 1), Tri (LFO 2) | The waveform. |

The LFOs run freely: they don't restart with each note.
:::

::: {.callout .callout-note .card}
## Filter {#filter}
A 24 dB/octave low-pass filter, after the oscillator and before the amplifier.

| Control | Range | Default | What it does |
|---|---|---|---|
| **Cutoff** | −6 to +5.4 oct | 2 oct | The cutoff frequency, in octaves from A 440 Hz: 0 is 440 Hz, 2 is 1760 Hz. The range runs from about 7 Hz to about 18.5 kHz. |
| **Resonance** | 1 to 20 | 1 | The filter's Q: a peak at the cutoff that grows as it rises. |
| **Key Track** | 0 to 2 | 0 | How far the cutoff follows the note you play. 0 holds it still; 1 moves it as far as the pitch moves (an octave up the keyboard, an octave up the cutoff); 2 moves it twice as far. |

How far the filter envelope moves the cutoff is set in a modulation section: **Mod 1** sends **ADSR 1** to **Cutoff** at Level 0.25 by default.
:::

::: {.callout .callout-note .card}
## Global {#global}
Four menus in two stacked pairs (**Voices** over **Mono Mode**, **MPE** over **Velocity**), then three knobs: **Glide**, **Level** and **Spread**.

| Control | Range | Default | What it does |
|---|---|---|---|
| **Voices** | Mono, Poly 10, Poly 20, Poly 32 | Poly 10 | One voice, or up to 10, 20 or 32 notes at once ([Voices](#voices)). |
| **Mono Mode** | Legato, Retrigger | Legato | In Mono, what a new note does while another is held. **Legato** slides into it without restarting the envelopes. **Retrigger** restarts their attack from where they are. |
| **MPE** | Off, On | Off | Turn on for an MPE controller ([MIDI 2.0, MPE and Expression](#midi)). |
| **Velocity** | Off, Light, Medium, Full | Off | How much velocity sets a note's loudness (below). |
| **Glide** | 0 to 5 s | 0 | Portamento: how long the pitch takes to slide from one note to the next. **Mono only**; in Poly it does nothing. |
| **Level** | 0 to 1 | 0.70 | Output level. |
| **Spread** | 0 to 1 | 0.50 | Stereo width of the output: 0 is mono, 1 is the full width Detune creates. |

**Velocity settings.** Velocity 127 is always full level.

| Setting | Quietest note | A note at velocity 64 |
|---|---|---|
| Off | full level | full level |
| Light | 50% | −2.5 dB |
| Medium | 20% | −6 dB |
| Full | 0% | −12 dB |

Velocity can also be a modulation source, apart from this setting ([Mod 1, Mod 2, Mod 3](#mod)).
:::

::: {.callout .callout-note .card}
## Mod 1, Mod 2, Mod 3 {#mod}
Three modulation sections, each the same. Each mixes two sources and sends the result to one destination. The columns, left to right:

| Column | Top | Bottom |
|---|---|---|
| Sources | **In A**: None, LFO 1, ADSR 1 | **In B**: None, LFO 2, ADSR 2, CC #1, Pressure, Timbre, Velocity |
| Levels | **Level A**, −1 to 1 | **Level B**, −1 to 1 |
| Output | **Mode**: Add, Multiply | **Dest**: Off, Osc Pitch, Pulse Width, Peak, Cutoff, Self Mod |

- **Levels** are how far each source moves the destination. 0 is none; a negative level turns the source upside down. They default to 0, so a section is silent until you turn one up.
- **Mode Add** sums the two scaled sources.
- **Mode Multiply** multiplies the two scaled sources: Level A × In A × Level B × In B, so one source scales the other. The Levels are plain gains here, as in Add, so either Level at 0 silences the section, and a source that swings both ways (an LFO) still swings around the destination's setting: LFO 1 × CC #1 into Osc Pitch is vibrato that the mod wheel deepens, without moving the pitch. With one source set to **None**, the other passes through alone at its Level. At Levels 1 and 1 the section reaches the destination's full range ([The destinations](#mod), below).
- **A destination belongs to one section.** Choosing a Dest that another section already uses turns that other section's Dest back to **Off**.

**The defaults:** Mod 1 is ADSR 1 to Cutoff at Level A 0.25 (the filter sweep). Mod 2 and Mod 3 are off.
:::

<details class="disclosure">
<summary class="disclosure-title subtle">The sources</summary>

::: disclosure-content
- **LFO 1, LFO 2** swing both ways (−1 to +1), so they move a destination up and down.
- **ADSR 1, ADSR 2** run from 0 to 1, per note.
- **CC #1** is the mod wheel, on any channel.
- **Pressure** is aftertouch, per note: polyphonic (key) pressure, MIDI 2.0 per-note pressure, or a CLAP / VST3 Pressure expression moves one note; channel pressure moves every note on its channel.
- **Timbre** is CC 74, per note: MIDI 2.0 per-note controller 74, or a CLAP / VST3 Brightness expression, moves one note; CC 74 moves every note on its channel. It's the "slide" axis of MPE controllers.
- **Velocity** is how hard the note was played, 0 to 1, held for the whole note.

Everything except the LFOs runs from 0 to 1, so with a positive level it only pushes its destination one way: up from where the knob sets it. Use a negative level to push it down.
:::

</details>

<details class="disclosure">
<summary class="disclosure-title subtle">The destinations</summary>

::: disclosure-content
Each moves the matching control, on top of where its knob sits. At Level 1:
- **Osc Pitch** — ±2 octaves. A small level gives vibrato: about 0.03 with LFO 1 is a gentle one.
- **Pulse Width** — the pulse's whole range (pulse-width modulation, with Shape on Pulse).
- **Peak** — the triangle-to-saw shape.
- **Cutoff** — ±8 octaves.
- **Self Mod** — the whole −0.9 to +0.9 range ([Oscillator](#oscillator)).
:::

</details>

::: {.callout .callout-note .card}
## Effect {#effect}
One effect on the whole output, after all the voices are mixed. **Mode** chooses what it is. The panel has three rows: the **Mode** menu with **Time** and **Feedback**; **Tone**, **Rate** and **Depth**; then **Spread** and **Mix** on the right.

| Control | Range | Default | What it does |
|---|---|---|---|
| **Mode** | Off, Flanger, Chorus, Delay, Sync Delay | Off | The effect. Off bypasses it completely. |
| **Time** | depends on Mode (below) | the middle | The delay time. |
| **Feedback** | −0.95 to 0.95 | 0 | How much of the output goes round again: more repeats, or a stronger flange. Negative feedback gives the hollow flanger sound. |
| **Tone** | 0 to 1 | 1 | Darkens the delayed signal, and each repeat further. 1 leaves it untouched. |
| **Rate** | 0.05 to 10 Hz | 0.5 Hz | Speed of the sweep that moves the delay time. |
| **Depth** | 0 to 1 | 0.5 | How far that sweep moves it. |
| **Spread** | 0 to 1 | 1 | Stereo width. In Flanger and Chorus it offsets the right side's sweep from the left's (opposite at 1). In Delay and Sync Delay it turns the echoes into ping-pong, bouncing between left and right. |
| **Mix** | 0 to 1 | 0.5 | Dry against effect. 0 is dry only, 1 effect only; at 0.5 both are at full level. |

**Time, Depth and Mode:**

| Mode | Time | Depth at 1 |
|---|---|---|
| Flanger | 0.5 to 10 ms | sweeps the time from about a tenth to nearly twice its setting |
| Chorus | 5 to 40 ms | swings it by up to half its setting |
| Delay | 20 ms to 2 s | up to 5 ms of tape-like wobble |
| Sync Delay | 1/32 to 1/1, with dotted (D) and triplet (T) values | follows the host's tempo, or 120 BPM if the host gives none |

Changing Time glides the delay rather than jumping. Changing Mode clears what's in the delay, so an old echo never comes back.
:::

::: {.callout .callout-note .card}
## Voices {#voices}
- **Poly 10, Poly 20, Poly 32** play that many notes at once. When all are in use, a new note takes over a sounding one.
- **Mono** plays one note at a time. Hold one key and play another, and the voice moves to the new note; let go, and it returns to the key still held. **Mono Mode** chooses whether the envelopes carry on (**Legato**) or restart (**Retrigger**), and **Glide** slides the pitch between them.

**Glide works in Mono only.** In Poly, each note would slide in from wherever its voice last was, so Glide is held at zero there.

**The sustain pedal** (CC 64) holds notes in Poly. In Mono, it doesn't.

PlainOldSynth tells CLAP hosts how many voices it is using, and that one key can sound more than once (Bitwig, for one, can play the same key again before the first has ended).
:::

::: {.callout .callout-note .card}
## MIDI 2.0, MPE and Expression {#midi}
**MIDI 2.0.** PlainOldSynth prefers MIDI 2.0 when the host offers it.
- **Exact pitch:** a note that carries a pitch (MIDI 2.0's Pitch 7.9) plays at that pitch, so microtonal notes are in tune.
- **Per-note pitch bend** bends only its own note, by the same range as channel bend (±2 semitones) unless the controller sets a per-note range.
- **Per-note pitch** (Pitch 7.25) sets a sounding note's exact pitch, without glide.
- **Per-note pressure** and **per-note controller 74** drive the **Pressure** and **Timbre** sources for one note.
- **Velocity 0** is a real, quiet note in MIDI 2.0 (only in MIDI 1.0 does it mean note-off).

**Pitch bend** is ±2 semitones unless a controller sets the range (RPN 0). Each note follows the bend of the channel it was played on. Bend is added after glide, so a bend is never smeared.

**MPE** (the **MPE** switch in Global). Turn it on for an MPE controller sending MIDI 1.0: a Seaboard, LinnStrument, Osmose or Continuum, or a host that passes MPE through. It expects the usual **lower zone**: channel 1 is the master channel, and channels 2–16 carry one note each. With it on:
- Each note's own bend is **±48 semitones**, as MPE expects.
- The master channel's bend, pressure and CC 74 reach every note.
- A note keeps its bend, pressure and timbre once you let go, through its release.

Leave MPE off for everything else, including MIDI 2.0 controllers: they don't need it. The upper zone (master channel 16) isn't supported.

**CLAP and VST3 note expressions.** Hosts that send them (Bitwig, for one) can drive **Tuning** (the note's pitch), **Pressure** (the Pressure source) and **Brightness** (the Timbre source) per note. AUv2 has no per-note expression.

**All Notes Off** (CC 123) releases the notes on its channel, held ones too; **All Sound Off** (CC 120) silences them at once. Neither stops the effect's tail.
:::

::: {.callout .callout-note .card}
## Presets {#presets}
The **Preset** menu, top right, holds six factory presets:
- **Init Saw** — the default patch: a saw with a short filter sweep.
- **Big Bass** — a mono saw an octave down, with a little glide.
- **Pulse Pad** — slow pulse pad with LFO 2 on the pulse width.
- **Solo Lead** — mono saw lead with LFO 1 vibrato.
- **PWM Strings** — pulse strings with LFO 2 on the pulse width.
- **Tri Pluck** — a short triangle pluck, an octave up.

Choosing one first puts every control back to its default, then applies the preset, so nothing is left over from the last patch. The menu shows "--" until you pick one.

**Your own patches:** **Save** and **Load**, left of Help, write the patch to a file and read one back, through the system's file dialog. The first time, the dialog opens on **Documents/PlainOldSynth**; after that, on the last folder you used.
- A saved patch has every control but **MPE**. Loading one works like choosing a preset: every control goes back to its default, then takes the file's value. MPE is left alone.
- The menu then shows the patch's name, dimmed, in place of a factory preset's.
- The file is the same JSON as the factory presets, so a patch you save can be put in the source's `Presets` folder to build it in as a factory preset.
- The host still saves the patch with its project, whether or not you saved it to a file.
:::

::: {.callout .callout-note .card}
## Tips {#tips}
- **For a square wave,** choose Pulse with Pulse Width at 0. For a classic pulse-width sweep, send LFO 2 to **Pulse Width** at a modest Level B.
- **For a fatter mono sound,** narrow **Detune**, or turn **Spread** down: Detune's width is all stereo.
- **For a filter sweep that follows velocity,** set Mod 1 to **Multiply**, put **Velocity** in In B, and turn Level A and Level B to 1. The sweep now spans up to 8 octaves, so bring **Cutoff** down to taste.
- **For fine tuning,** hold Ctrl or Cmd and drag **Pitch**.
- **For vibrato,** send LFO 1 to **Osc Pitch** in Add mode with Level A around 0.03, and set LFO 1 Rate near 2.3 oct (about 5 Hz), as **Solo Lead** does.
:::

::: {.callout .callout-note .card}
## Troubleshooting {#troubleshooting}
- **No sound:** check **Level** in Global, and that ADSR 2's Sustain isn't 0 with a short Decay. A **Cutoff** turned all the way down silences most notes.
- **Glide does nothing:** Voices must be **Mono**.
- **An LFO or ADSR 1 does nothing:** they move nothing until a modulation section uses them. Check the section's **Level**, and that its **Dest** isn't Off.
- **A modulation section switched itself to Off:** another section was pointed at the same destination. Each destination belongs to one section.
- **The effect is silent:** Mode is **Off**, or **Mix** is at 0.
- **MPE bends are far too wide, or notes jump:** turn **MPE** off unless the controller is MPE. MIDI 2.0 controllers don't need it.
- **Notes ring on after the pedal is up:** they finish their release from ADSR 2; shorten **Release**.
- **A rebuilt or updated plugin doesn't change:** quit and restart the host. A running host keeps the copy it loaded first.
:::
