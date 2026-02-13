"""
prepare_doom_sfx.py

Convert original Doom WAV files (11025 Hz, 8-bit, mono) to packed 4-bit
nibble arrays for PCM playback on the Virtual Boy.

Playback method (from DogP's wavonvb):
  - Fill waveform RAM with DC constant (0x3F)
  - Modulate the SxLRV (volume) register per sample as a 4-bit DAC
  - Each sample is 0-15, packed two per byte (high nibble first)

Pipeline:
  1. Read each WAV (expected: 11025 Hz, 8-bit unsigned, mono)
  2. Downsample to ~5000 Hz with proper anti-aliasing (scipy resample_poly)
  3. Convert to 4-bit unsigned (0-15) with rounding
  4. Trim trailing silence
  5. Pad to even sample count (2 samples per byte)
  6. Pack as nibble pairs: high nibble = even sample, low nibble = odd sample
  7. Save downsampled WAV to doom_sfx_downsampled/ for auditing
  8. Output C arrays in src/vbdoom/assets/audio/doom_sfx.c and doom_sfx.h
"""

import os
import wave
import struct
from math import gcd

import numpy as np
from scipy.signal import resample_poly

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SFX_INPUT_DIR = os.path.join(SCRIPT_DIR, "doom_original_sfx")
SFX_OUTPUT_DIR = os.path.join(SCRIPT_DIR, "doom_sfx_downsampled")
C_OUTPUT_DIR = os.path.join(SCRIPT_DIR, "src", "vbdoom", "assets", "audio")

TARGET_RATE = 5000       # 5 kHz: correct pitch on real hardware (timer often ~5-8 kHz effective)
SILENCE_THRESHOLD = 1    # 4-bit values 7-8 are center; threshold from center
CENTER_4BIT = 8          # center of 4-bit range (0-15)

# Sound definitions: (filename, C identifier suffix)
PLAYER_SOUNDS = [
    ("punch.wav",         "punch"),
    ("pistol.wav",        "pistol"),
    ("shotgun.wav",       "shotgun"),
    ("shotgun cock.wav",  "shotgun_cock"),
    ("item up.wav",       "item_up"),
    ("player umf.wav",    "player_umf"),
    ("player pain.wav",   "player_pain"),
    ("player death.wav",  "player_death"),
]

ENEMY_SOUNDS = [
    ("possessed sight 1.wav",    "possessed_sight1"),
    # possessed_sight2/3 removed (ROM savings)
    ("possessed death 1.wav",    "possessed_death1"),
    # possessed_death2/3 removed (ROM savings)
    ("possessed pain.wav",       "possessed_pain"),
    ("possessed activity.wav",   "possessed_activity"),
    ("claw attack.wav",          "claw_attack"),
    ("projectile.wav",           "projectile"),
    ("projectile contact.wav",   "projectile_contact"),
    ("imp sight 1.wav",          "imp_sight1"),
    # imp_sight2 removed (ROM savings)
    ("imp death 1.wav",          "imp_death1"),
    # imp_death2 removed (ROM savings)
    ("imp activity.wav",         "imp_activity"),
    ("pinky attack.wav",         "pinky_attack"),
    ("pinky death.wav",          "pinky_death"),
    ("pinky sight.wav",          "pinky_sight"),
]

WORLD_SOUNDS = [
    ("door open.wav",            "door_open"),
    ("door close.wav",           "door_close"),
    ("switch on.wav",            "switch_on"),
    ("rocket launch.wav",        "rocket_launch"),
    ("barrel explode.wav",       "barrel_explode"),
    ("elevator_stp.wav",         "elevator_stp"),
    ("stone move.wav",           "stone_move"),
    ("TELEPORT.wav",             "teleport"),
    ("drum_kick.wav",            "drum_kick"),
    ("drum_snare.wav",           "drum_snare"),
    ("drum_hihat.wav",           "drum_hihat"),
    ("drum_crash.wav",           "drum_crash"),
    ("drum_tom_low.wav",         "drum_tom_low"),
    ("drum_tom_bright.wav",      "drum_tom_bright"),
    ("drum_clap.wav",            "drum_clap"),
    ("drum_snare_sidehit.wav",   "drum_snare_sidehit"),
    ("drum_snare2.wav",          "drum_snare2"),
    ("drum_conga.wav",           "drum_conga"),
    ("drum_timpani.wav",         "drum_timpani"),
]

ALL_SOUNDS = PLAYER_SOUNDS + ENEMY_SOUNDS + WORLD_SOUNDS


