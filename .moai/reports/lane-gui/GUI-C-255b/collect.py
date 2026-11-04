from pathlib import Path
import hashlib, json, re
from PIL import Image, ImageChops

root = Path('D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-255b/run-20261023-172433')
selected = []
for directory in sorted(root.glob('*-uia3-*')):
    log = (directory / 'flow.log').read_text(encoding='utf-8-sig')
    if log.count('NATIVE_DIALOG_NONWHITE_RATIO=') == 5 and 'CLI_COMPARISON_WRITTEN' in log:
        selected.append((directory, log))

def make_stats():
    flows, image_rows, quality_rows = [], [], []
    for directory, log in selected:
        compare = json.loads((directory / 'bit-comparison.json').read_text(encoding='utf-8-sig'))
        dpi = sorted(set(re.findall(r'dpi=(\d+)', log)))
        images = list(directory.glob('*.png'))
        quality = [json.loads(p.read_text(encoding='utf-8-sig')) for p in directory.glob('*.quality.json')]
        quality_rows.extend(quality)
        for path in images:
            image = Image.open(path).convert('RGBA')
            opaque = Image.new('RGBA', image.size, (255, 255, 255, 255))
            opaque.alpha_composite(image)
            image = opaque.convert('RGB')
            if not path.name.endswith('.wpf-client.png'):
                top = min(55, image.height // 4)
                image = image.crop((12, top, image.width - 12, image.height - 12))
            red, green, blue = image.split()
            histogram = ImageChops.darker(ImageChops.darker(red, green), blue).histogram()
            ratio = sum(histogram[:246]) / sum(histogram)
            image_rows.append(dict(path=path.relative_to(root).as_posix(), nonWhiteRatio=ratio, exceeds5Percent=ratio > .05))
        flows.append(dict(directory=directory.name, dpi=dpi, pngCount=len(images), qualityCount=len(quality),
                          guiCliEqual=compare['equal'], bytes=compare['guiBytes'], sha256=compare['guiSha256'],
                          ownPid=int(re.search(r'LAUNCH pid=(\d+)', log).group(1)),
                          remaining0='OWNED_PROCESS_REMAINING=0' in log, exit0='EXIT=0' in log))
    return dict(flows=flows, pngCount=len(image_rows), qualityCount=len(quality_rows),
                qualityFailures=sum(not q['accepted'] for q in quality_rows),
                imageThresholdFailures=sum(not image['exceeds5Percent'] for image in image_rows),
                minNonWhiteRatio=min(image['nonWhiteRatio'] for image in image_rows), images=image_rows)

def manifest():
    lines = []
    for directory, _ in selected:
        for path in sorted(directory.iterdir()):
            if path.is_file():
                lines.append(path.relative_to(root).as_posix() + '\t' + hashlib.sha256(path.read_bytes()).hexdigest() + '\t' + str(path.stat().st_size))
    return lines

if MODE == 'stats':
    print('DATA_START')
    print(json.dumps(make_stats(), ensure_ascii=False))
    print('DATA_END')
elif MODE == 'manifest':
    lines = manifest()
    print('DATA_START')
    print(json.dumps(dict(total=len(lines), offset=OFFSET, lines=lines[OFFSET:OFFSET + 80]), ensure_ascii=False))
    print('DATA_END')
