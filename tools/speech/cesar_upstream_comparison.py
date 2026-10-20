#!/usr/bin/env python3
"""Reconstruct the original speech.h engine at its native rate, outside the app.

The archived Swedish bridge remains ours, not an upstream Swedish voice. Its
timing and melody differ from Cesar. eSpeak is an external research executable;
no GPL code or voice tables are imported into the product.
"""
import argparse
import array
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import wave

from cesar_source_comparison import measure

REVISION = 'eda1b01'
HEADER_SHA256 = '9368a13b44b50bbcfd5ec10e82bf0472fafecc74f6c07740abfa702d7bd3f726'
HARNESS = r'''
#include "speech/formant_voice.hpp"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 3;
    std::string ipa((std::istreambuf_iterator<char>(input)), {});
    while (!ipa.empty() && (ipa.back() == '\n' || ipa.back() == '\r')) ipa.pop_back();
    for (const auto& phone : ortho::formant_unknown_phones(ipa)) {
        std::cerr << "Unsupported phone: " << phone << '\n';
        return 4;
    }
    const auto samples = ortho::formant_speech(ipa);
    std::ofstream output(argv[2], std::ios::binary);
    const auto put = [&](uint32_t v, int n) {
        for (int i = 0; i < n; ++i) output.put(char((v >> (8*i)) & 255));
    };
    output.write("RIFF", 4); put(36 + samples.size()*2, 4);
    output.write("WAVEfmt ", 8); put(16, 4); put(1, 2); put(1, 2);
    put(11025, 4); put(22050, 4); put(2, 2); put(16, 2);
    output.write("data", 4); put(samples.size()*2, 4);
    for (float s : samples) put(uint16_t(int16_t(std::lround(s*32768))), 2);
    return output ? 0 : 5;
}
'''


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def pcm(path):
    with wave.open(str(path), 'rb') as source:
        if source.getsampwidth() != 2 or source.getnchannels() != 1:
            raise ValueError('Expected mono PCM16')
        rate = source.getframerate()
        samples = array.array('h', source.readframes(source.getnframes()))
    if sys.byteorder != 'little':
        samples.byteswap()
    return rate, samples


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=root / 'build/cesar-upstream-comparison')
    parser.add_argument('--probe', type=Path, default=root / 'build/cmake/bin/ortho-cesar-audio-probe')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--espeak', default='espeak-ng')
    args = parser.parse_args()
    folder = args.output.resolve()
    source = folder / 'source'
    (source / 'speech').mkdir(parents=True, exist_ok=True)
    header = subprocess.check_output(['git', '-C', str(root), 'show',
                                      f'{REVISION}:third_party/speech/speech.h'])
    if hashlib.sha256(header).hexdigest() != HEADER_SHA256:
        raise ValueError('Archived upstream header changed')
    (source / 'speech.h').write_bytes(header)
    for name in ['formant_voice.cpp', 'formant_voice.hpp']:
        text = run('git', '-C', str(root), 'show', f'{REVISION}:src/speech/{name}')
        if name.endswith('.cpp'):
            if text.count('constexpr int sample_rate = 24000;') != 1:
                raise ValueError('Unexpected archived Swedish bridge')
            text = text.replace('constexpr int sample_rate = 24000;',
                                'constexpr int sample_rate = 11025;')
        (source / 'speech' / name).write_text(text)
    (source / 'main.cpp').write_text(HARNESS)
    executable = folder / 'upstream-probe'
    run('c++', '-std=c++23', '-O2', '-Wno-unknown-pragmas', '-I', str(source),
        str(source / 'speech/formant_voice.cpp'), str(source / 'main.cpp'), '-o', str(executable))
    suite_path = Path(__file__).parent / 'fixtures/compact-swedish.json'
    suite = json.loads(suite_path.read_text())
    report = {'archived_revision': REVISION, 'speech_h_sha256': HEADER_SHA256,
              'verified_upstream_revision': 'bc03ac2268db88d1fadfbe4cee5ec1d55f715edc',
              'suite_sha256': hashlib.sha256(suite_path.read_bytes()).hexdigest(),
              'espeak_version': run(args.espeak, '--version').strip(),
              'frontend': {'cesar': 'frozen project IPA, compact melody',
                           'upstream': 'same IPA, archived project Swedish bridge, upstream engine at 11025 Hz',
                           'espeak': 'text, upstream Swedish frontend, default backend',
                           'espeak-klatt': 'text, same upstream Swedish frontend, Klatt backend'},
              'limitations': ['Not a controlled source-only experiment: timing and melody differ.',
                              'Upstream speech.h has no Swedish voice in this experiment.',
                              'eSpeak references do not use our phonetic data.',
                              'No time stretch, resampling, EQ, compression or limiting.'],
              'default_app_voice_changed': False, 'listening_status': 'unreviewed', 'cases': {}}
    for case in suite['cases']:
        if case['id'] not in ['psalm', 'prose', 'quantity', 'swedish-sounds']:
            continue
        case_dir = folder / case['id']
        case_dir.mkdir(exist_ok=True)
        ipa = case_dir / 'input.ipa.txt'
        text = case_dir / 'input.txt'
        ipa.write_text(case['ipa'])
        text.write_text(case['text'])
        run(str(executable), str(ipa), str(case_dir / 'upstream.raw.wav'))
        run(str(args.probe), '-', str(ipa), str(case_dir / 'cesar.raw.wav'), '--ipa', '--compact')
        for variant, voice in [('espeak', 'sv'), ('espeak-klatt', 'sv+klatt')]:
            run(args.espeak, '-D', '-v', voice, '-s', '175', '-w',
                str(case_dir / f'{variant}.raw.wav'), '-f', str(text))
        variants = {}
        for variant in report['frontend']:
            path = case_dir / f'{variant}.raw.wav'
            rate, samples = pcm(path)
            expected_rate = 11025 if variant == 'upstream' else 24000 if variant == 'cesar' else 22050
            if rate != expected_rate or not samples:
                raise ValueError(f'{variant}: unexpected native rate or empty audio')
            level = measure(args.ffmpeg, path)
            gain = 10 ** ((-21 - level) / 20)
            variants[variant] = {'sample_rate': rate, 'samples': len(samples),
                                 'seconds': len(samples) / rate, 'raw_lufs': level,
                                 'raw_rail_samples': sum(abs(s) >= 32767 for s in samples),
                                 'raw_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                                 'gain': gain, 'peak': max(map(abs, samples)) / 32768}
        headroom = max(1, *(v['gain'] * v['peak'] / .95 for v in variants.values()))
        for variant, values in variants.items():
            output = case_dir / f'{variant}.wav'
            gain = values['gain'] / headroom
            run(args.ffmpeg, '-y', '-hide_banner', '-loglevel', 'error', '-i',
                str(case_dir / f'{variant}.raw.wav'), '-af', f'volume={gain:.12g}',
                '-c:a', 'pcm_s16le', str(output))
            values['listening_lufs'] = measure(args.ffmpeg, output)
            values['listening_sha256'] = hashlib.sha256(output.read_bytes()).hexdigest()
        levels = [v['listening_lufs'] for v in variants.values()]
        if max(levels) - min(levels) > .2:
            raise ValueError('Listening copies are not level matched')
        report['cases'][case['id']] = {'text': case['text'], 'ipa': case['ipa'], 'variants': variants}
    (folder / 'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(f'Rendered {len(report["cases"])} cases × 4 variants in {folder}')


if __name__ == '__main__':
    main()