def read_wav(filepath):
    """Read a WAV file and return sample rate + raw unsigned 8-bit samples.
    Supports mono or stereo (mixes to mono), 8-bit, 16-bit, or 32-bit float."""
    try:
        with wave.open(filepath, 'r') as w:
            nch = w.getnchannels()
            sw = w.getsampwidth()
            rate = w.getframerate()
            n_frames = w.getnframes()
            raw = w.readframes(n_frames)

            if sw == 1:
                # 8-bit unsigned: 0-255, 128=silence
                all_samples = list(raw)
            elif sw == 2:
                # 16-bit signed: convert to 8-bit unsigned
                all_samples = []
                for i in range(0, len(raw), 2):
                    s16 = struct.unpack_from('<h', raw, i)[0]
                    all_samples.append((s16 + 32768) >> 8)
            else:
                raise ValueError(f"Unsupported sample width: {sw*8}-bit")

            # Mix stereo to mono
            if nch == 2:
                mono = []
                for i in range(0, len(all_samples), 2):
                    if i + 1 < len(all_samples):
                        mono.append((all_samples[i] + all_samples[i + 1]) >> 1)
                    else:
                        mono.append(all_samples[i])
                all_samples = mono
            elif nch != 1:
                raise ValueError(f"Unsupported channel count: {nch}")

            return rate, all_samples
    except wave.Error:
        # Fallback for IEEE float WAVs (format 3) using scipy
        from scipy.io import wavfile as sciwav
        rate, data = sciwav.read(filepath)
        # scipy returns float32/float64 for float WAVs, int16/int32 for PCM
        arr = np.array(data, dtype=np.float64)
        if arr.ndim == 2:
            arr = arr.mean(axis=1)  # mix stereo to mono
        # Normalize float range (-1..1 or int range) to 0-255 unsigned 8-bit
        if np.issubdtype(data.dtype, np.floating):
            arr = np.clip(arr, -1.0, 1.0)
            all_samples = [max(0, min(255, int(round((s + 1.0) * 127.5)))) for s in arr]
        else:
            # Integer PCM: scale to 0-255
            info = np.iinfo(data.dtype)
            all_samples = [max(0, min(255, int(round((s - info.min) / (info.max - info.min) * 255)))) for s in arr]
        return rate, all_samples


def downsample(samples, src_rate, dst_rate):
    """Resample with proper anti-aliasing filter (scipy resample_poly).

    resample_poly applies a Kaiser-window FIR low-pass filter before
    decimation, correctly suppressing frequencies above the new Nyquist
    and eliminating the aliasing artifacts that made sounds brighter."""
    g = gcd(int(dst_rate), int(src_rate))
    up = int(dst_rate) // g
    down = int(src_rate) // g
    arr = np.array(samples, dtype=np.float64)
    resampled = resample_poly(arr, up, down)
    # Clip to valid 8-bit unsigned range and round
    return [max(0, min(255, int(round(s)))) for s in resampled]


def to_4bit(samples_8bit):
    """Convert 8-bit unsigned (0-255) to 4-bit unsigned (0-15).
    Uses rounding instead of truncation to reduce quantization bias."""
    return [min(15, (s + 8) >> 4) for s in samples_8bit]


def trim_trailing_silence(samples_4bit):
    """Remove trailing near-center samples (4-bit, center=8)."""
    end = len(samples_4bit)
    while end > 0 and abs(samples_4bit[end - 1] - CENTER_4BIT) <= SILENCE_THRESHOLD:
        end -= 1
    if end < 2:
        end = min(2, len(samples_4bit))
    return samples_4bit[:end]


def pad_to_even(samples):
    """Pad to even count (2 samples per packed byte)."""
    if len(samples) & 1:
        samples = samples + [CENTER_4BIT]  # pad with silence (center)
    return samples


def pack_nibbles(samples_4bit):
    """Pack pairs of 4-bit samples into bytes (high nibble first)."""
    packed = []
    for i in range(0, len(samples_4bit), 2):
        hi = samples_4bit[i] & 0x0F
        lo = samples_4bit[i + 1] & 0x0F
        packed.append((hi << 4) | lo)
    return packed


def save_converted_wav(filepath, rate, samples_4bit):
    """Save 4-bit samples back as 8-bit WAV for auditing."""
    with wave.open(filepath, 'w') as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(rate)
        data = bytes(min(255, s << 4) for s in samples_4bit)
        w.writeframes(data)


def generate_c_array(name, packed_bytes):
    """Generate a C const u8 array string from packed nibble data."""
    lines = []
    lines.append(f"const u8 sfx_{name}[] __attribute__((aligned(4))) = {{")
    for i in range(0, len(packed_bytes), 16):
        chunk = packed_bytes[i:i+16]
        vals = ", ".join(f"0x{v:02X}" for v in chunk)
        comma = "," if i + 16 < len(packed_bytes) else ""
        lines.append(f"\t{vals}{comma}")
    lines.append("};")
    return "\n".join(lines)


