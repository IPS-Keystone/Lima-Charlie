#!/usr/bin/env python3
"""Builds the "vanilla" beep set from the game's own roger beep.

Arma Reforger plays a two-part roger beep at the end of a radio transmission: Sounds/VON/von.acp wires
SOUND_ROGER_BEEP to two sample banks, "Roger Beep A" at -23 dB and "Roger Beep B" at -25 dB delayed 150 ms.
This reads those samples straight out of the installed game's paks, mixes each pair the way von.acp does and
writes 16-bit 48 kHz WAVs the plugin can load.

The samples are Bohemia Interactive's, so the generated folder is the game's audio, not ours. Run this
against your own install; think before redistributing what comes out.

    python make_vanilla_beeps.py [path\\to\\Arma Reforger] [output dir]

Defaults: the usual Steam install, and ..\\sounds\\vanilla next to this script.
"""
import glob
import os
import struct
import sys
import zlib

RATE = 48000
#! von.acp: bank A -23 dB, bank B -25 dB, B delayed by its "Silence 150" property
GAIN_A = 10.0 ** (-23.0 / 20.0)
GAIN_B = 10.0 ** (-25.0 / 20.0)
DELAY_B_MS = 150
#! Peak the finished beep is normalised to, in line with the TFAR sets already shipped (24-34% of full
#! scale). These are mixed on top of speech, so they are deliberately nowhere near full scale.
TARGET_PEAK = 0.32
#! A beep that stops mid-waveform clicks, so the tail is faded whatever the source did
FADE_MS = 3

DEFAULT_GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger"
#! Which A/B pair each of our sound names uses, so your own transmission ending and someone else's are
#! distinguishable. Vanilla picks one of the three at random; any of them is the vanilla beep.
WANTED = {"local_end": ("Radio_RogerBeep_A_1.wav", "Radio_RogerBeep_B_1.wav"),
          "remote_end": ("Radio_RogerBeep_A_2.wav", "Radio_RogerBeep_B_2.wav")}


def pak_entries(path):
    """[(name, (offset, compressedSize, size, ...))] for one Enfusion .pak (FORM/PAC1)."""
    handle = open(path, "rb")
    header = handle.read(12)
    if header[:4] != b"FORM" or header[8:12] != b"PAC1":
        return handle, []

    end, position, index = 8 + struct.unpack(">I", header[4:8])[0], 12, None
    while position < end:
        handle.seek(position)
        tag = handle.read(4)
        size = struct.unpack(">I", handle.read(4))[0]
        if tag == b"FILE":
            index = handle.read(size)
        position += 8 + size
    if index is None:
        return handle, []

    entries, cursor = [], [0]

    def walk(prefix):
        kind = index[cursor[0]]
        length = index[cursor[0] + 1]
        name = index[cursor[0] + 2:cursor[0] + 2 + length].decode("utf-8", "replace")
        cursor[0] += 2 + length
        if kind == 0:
            count = struct.unpack("<I", index[cursor[0]:cursor[0] + 4])[0]
            cursor[0] += 4
            for _ in range(count):
                walk(prefix + name + "/" if name else prefix)
        else:
            entries.append((prefix + name, struct.unpack("<6I", index[cursor[0]:cursor[0] + 24])))
            cursor[0] += 24

    walk("")
    return handle, entries


def pak_read(handle, meta):
    offset, packed, size = meta[0], meta[1], meta[2]
    handle.seek(offset)
    raw = handle.read(packed)
    if packed == size:
        return raw
    for window in (15, -15, 47):
        try:
            return zlib.decompress(raw, window)
        except zlib.error:
            pass
    return None


def find_samples(gameDir):
    """{filename: bytes} for every roger beep sample in the install."""
    found = {}
    for pak in sorted(glob.glob(os.path.join(gameDir, "addons", "data", "data*.pak"))):
        handle, entries = pak_entries(pak)
        for name, meta in entries:
            if "RogerBeep/" in name and name.endswith(".wav"):
                data = pak_read(handle, meta)
                if data:
                    found[name.split("/")[-1]] = data
        handle.close()
    return found


