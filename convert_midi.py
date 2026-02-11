"""
convert_midi.py

Convert standard MIDI files to Virtual Boy 4-channel music data.

Output per song (C header):
  - int   prefix_melody[N]  : (vel << 12) | (midi_note << 4)  for ch0
  - int   prefix_bass[N]    : (vel << 12) | (midi_note << 4)  for ch1
  - int   prefix_chords[N]  : (vel << 12) | (midi_note << 4)  for ch3
  - int   prefix_drums[N]   : (tap << 12) | freq               for ch5 (NOISE)
  - u16   prefix_timing[N]  : shared duration in milliseconds per step
  - u8    prefix_arp[N]     : arpeggio offsets (optional, 0=none)
  - #define PREFIX_NOTE_COUNT N

All four channels share a single timing array (lock-step).
Timing values are in milliseconds for rate-independent playback.

Tonal channels (melody/bass/chords) encode MIDI note number:
  bits 15-12 = velocity (0-15, where 0 means silence/rest)
  bits 10-4  = MIDI note number (0-127)
  bits 11, 3-0 = unused (0)
  The VB player uses a lookup table to convert MIDI note -> VB freq register.

Arpeggio array (tracker-style 0xy effect):
  bits 7-4 = semitone offset for 2nd note (0-15)
  bits 3-0 = semitone offset for 3rd note (0-15)
  0x00 = no arpeggio (hold base note)
  Player cycles: base, base+hi_nib, base+lo_nib at ~50Hz frame rate.

Drums are encoded differently in bits 14-12 and 11-0:
  bits 14-12 = noise tap register (0-7, controls timbre)
  bits 11-0  = noise frequency register
  Value 0 = silence (rest)
"""

import os
import sys
import mido

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MUSIC_DIR = os.path.join(SCRIPT_DIR, "music")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "audio")

# VB frequency register values for MIDI note numbers 0-127.
# Taken directly from dph9.h (the proven VB note table).
MIDI_TO_VB_FREQ = [
    # 0-11: C-1 to B-1 (inaudible)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    # 12-23: C0 to B0 (inaudible)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    # 24-35: C1 to B1 (inaudible)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    # 36-39: C2 to D#2 (inaudible)
    0x00, 0x00, 0x00, 0x00,
    # 40: E2   41: F2   42: F#2  43: G2   44: G#2  45: A2   46: A#2  47: B2
    0x02C, 0x09C, 0x106, 0x16B, 0x1C9, 0x223, 0x277, 0x2C6,
    # 48: C3   49: C#3  50: D3   51: D#3  52: E3   53: F3   54: F#3  55: G3
    0x312, 0x356, 0x39B, 0x3DA, 0x416, 0x44E, 0x483, 0x4B5,
    # 56: G#3  57: A3   58: A#3  59: B3   60: C4   61: C#4  62: D4   63: D#4
    0x4E5, 0x511, 0x53B, 0x563, 0x589, 0x5AC, 0x5CE, 0x5ED,
    # 64: E4   65: F4   66: F#4  67: G4   68: G#4  69: A4   70: A#4  71: B4
    0x60A, 0x627, 0x642, 0x65B, 0x672, 0x689, 0x69E, 0x6B2,
    # 72: C5   73: C#5  74: D5   75: D#5  76: E5   77: F5   78: F#5  79: G5
    0x6C4, 0x6D6, 0x6E7, 0x6F7, 0x706, 0x714, 0x721, 0x72D,
    # 80: G#5  81: A5   82: A#5  83: B5   84: C6   85: C#6  86: D6   87: D#6
    0x739, 0x744, 0x74F, 0x759, 0x762, 0x76B, 0x773, 0x77B,
    # 88: E6   89: F6   90: F#6  91: G6   92: G#6  93: A6   94: A#6  95: B6
    0x783, 0x78A, 0x790, 0x797, 0x79D, 0x7A2, 0x7A7, 0x7AC,
    # 96: C7   97: C#7  98: D7   99: D#7  100: E7  101: F7  102: F#7 103: G7
    0x7B1, 0x7B6, 0x7BA, 0x7BE, 0x7C1, 0x7C4, 0x7C8, 0x7CB,
    # 104: G#7 105: A7  106: A#7 107: B7  108: C8  109: C#8 110: D8  111: D#8
    0x7CE, 0x7D1, 0x7D4, 0x7D6, 0x7D9, 0x7DB, 0x7DD, 0x7DF,
    # 112-119: E8+ (above VB audible range)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    # 120-127: way above range
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
]

PAU = 0x000  # Silence