def main():
    os.makedirs(SFX_OUTPUT_DIR, exist_ok=True)
    os.makedirs(C_OUTPUT_DIR, exist_ok=True)

    sound_data = []  # (name, packed_bytes, num_samples, duration)

    print("=== Doom SFX Converter for Virtual Boy (4-bit SxLRV method) ===\n")

    for filename, cname in ALL_SOUNDS:
        filepath = os.path.join(SFX_INPUT_DIR, filename)
        if not os.path.exists(filepath):
            print(f"WARNING: {filename} not found, skipping")
            continue

        rate, raw_samples = read_wav(filepath)
        original_len = len(raw_samples)

        # Downsample to target rate
        downsampled = downsample(raw_samples, rate, TARGET_RATE)

        # Convert to 4-bit
        samples_4bit = to_4bit(downsampled)

        # Trim trailing silence
        trimmed = trim_trailing_silence(samples_4bit)

        # Pad to even
        padded = pad_to_even(trimmed)

        num_samples = len(padded)
        packed = pack_nibbles(padded)
        duration = num_samples / TARGET_RATE

        print(f"{filename}:")
        print(f"  Original: {original_len} samples @ {rate}Hz ({original_len/rate:.2f}s)")
        print(f"  Downsampled: {len(downsampled)} samples @ {TARGET_RATE}Hz")
        print(f"  Trimmed+padded: {num_samples} samples ({duration:.2f}s)")
        print(f"  Packed: {len(packed)} bytes")
        print()

        # Save converted WAV for auditing
        out_wav = os.path.join(SFX_OUTPUT_DIR, filename)
        save_converted_wav(out_wav, TARGET_RATE, padded)

        sound_data.append((cname, packed, num_samples, duration))

    # Generate C source file
    total_bytes = 0
    c_lines = []
    c_lines.append("/*")
    c_lines.append(" * doom_sfx.c -- PCM sound effect data for VB Doom")
    c_lines.append(" *")
    c_lines.append(" * Auto-generated by prepare_doom_sfx.py. Do not edit.")
    c_lines.append(" *")
    c_lines.append(" * Each array contains packed 4-bit unsigned samples (0-15)")
    c_lines.append(" * in nibble pairs (high nibble first) for SxLRV volume playback.")
    c_lines.append(" */")
    c_lines.append("")
    c_lines.append('#include "doom_sfx.h"')
    c_lines.append("")

    for cname, packed, num_samples, duration in sound_data:
        c_lines.append(generate_c_array(cname, packed))
        c_lines.append("")
        total_bytes += len(packed)

    # Generate sound table
    c_lines.append("/* Sound lookup table */")
    c_lines.append("const SFXEntry sfx_table[] = {")
    for cname, packed, num_samples, duration in sound_data:
        c_lines.append(f"\t{{ sfx_{cname}, {num_samples} }},  /* SFX_{cname.upper()} */")
    c_lines.append("};")
    c_lines.append("")

    c_path = os.path.join(C_OUTPUT_DIR, "doom_sfx.c")
    with open(c_path, 'w') as f:
        f.write("\n".join(c_lines))
    print(f"Written: {c_path}")

    # Generate C header file
    h_lines = []
    h_lines.append("#ifndef _DOOM_SFX_H_")
    h_lines.append("#define _DOOM_SFX_H_")
    h_lines.append("")
    h_lines.append("#include <types.h>")
    h_lines.append("")
    h_lines.append("/* Sound effect IDs */")
    h_lines.append("enum {")
    for i, (cname, packed, num_samples, duration) in enumerate(sound_data):
        h_lines.append(f"\tSFX_{cname.upper()} = {i},")
    h_lines.append(f"\tSFX_COUNT = {len(sound_data)}")
    h_lines.append("};")
    h_lines.append("")
    h_lines.append("/* Sound entry: pointer to packed 4-bit data + sample count */")
    h_lines.append("typedef struct {")
    h_lines.append("\tconst u8 *data;    /* packed nibble pairs */")
    h_lines.append("\tu16 length;        /* total samples (not bytes) */")
    h_lines.append("} SFXEntry;")
    h_lines.append("")
    h_lines.append("/* Lookup table */")
    h_lines.append("extern const SFXEntry sfx_table[];")
    h_lines.append("")

    # Extern declarations for individual arrays
    h_lines.append("/* Individual sample arrays */")
    for cname, packed, num_samples, duration in sound_data:
        h_lines.append(f"extern const u8 sfx_{cname}[];")
    h_lines.append("")
    h_lines.append("#endif")
    h_lines.append("")

    h_path = os.path.join(C_OUTPUT_DIR, "doom_sfx.h")
    with open(h_path, 'w') as f:
        f.write("\n".join(h_lines))
    print(f"Written: {h_path}")

    print(f"\nTotal packed data: {total_bytes} bytes ({total_bytes/1024:.1f} KB)")
    print(f"Sounds converted: {len(sound_data)}/{len(ALL_SOUNDS)}")


if __name__ == "__main__":
    main()
