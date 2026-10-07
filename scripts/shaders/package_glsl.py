#!/usr/bin/env python3
"""Package original GLSL sources and RGBA textures without translating effects.

The JSON manifest owns the pass graph and portable uniform bindings. MIT License,
Copyright (C) 2026 Tomaz Stih. Pillow is used only to decode authoring-time PNGs.
"""
import argparse
import json
from pathlib import Path
from PIL import Image


def quote(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def compile_package(manifest, base):
    lines = ['native-glsl-1 1', f'parameters {len(manifest["parameters"])}']
    for p in manifest['parameters']:
        v = p['default'] if isinstance(p['default'], list) else [p['default']]
        lines.append('parameter {} {} {} {} {}'.format(p['name'], len(v), *p['range'],
                     ' '.join(map(str, v + [0] * (4 - len(v))))))
    lines.append(f'textures {len(manifest.get("textures", []))}')
    for name in manifest.get('textures', []):
        image = Image.open(base / name).convert('RGBA')
        lines.append(f'texture {image.width} {image.height} {quote(image.tobytes().hex())}')
    lines.append(f'passes {len(manifest["passes"])}')
    for p in manifest['passes']:
        lines.append('pass {} {} {} {} {} {} {} {}'.format(p['name'], p.get('extent', 'source'),
                     p.get('reduction', 1), int(p.get('history', False)), p.get('padding', 0),
                     p.get('alignment', 1), p.get('mapping', 0),
                     ' '.join(map(str, p.get('source_rect', [0, 0, 1, 1])))))
        for stage in ['vertex', 'fragment']:
            text = (base / p[stage]).read_text()
            if p.get('defines'):
                header, body = text.split('\n', 1)
                text = header + '\n' + ''.join(f'#define {k} {v}\n' for k, v in p['defines'].items()) + body
            lines.append(f'{stage} {quote(text)}')
        lines.append(f'uniforms {len(p.get("uniforms", {}))}')
        for name, value in p.get('uniforms', {}).items():
            if isinstance(value, str):
                binding, v = value, [0, 0, 0, 0]
                count = next((len(x['default']) if isinstance(x['default'], list) else 1 for x in manifest['parameters'] if x['name'] == value),
                             2 if value in ('source_size', 'viewport_size') else 1)
            elif isinstance(value, dict):
                binding, v, count = value['binding'], value['value'], value.get('components', 1)
            else:
                binding = 'value'; v = value if isinstance(value, list) else [value]; count = len(v)
            lines.append(f'uniform {name} {binding} {count} ' + ' '.join(map(str, v + [0] * (4 - len(v)))))
        lines.append(f'samplers {len(p.get("samplers", {}))}')
        for name, pair in p.get('samplers', {}).items():
            value = pair if isinstance(pair, list) else [pair, 0]
            lines.append(f'sampler {name} {value[0]} {value[1]}')
    return '\n'.join(lines + ['end', ''])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_text(compile_package(json.loads(args.manifest.read_text()), args.manifest.parent))