def decode_wav(data):
    """(frames as floats -1..1, rate). 8-bit unsigned or 16-bit signed PCM, mono or stereo."""
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a RIFF WAVE")

    channels = bits = 0
    rate, pcm, offset = 0, None, 12
    while offset + 8 <= len(data):
        tag = data[offset:offset + 4]
        size = struct.unpack("<I", data[offset + 4:offset + 8])[0]
        body = data[offset + 8:offset + 8 + size]
        if tag == b"fmt ":
            fmt, channels, rate = struct.unpack("<HHI", body[:8])
            bits = struct.unpack("<H", body[14:16])[0]
            if fmt != 1:
                raise ValueError("compressed WAV (format %d)" % fmt)
        elif tag == b"data":
            pcm = body
        offset += 8 + size + (size & 1)
    if pcm is None or channels < 1:
        raise ValueError("no data chunk")

    if bits == 8:
        samples = [(value - 128) / 128.0 for value in pcm]
    elif bits == 16:
        samples = [value / 32768.0 for value in struct.unpack("<%dh" % (len(pcm) // 2), pcm[:len(pcm) // 2 * 2])]
    else:
        raise ValueError("%d-bit WAV" % bits)

    if channels == 1:
        return samples, rate
    return [sum(samples[i:i + channels]) / channels for i in range(0, len(samples) - channels + 1, channels)], rate


def resample(samples, rate):
    if rate == RATE or not samples:
        return samples
    count = int(len(samples) * RATE / rate)
    out = []
    for i in range(count):
        position = i * rate / float(RATE)
        low = int(position)
        high = min(low + 1, len(samples) - 1)
        fraction = position - low
        out.append(samples[low] + (samples[high] - samples[low]) * fraction)
    return out


def mix(a, b):
    """Bank A, plus bank B delayed, at von.acp's relative levels, normalised and faded out."""
    delay = DELAY_B_MS * RATE // 1000
    out = [0.0] * max(len(a), delay + len(b))
    for i, value in enumerate(a):
        out[i] += value * GAIN_A
    for i, value in enumerate(b):
        out[delay + i] += value * GAIN_B

    peak = max(abs(value) for value in out) if out else 0.0
    if peak > 0.0:
        scale = TARGET_PEAK / peak
        out = [value * scale for value in out]

    fade = min(FADE_MS * RATE // 1000, len(out))
    for i in range(fade):
        out[len(out) - fade + i] *= 1.0 - (i + 1) / float(fade)
    return out


def write_wav(path, samples):
    pcm = b"".join(struct.pack("<h", max(-32768, min(32767, int(round(value * 32767.0))))) for value in samples)
    with open(path, "wb") as handle:
        handle.write(b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE")
        handle.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        handle.write(b"data" + struct.pack("<I", len(pcm)) + pcm)


def main():
    gameDir = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_GAME
    outDir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sounds", "vanilla")

    if not os.path.isdir(os.path.join(gameDir, "addons", "data")):
        sys.exit("No addons\\data under %s. Pass the game directory as the first argument." % gameDir)

    samples = find_samples(gameDir)
    if not samples:
        sys.exit("No RogerBeep samples in that install. A game update may have moved them.")
    print("Found %d roger beep samples" % len(samples))

    outDir = os.path.normpath(outDir)
    if not os.path.isdir(outDir):
        os.makedirs(outDir)

    for name, (fileA, fileB) in sorted(WANTED.items()):
        if fileA not in samples or fileB not in samples:
            sys.exit("Missing %s or %s" % (fileA, fileB))
        a, rateA = decode_wav(samples[fileA])
        b, rateB = decode_wav(samples[fileB])
        mixed = mix(resample(a, rateA), resample(b, rateB))
        path = os.path.join(outDir, name + ".wav")
        write_wav(path, mixed)
        print("%-28s %.3fs  from %s + %s" % (os.path.basename(path), len(mixed) / float(RATE), fileA, fileB))

    print("\nWrote %s" % outDir)
    print("These are the game's own samples. Check your own position on redistributing them before you")
    print("publish a plugin package that contains this folder.")


if __name__ == "__main__":
    main()