# ----------------------------------------------------------------
# General MIDI drum note to VB noise channel mapping
#
# Based on VBeat LFSR analysis (by enthusi/PriorArt):
#   Tap sequence lengths: 0=32767 1=1953 2=254 3=217 4=73 5=63 6=42 7=28
#   Shorter = more tonal/pitched.  Longer = more noise-like.
#
# Each entry: (tap, decay, freq)
#   tap:   3-bit LFSR feedback point (0-7), controls timbre
#   decay: 2-bit envelope speed (0=very fast, 1=fast, 2=medium, 3=slow)
#   freq:  10-bit frequency register for noise pitch (0x000-0x3FF)
#
# Packed output value = (tap << 12) | (decay << 10) | freq
# ----------------------------------------------------------------

GM_DRUM_MAP = {
    # Kick drums (MIDI 35-36): tonal tap7 (len=28), low freq, fast decay
    35: (7, 1, 0x020),   # Acoustic Bass Drum -- deep thud
    36: (7, 1, 0x030),   # Bass Drum 1 -- punchy kick

    # Snare drums (MIDI 37-40): noisy tap1 (len=1953), medium freq, fast decay
    37: (2, 0, 0x180),   # Side Stick -- short click (tap2=254, very fast)
    38: (1, 1, 0x100),   # Acoustic Snare -- crunchy crack
    39: (1, 0, 0x0C0),   # Hand Clap -- sharp pop
    40: (1, 1, 0x120),   # Electric Snare -- bright snare

    # Toms (MIDI 41-50): tonal tap6 (len=42) or tap7 (len=28), scaled freq
    41: (6, 1, 0x040),   # Low Floor Tom
    43: (6, 1, 0x060),   # High Floor Tom
    45: (6, 1, 0x080),   # Low Tom
    47: (7, 1, 0x060),   # Low-Mid Tom
    48: (7, 1, 0x080),   # Hi-Mid Tom
    50: (7, 1, 0x0A0),   # High Tom

    # Hi-hat (MIDI 42, 44, 46): white noise tap0 (len=32767), high freq
    42: (0, 0, 0x300),   # Closed Hi-Hat -- very fast decay, crisp
    44: (0, 0, 0x300),   # Pedal Hi-Hat -- same as closed
    46: (0, 2, 0x300),   # Open Hi-Hat -- medium decay, sizzle

    # Cymbals (MIDI 49-59): white noise tap0, high freq, slow decay
    49: (0, 3, 0x380),   # Crash Cymbal 1 -- long wash
    51: (0, 2, 0x340),   # Ride Cymbal 1 -- medium sustain
    52: (0, 3, 0x3A0),   # Chinese Cymbal -- long wash
    55: (0, 2, 0x360),   # Splash Cymbal -- medium
    57: (0, 3, 0x390),   # Crash Cymbal 2 -- long wash
    59: (0, 2, 0x350),   # Ride Cymbal 2 -- medium sustain

    # Miscellaneous
    53: (0, 1, 0x320),   # Ride Bell -- short ping
    54: (2, 0, 0x200),   # Tambourine -- tap2 (len=254), fast jingle
    56: (3, 0, 0x180),   # Cowbell -- tap3 (len=217), short
}

# Default fallback for unmapped GM drum notes
GM_DRUM_DEFAULT = (1, 1, 0x100)  # snare-like


def gm_drum_to_packed(midi_note):
    """Convert a GM drum note number to packed (tap << 12) | (decay << 10) | freq."""
    tap, decay, freq = GM_DRUM_MAP.get(midi_note, GM_DRUM_DEFAULT)
    return (tap << 12) | ((decay & 0x03) << 10) | (freq & 0x3FF)


# ----------------------------------------------------------------
# Utility
# ----------------------------------------------------------------

def build_tempo_map(mid):
    """Build a sorted list of (tick, tempo_us) from all tracks."""
    tempo_map = []
    for track in mid.tracks:
        t = 0
        for msg in track:
            t += msg.time
            if msg.type == 'set_tempo':
                tempo_map.append((t, msg.tempo))
    if not tempo_map:
        tempo_map = [(0, 500000)]  # default 120 BPM only if no set_tempo found
    tempo_map.sort()
    return tempo_map


def get_tempo_at(tempo_map, tick):
    """Return the tempo (in us/beat) active at the given tick."""
    t = 500000
    for tt, tp in tempo_map:
        if tt <= tick:
            t = tp
        else:
            break
    return t


def ticks_to_ms(ticks, tpb, tempo_us):
    """Convert MIDI ticks to milliseconds."""
    if ticks <= 0:
        return 0
    ms = (ticks * tempo_us) / (tpb * 1000)
    return max(1, round(ms))


def note_to_freq(midi_note):
    """Convert MIDI note number to VB frequency register value."""
    if 0 <= midi_note < 128:
        return MIDI_TO_VB_FREQ[midi_note]
    return 0


def velocity_to_4bit(velocity):
    """Convert MIDI velocity (0-127) to 4-bit (1-15). 0 velocity = rest."""
    if velocity <= 0:
        return 0
    # Map 1-127 to 1-15
    return max(1, min(15, (velocity * 15 + 63) // 127))


# ----------------------------------------------------------------
# Note span extraction (monophonic per channel)
# ----------------------------------------------------------------

def extract_note_spans(mid, track_idx, ch_filter, transpose=0):
    """
    Extract note spans from specified track(s) and channels.
    Returns a sorted list of (start_tick, end_tick, midi_note, velocity).
    Monophonic: new note-on cuts any previous note.

    track_idx: int (specific track) or None (scan all tracks).
    ch_filter: set of allowed MIDI channel numbers.
    transpose: semitones to shift notes (e.g., +24 = up 2 octaves).
    """
    if track_idx is not None:
        tracks = [(track_idx, mid.tracks[track_idx])]
    else:
        tracks = list(enumerate(mid.tracks))

    # Collect raw events: (abs_tick, type, midi_note, velocity)
    events = []
    for ti, track in tracks:
        abs_t = 0
        for msg in track:
            abs_t += msg.time
            if not hasattr(msg, 'channel') or msg.channel not in ch_filter:
                continue
            if msg.type == 'note_on' and msg.velocity > 0:
                note = min(127, max(0, msg.note + transpose))
                events.append((abs_t, 1, note, msg.velocity))   # 1 = on
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                note = min(127, max(0, msg.note + transpose))
                events.append((abs_t, 0, note, 0))   # 0 = off

    events.sort(key=lambda e: (e[0], e[1]))  # off before on at same tick

    # Convert to non-overlapping spans (monophonic)
    spans = []
    current_note = -1
    current_vel = 0
    note_start = 0

    for abs_t, etype, midi_note, vel in events:
        if etype == 1:  # note on
            if current_note >= 0 and abs_t > note_start:
                spans.append((note_start, abs_t, current_note, current_vel))
            current_note = midi_note
            current_vel = vel
            note_start = abs_t
        elif etype == 0 and midi_note == current_note:  # note off
            if abs_t > note_start:
                spans.append((note_start, abs_t, current_note, current_vel))
            current_note = -1

    return spans


def extract_drum_spans(mid, track_idx=None):
    """
    Extract drum hits from MIDI channel 9 (zero-indexed).
    Returns a sorted list of (start_tick, end_tick, midi_note, velocity).
    Each hit is treated as a short burst; the end_tick is set to the
    next drum hit or start_tick + reasonable duration.
    """
    drum_ch = {9}  # GM drums are always channel 9 (zero-indexed)

    if track_idx is not None:
        tracks = [(track_idx, mid.tracks[track_idx])]
    else:
        tracks = list(enumerate(mid.tracks))

    # Collect note-on events for drum channel
    hits = []
    for ti, track in tracks:
        abs_t = 0
        for msg in track:
            abs_t += msg.time
            if not hasattr(msg, 'channel') or msg.channel not in drum_ch:
                continue
            if msg.type == 'note_on' and msg.velocity > 0:
                hits.append((abs_t, msg.note, msg.velocity))

    hits.sort(key=lambda h: h[0])

    if not hits:
        return []

    # Convert hits to spans: each hit lasts until the next hit
    spans = []
    for i in range(len(hits)):
        start = hits[i][0]
        note = hits[i][1]
        vel = hits[i][2]
        # End at next hit or start + small offset
        if i + 1 < len(hits):
            end = hits[i + 1][0]
            if end == start:
                end = start + 1
        else:
            end = start + 120  # ~1 beat at 120bpm/480tpb
        spans.append((start, end, note, vel))

    return spans


# ----------------------------------------------------------------
# Drum priority for filtering simultaneous hits
# Lower number = higher priority
# ----------------------------------------------------------------

DRUM_PRIORITY = {}
for _n in (35, 36):                          DRUM_PRIORITY[_n] = 1  # Kick
for _n in (37, 38, 39, 40):                  DRUM_PRIORITY[_n] = 2  # Snare
for _n in (41, 43, 45, 47, 48, 50):          DRUM_PRIORITY[_n] = 3  # Toms
for _n in (53, 54, 56):                       DRUM_PRIORITY[_n] = 4  # Misc perc
for _n in (49, 51, 52, 55, 57, 59):          DRUM_PRIORITY[_n] = 5  # Cymbals
for _n in (42, 44, 46):                       DRUM_PRIORITY[_n] = 6  # Hihat


def extract_drum_spans_prioritized(mid, track_idx=None):
    """
    Extract drum hits with priority filtering.
    When multiple hits occur at the same tick, keep only the highest priority.
    Priority: kick > snare > toms > misc > cymbals > hihat.
    """
    drum_ch = {9}

    if track_idx is not None:
        tracks = [(track_idx, mid.tracks[track_idx])]
    else:
        tracks = list(enumerate(mid.tracks))

    hits = []
    for ti, track in tracks:
        abs_t = 0
        for msg in track:
            abs_t += msg.time
            if not hasattr(msg, 'channel') or msg.channel not in drum_ch:
                continue
            if msg.type == 'note_on' and msg.velocity > 0:
                hits.append((abs_t, msg.note, msg.velocity))

    hits.sort(key=lambda h: h[0])

    if not hits:
        return []

    # Filter simultaneous hits by priority (keep highest priority per tick)
    filtered = []
    i = 0
    while i < len(hits):
        j = i
        while j < len(hits) and hits[j][0] == hits[i][0]:
            j += 1
        group = hits[i:j]
        best = min(group, key=lambda h: DRUM_PRIORITY.get(h[1], 99))
        filtered.append(best)
        i = j
    hits = filtered

    # Convert hits to spans
    spans = []
    for i in range(len(hits)):
        start = hits[i][0]
        note = hits[i][1]
        vel = hits[i][2]
        if i + 1 < len(hits):
            end = hits[i + 1][0]
            if end == start:
                end = start + 1
        else:
            end = start + 120
        spans.append((start, end, note, vel))

    return spans


# ----------------------------------------------------------------
# Tracker-style arpeggio: compact data, runtime cycling
# ----------------------------------------------------------------

def build_arpeggio_for_merge(mid, track_configs):
    """
    Build merged monophonic spans and arpeggio offset map from multiple tracks.
    No time subdivision -- arpeggios are handled at runtime by the VB player.

    track_configs: list of (track_idx, channel_set, transpose)

    Returns: (base_spans, arp_intervals)
      base_spans: sorted list of (start_tick, end_tick, midi_note, velocity)
                  using the lowest active note as the base.
      arp_intervals: sorted list of (start_tick, end_tick, offset1, offset2)
                     semitone offsets for the 2nd and 3rd arp notes (0-15 each).
    """
    # Extract spans from each track independently
    per_track = []
    for track_idx, channels, transpose in track_configs:
        spans = extract_note_spans(mid, track_idx, channels, transpose)
        per_track.append(spans)

    # Collect all tick boundaries from all tracks
    tick_set = set()
    for spans in per_track:
        for start, end, _, _ in spans:
            tick_set.add(start)
            tick_set.add(end)

    if not tick_set:
        return [], []

    sorted_ticks = sorted(tick_set)
    base_spans = []
    arp_intervals = []

    # Index pointers for each track's span list (advance monotonically)
    ptrs = [0] * len(per_track)

    for i in range(len(sorted_ticks) - 1):
        t_start = sorted_ticks[i]
        t_end = sorted_ticks[i + 1]

        # Find active notes from each track at t_start
        active = []
        for ti, spans in enumerate(per_track):
            while ptrs[ti] < len(spans) and spans[ptrs[ti]][1] <= t_start:
                ptrs[ti] += 1
            idx = ptrs[ti]
            if idx < len(spans) and spans[idx][0] <= t_start < spans[idx][1]:
                active.append((spans[idx][2], spans[idx][3]))

        if not active:
            continue  # silence -- merge_channels will fill with 0

        # Sort by pitch ascending for rising arpeggio pattern
        active.sort(key=lambda x: x[0])

        base_note = active[0][0]
        base_vel = active[0][1]
        base_spans.append((t_start, t_end, base_note, base_vel))

        # Compute semitone offsets for 2nd and 3rd notes (clamped to 4-bit 0-15)
        off1 = min(15, active[1][0] - base_note) if len(active) > 1 else 0
        off2 = min(15, active[2][0] - base_note) if len(active) > 2 else 0

        if off1 > 0 or off2 > 0:
            arp_intervals.append((t_start, t_end, off1, off2))

    return base_spans, arp_intervals


# ----------------------------------------------------------------
# Auto-detect channels for type-0 MIDIs
# ----------------------------------------------------------------

def detect_channels_type0(mid):
    """
    For type-0 MIDIs, group notes by MIDI channel and classify by avg pitch.
    Returns (melody_channels, bass_channels, chord_channels) as sets.
    """
    ch_pitches = {}
    for track in mid.tracks:
        abs_t = 0
        for msg in track:
            abs_t += msg.time
            if msg.type == 'note_on' and msg.velocity > 0 and hasattr(msg, 'channel'):
                if msg.channel == 9:  # skip drums
                    continue
                ch_pitches.setdefault(msg.channel, []).append(msg.note)

    if not ch_pitches:
        return set(), set(), set()

    # Sort channels by average pitch (ascending)
    ch_avg = sorted(
        [(ch, sum(p)/len(p), len(p)) for ch, p in ch_pitches.items()],
        key=lambda x: x[1]
    )

    print(f"  Channels by avg pitch: {[(ch, f'{avg:.1f}', cnt) for ch, avg, cnt in ch_avg]}")

    if len(ch_avg) == 1:
        # Single channel: use it for melody, silence bass/chords
        return {ch_avg[0][0]}, set(), set()
    elif len(ch_avg) == 2:
        return {ch_avg[1][0]}, {ch_avg[0][0]}, set()
    else:
        # Lowest = bass, highest = melody, everything in between = chords
        bass_ch = {ch_avg[0][0]}
        melody_ch = {ch_avg[-1][0]}
        chord_chs = {ch for ch, _, _ in ch_avg[1:-1]}
        return melody_ch, bass_ch, chord_chs


# ----------------------------------------------------------------
# Merge 4 channels onto a shared timeline
# ----------------------------------------------------------------

def merge_channels(span_lists, tpb, tempo_map, vel_scales=None,
                   arp_intervals=None, arp_ch=None):
    """
    Merge 4 channels' note spans onto a shared timeline.
    span_lists: [melody_spans, bass_spans, chord_spans, drum_spans]
    vel_scales: optional list of velocity scale factors per tonal channel
                (e.g. [1.0, 1.0, 1.8] to boost chords). Default all 1.0.
    arp_intervals: optional sorted list of (start_tick, end_tick, off1, off2)
                   for tracker-style arpeggio on one tonal channel.
    arp_ch: which channel index (0-2) the arp applies to (None = no arp).

    Returns: (melody[], bass[], chords[], drums[], timing[], arp[])
    All arrays are the same length.
    Consecutive identical entries are merged to save space.

    For melody/bass/chords: packed = (vel4 << 12) | (midi_note << 4)
    For drums: packed = (tap << 12) | freq, or 0 for silence
    arp[]: u8 per step, (off1 << 4) | off2. All zeros if no arpeggio.
    """
    if vel_scales is None:
        vel_scales = [1.0, 1.0, 1.0]
    num_ch = len(span_lists)
    has_arp = arp_intervals is not None and arp_ch is not None and len(arp_intervals) > 0

    # Collect all unique tick boundaries
    tick_set = set()
    for spans in span_lists:
        for start, end, _, _ in spans:
            tick_set.add(start)
            tick_set.add(end)

    if not tick_set:
        return [], [], [], [], [], []

    sorted_ticks = sorted(tick_set)

    # Build raw interval data
    raw_packed = [[] for _ in range(num_ch)]
    raw_timing = []
    raw_arp = []

    # Use index pointers for efficient span lookup (spans are sorted)
    ch_idx = [0] * num_ch
    arp_ptr = 0

    for i in range(len(sorted_ticks) - 1):
        t_start = sorted_ticks[i]
        t_end = sorted_ticks[i + 1]

        dur_ms = ticks_to_ms(t_end - t_start, tpb, get_tempo_at(tempo_map, t_start))
        if dur_ms <= 0:
            continue

        for ch in range(num_ch):
            spans = span_lists[ch]
            # Advance past expired spans
            while ch_idx[ch] < len(spans) and spans[ch_idx[ch]][1] <= t_start:
                ch_idx[ch] += 1

            # Check if current span covers t_start
            idx = ch_idx[ch]
            if idx < len(spans) and spans[idx][0] <= t_start < spans[idx][1]:
                midi_note = spans[idx][2]
                velocity = spans[idx][3]
                if ch < 3:
                    # Tonal: pack velocity + MIDI note number
                    if note_to_freq(midi_note) == 0:
                        raw_packed[ch].append(PAU)
                    else:
                        vel4 = velocity_to_4bit(velocity)
                        vel4 = min(15, max(1, int(vel4 * vel_scales[ch])))
                        raw_packed[ch].append((vel4 << 12) | (midi_note << 4))
                else:
                    # Drums: pack tap + noise freq (unchanged)
                    raw_packed[ch].append(gm_drum_to_packed(midi_note))
            else:
                raw_packed[ch].append(PAU)

        # Build arp byte for this step
        arp_byte = 0
        if has_arp:
            while arp_ptr < len(arp_intervals) and arp_intervals[arp_ptr][1] <= t_start:
                arp_ptr += 1
            if (arp_ptr < len(arp_intervals) and
                    arp_intervals[arp_ptr][0] <= t_start < arp_intervals[arp_ptr][1]):
                off1 = arp_intervals[arp_ptr][2]
                off2 = arp_intervals[arp_ptr][3]
                arp_byte = (off1 << 4) | off2
        raw_arp.append(arp_byte)
        raw_timing.append(dur_ms)

    if not raw_timing:
        return [], [], [], [], [], []

    # Merge consecutive identical entries
    merged_packed = [[raw_packed[ch][0]] for ch in range(num_ch)]
    merged_timing = [raw_timing[0]]
    merged_arp = [raw_arp[0]]

    for i in range(1, len(raw_timing)):
        same = (all(raw_packed[ch][i] == merged_packed[ch][-1] for ch in range(num_ch))
                and raw_arp[i] == merged_arp[-1])
        if same:
            merged_timing[-1] += raw_timing[i]
        else:
            for ch in range(num_ch):
                merged_packed[ch].append(raw_packed[ch][i])
            merged_timing.append(raw_timing[i])
            merged_arp.append(raw_arp[i])

    return (merged_packed[0], merged_packed[1], merged_packed[2],
            merged_packed[3], merged_timing, merged_arp)


# ----------------------------------------------------------------
# Write C header
# ----------------------------------------------------------------

def write_header(prefix, melody, bass, chords, drums, timing, arp, output_path):
    """Write a C header file with 4-channel music data + optional arp array."""
    n = len(timing)
    assert len(melody) == n and len(bass) == n and len(chords) == n and len(drums) == n
    assert arp is None or len(arp) == n

    has_arp = arp is not None and any(a != 0 for a in arp)

    with open(output_path, 'w') as f:
        f.write(f"/* {os.path.basename(output_path)} -- auto-generated by convert_midi.py */\n")
        f.write(f"#ifndef __{prefix.upper()}_H__\n")
        f.write(f"#define __{prefix.upper()}_H__\n\n")
        f.write(f"#define {prefix.upper()}_NOTE_COUNT  {n}\n")
        f.write(f"#define {prefix.upper()}_HAS_ARP     {1 if has_arp else 0}\n\n")

        # Melody (ch0) - (vel << 12) | (midi_note << 4)
        f.write(f"static const int {prefix}_melody[{n}] = {{\n")
        for i in range(0, n, 12):
            chunk = melody[i:i+12]
            f.write("\t" + ",".join(f"0x{v:04X}" for v in chunk))
            f.write(",\n" if i + 12 < n else "\n")
        f.write("};\n\n")

        # Bass (ch1) - (vel << 12) | (midi_note << 4)
        f.write(f"static const int {prefix}_bass[{n}] = {{\n")
        for i in range(0, n, 12):
            chunk = bass[i:i+12]
            f.write("\t" + ",".join(f"0x{v:04X}" for v in chunk))
            f.write(",\n" if i + 12 < n else "\n")
        f.write("};\n\n")

        # Chords (ch3) - (vel << 12) | (midi_note << 4)
        f.write(f"static const int {prefix}_chords[{n}] = {{\n")
        for i in range(0, n, 12):
            chunk = chords[i:i+12]
            f.write("\t" + ",".join(f"0x{v:04X}" for v in chunk))
            f.write(",\n" if i + 12 < n else "\n")
        f.write("};\n\n")

        # Drums (ch5 NOISE) - (tap << 12) | freq
        f.write(f"static const int {prefix}_drums[{n}] = {{\n")
        for i in range(0, n, 12):
            chunk = drums[i:i+12]
            f.write("\t" + ",".join(f"0x{v:04X}" for v in chunk))
            f.write(",\n" if i + 12 < n else "\n")
        f.write("};\n\n")

        # Shared timing (ms)
        f.write(f"static const unsigned short {prefix}_timing[{n}] = {{\n")
        for i in range(0, n, 12):
            chunk = timing[i:i+12]
            f.write("\t" + ",".join(f"{v:5d}" for v in chunk))
            f.write(",\n" if i + 12 < n else "\n")
        f.write("};\n\n")

        # Arpeggio offsets (u8 per step, tracker-style 0xy)
        if has_arp:
            f.write(f"static const unsigned char {prefix}_arp[{n}] = {{\n")
            for i in range(0, n, 16):
                chunk = arp[i:i+16]
                f.write("\t" + ",".join(f"0x{v:02X}" for v in chunk))
                f.write(",\n" if i + 16 < n else "\n")
            f.write("};\n\n")

        f.write("#endif\n")

    data_bytes = n * 4 * 4 + n * 2 + (n if has_arp else 0)
    arp_info = f", arp={sum(1 for a in arp if a != 0)} active" if has_arp else ""
    print(f"  Written: {output_path}  ({n} steps, "
          f"{data_bytes} bytes data{arp_info})")


# ----------------------------------------------------------------
# Song configurations
# ----------------------------------------------------------------

SONGS = [
    # title_screen: type-0 MIDI, channels mapped per user Reaper analysis:
    #   Reaper Track 2 (Ch 1) = main melody (French Horn, pitch 41-62)
    #   Reaper Track 7 (Ch 6) = bassline / contrabass (pitch 29-36, needs +12)
    #   Reaper Tracks 3+4+5 (Ch 2+3+4) = strings (Violin/Viola/Cello) → C64 arpeggio
    {
        'midi': 'title_screen.mid',
        'prefix': 'music_title',
        'header': 'music_title.h',
        'type': 'channels',
        'melody': {'track': None, 'channels': {1}},
        'bass':   {'track': None, 'channels': {6}, 'transpose': 12},
        'chords': {
            'arpeggio': True,
            'tracks': [
                {'track': None, 'channels': {2}},   # Violin (pitch 56-68)
                {'track': None, 'channels': {3}},   # Viola  (pitch 65-72)
                {'track': None, 'channels': {4}},   # Cello  (pitch 59-75)
            ],
            'arp_ticks': 8,  # 8 ticks ≈ 18ms at 140BPM/192TPB → C64-style
        },
        'drums':  'auto',
        'vel_scales': [1.0, 1.0, 2.0],  # boost strings (vel 29-68 are quiet)
        'max_seconds': 120,  # song is ~1:08, no truncation needed
    },
    # e1m1: type-1 -- C64-style arpeggios from tracks 3+4+5 (harmony voices),
    #   Track 1(ch0)=bass riff transposed +24, Track 8(ch7)=late melody,
    #   Track 9(ch9)=drums with kick>snare>hihat priority
    {
        'midi': 'e1m1.mid',
        'prefix': 'music_e1m1',
        'header': 'music_e1m1.h',
        'type': 'tracks',
        'melody': {
            'arpeggio': True,
            'tracks': [
                {'track': 3, 'channels': {2}},
                {'track': 4, 'channels': {3}},
                {'track': 5, 'channels': {4}},
            ],
            'arp_ticks': 4,  # ~21ms at 95 BPM = C64 PAL arpeggio rate
        },
        'bass':   {'track': 1, 'channels': {0}, 'transpose': 24},
        'chords': {'track': 8, 'channels': {7}},
        'drums':  {'track': 9, 'priority': True},
        'vel_scales': [1.0, 1.0, 1.8],  # boost late melody on chords channel
        'max_seconds': 180,
    },
    # e1m2: type-0 -- ch1=melody(46-70), ch3=bass(32-50), ch2=chords(44-62)
    {
        'midi': 'e1m2.mid',
        'prefix': 'music_e1m2',
        'header': 'music_e1m2.h',
        'type': 'channels',
        'melody': {'track': None, 'channels': {1}},
        'bass':   {'track': None, 'channels': {3}},
        'chords': {'track': None, 'channels': {2}},
        'drums':  'auto',
        'vel_scales': [1.0, 1.0, 1.8],  # boost chords to be audible
        'max_seconds': 60,
    },
    # e1m3: type-1 -- Track 2(ch1)=melody(63-74), Track 1(ch0)=bass(29-43),
    #   Track 3(ch2)=chords(59-70)
    {
        'midi': 'e1m3.mid',
        'prefix': 'music_e1m3',
        'header': 'music_e1m3.h',
        'type': 'tracks',
        'melody': {'track': 2, 'channels': {1}},
        'bass':   {'track': 1, 'channels': {0}},
        'chords': {'track': 3, 'channels': {2}},
        'drums':  'auto',
        'vel_scales': [1.0, 1.0, 1.8],  # boost chords to be audible
        'max_seconds': 60,
    },
]


# ----------------------------------------------------------------
# Main
# ----------------------------------------------------------------

def truncate_to_duration(melody, bass, chords, drums, timing, arp, max_ms):
    """Truncate arrays so total duration does not exceed max_ms."""
    total = 0
    for i, dur in enumerate(timing):
        total += dur
        if total >= max_ms:
            cut = i + 1
            return (melody[:cut], bass[:cut], chords[:cut], drums[:cut],
                    timing[:cut], arp[:cut] if arp else None)
    return melody, bass, chords, drums, timing, arp


def convert_song(song_cfg):
    midi_path = os.path.join(MUSIC_DIR, song_cfg['midi'])
    if not os.path.isfile(midi_path):
        print(f"WARNING: {midi_path} not found, skipping")
        return

    print(f"\nConverting {song_cfg['midi']}:")
    mid = mido.MidiFile(midi_path)
    tpb = mid.ticks_per_beat
    tempo_map = build_tempo_map(mid)

    print(f"  Type {mid.type}, {len(mid.tracks)} tracks, tpb={tpb}")

    arp_intervals = None
    arp_ch = None

    if song_cfg['type'] in ('tracks', 'channels'):
        # Explicit track/channel assignments (with optional transpose and arpeggio)
        span_lists = []
        role_idx = 0
        for role in ('melody', 'bass', 'chords'):
            cfg = song_cfg[role]

            if isinstance(cfg, dict) and cfg.get('arpeggio'):
                # Tracker-style arpeggio from multiple tracks (no time subdivision)
                arp_configs = [
                    (tc['track'], tc['channels'], tc.get('transpose', 0))
                    for tc in cfg['tracks']
                ]
                spans, arp_ivs = build_arpeggio_for_merge(mid, arp_configs)
                arp_intervals = arp_ivs
                arp_ch = role_idx
                track_ids = [tc['track'] for tc in cfg['tracks']]
                print(f"  {role}: ARPEGGIO tracks {track_ids}, "
                      f"{len(spans)} base spans, {len(arp_ivs)} arp intervals")
                span_lists.append(spans)
            else:
                track_idx = cfg.get('track')
                channels = cfg['channels']
                transpose = cfg.get('transpose', 0)
                spans = extract_note_spans(mid, track_idx, channels, transpose)
                label = ''
                if track_idx is not None:
                    tn = mid.tracks[track_idx].name if mid.tracks[track_idx].name else 'unnamed'
                    label = f"track {track_idx} ({tn}), "
                xp = f", transpose={transpose:+d}" if transpose else ""
                print(f"  {role}: {label}ch {channels}, {len(spans)} spans{xp}")
                span_lists.append(spans)
            role_idx += 1

    elif song_cfg['type'] == 'auto':
        # Auto-detect channels by pitch
        melody_chs, bass_chs, chord_chs = detect_channels_type0(mid)
        print(f"  Auto-assigned: melody={melody_chs}, bass={bass_chs}, chords={chord_chs}")

        span_lists = []
        for role, chs in [('melody', melody_chs), ('bass', bass_chs), ('chords', chord_chs)]:
            if chs:
                spans = extract_note_spans(mid, None, chs)
            else:
                spans = []
            print(f"  {role}: {len(spans)} note spans")
            span_lists.append(spans)

    # Extract drum track
    drums_cfg = song_cfg.get('drums', 'auto')
    if drums_cfg == 'auto':
        drum_spans = extract_drum_spans(mid)
        print(f"  drums: {len(drum_spans)} hits (channel 9)")
    elif drums_cfg == 'none':
        drum_spans = []
        print(f"  drums: none")
    elif isinstance(drums_cfg, dict):
        track = drums_cfg.get('track')
        if drums_cfg.get('priority', False):
            drum_spans = extract_drum_spans_prioritized(mid, track)
            print(f"  drums: {len(drum_spans)} hits (prioritized: kick>snare>hihat)")
        else:
            drum_spans = extract_drum_spans(mid, track)
            print(f"  drums: {len(drum_spans)} hits (channel 9)")
    else:
        drum_spans = extract_drum_spans(mid)
        print(f"  drums: {len(drum_spans)} hits (channel 9)")
    span_lists.append(drum_spans)

    # Merge onto shared timeline (now 4 channels + arp)
    vel_scales = song_cfg.get('vel_scales', [1.0, 1.0, 1.0])
    melody, bass, chords, drums, timing, arp = merge_channels(
        span_lists, tpb, tempo_map, vel_scales, arp_intervals, arp_ch)

    total_ms = sum(timing) if timing else 0
    print(f"  Merged: {len(timing)} steps, {total_ms/1000:.1f}s total")

    # Truncate if needed
    max_s = song_cfg.get('max_seconds', 120)
    if total_ms > max_s * 1000:
        melody, bass, chords, drums, timing, arp = truncate_to_duration(
            melody, bass, chords, drums, timing, arp, max_s * 1000)
        print(f"  Truncated to {sum(timing)/1000:.1f}s ({len(timing)} steps)")

    if not timing:
        print("  No data extracted, skipping")
        return

    output_path = os.path.join(OUTPUT_DIR, song_cfg['header'])
    write_header(song_cfg['prefix'], melody, bass, chords, drums, timing, arp, output_path)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    for song_cfg in SONGS:
        convert_song(song_cfg)
    print("\nDone.")


if __name__ == "__main__":
    main()
