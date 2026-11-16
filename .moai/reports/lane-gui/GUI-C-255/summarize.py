import pathlib, json, hashlib, re
from PIL import Image, ImageChops

base = pathlib.Path('D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-255')
run = pathlib.Path((base / 'current-run.txt').read_text(encoding='utf-8-sig').strip())
rows, records, hashes, flows = [], [], [], []
for directory in sorted(run.glob('*-uia3-*')):
    log = (directory / 'flow.log').read_text(encoding='utf-8-sig')
    dpis = sorted(set(re.findall(r'dpi=(\d+)', log)))
    comp = json.loads((directory / 'bit-comparison.json').read_text(encoding='utf-8-sig'))
    flows.append(dict(directory=directory.name, dpi=dpis, equal=comp['equal'],
                      guiSha256=comp['guiSha256'], bytes=comp['guiBytes'],
                      exit0='EXIT=0' in log, remaining0='OWNED_PROCESS_REMAINING=0' in log))
    for file in sorted(directory.glob('*.png')):
        stem = file.name.replace('.print.png', '').replace('.screen.png', '')
        tree = directory / (stem + '.uia.json')
        elements = json.loads(tree.read_text(encoding='utf-8-sig')) if tree.exists() else []
        fonts = []
        for element in elements:
            try:
                fonts.append((element.get('id') or '', float(element['fontSizePt'])))
            except (KeyError, TypeError, ValueError):
                pass
        small = [(name, size) for name, size in fonts if size < 9]
        hidden = sum(bool(e.get('offscreen')) and e.get('type') in ['Button', 'MenuItem'] for e in elements)
        glyphs = sum(('\u25a1' in (e.get('name') or '') or '\ufffd' in (e.get('name') or '')) for e in elements)
        questions = sum('?' in (e.get('name') or '') for e in elements)
        ellipses = sum('…' in (e.get('name') or '') for e in elements)
        mojibake = sum(any(marker in (e.get('name') or '') for marker in ['Ã', 'Â', 'ì', 'ë', 'ï¿½']) for e in elements)
        image = Image.open(file).convert('RGB')
        crop = image.crop((12, 55, image.width - 12, image.height - 12))
        red, green, blue = crop.split()
        minimum = ImageChops.darker(ImageChops.darker(red, green), blue)
        histogram = minimum.histogram()
        white = sum(histogram[246:]) / sum(histogram)
        relative = file.relative_to(run).as_posix()
        sha = hashlib.sha256(file.read_bytes()).hexdigest()
        visual = '흰 본문; 구성/겹침/잘림 미확인' if white > .99 else '본문 내용 일부; 시각 판정 보류'
        font_text = ', '.join(name + '=' + str(size) + 'pt' for name, size in small) or '측정된 9pt 미만 없음'
        rows.append('| [' + relative + '](' + relative + ') | ' + '/'.join(dpis) + ' | ' +
                    str(image.width) + '×' + str(image.height) + ' | ' + visual +
                    ' | UIA 밖 버튼/메뉴 ' + str(hidden) + '개 | □/U+FFFD ' + str(glyphs) +
                    '개; ? ' + str(questions) + '개; … ' + str(ellipses) + '개; 인코딩 의심 ' + str(mojibake) +
                    '개; 실제 렌더/말줄임 미확인 | ' + font_text + '; 글꼴 읽기 ' +
                    str(len(fonts)) + '/' + str(len(elements)) + ' |')
        records.append(dict(path=relative, sha256=sha, dpi=dpis, size=list(image.size),
                            clientWhiteFraction=white, fontsRead=len(fonts), elements=len(elements),
                            smallFonts=small, offscreenButtonsAndMenus=hidden, suspiciousGlyphNames=glyphs,
                            questionMarkNames=questions, ellipsisNames=ellipses, mojibakeMarkerNames=mojibake))
for file in sorted(run.rglob('*')):
    if file.is_file() and (file.suffix == '.png' or file.name.endswith('.uia.json') or
                           file.name in ['bit-comparison.json', 'flow.log']):
        hashes.append(file.relative_to(run).as_posix() + '\t' + hashlib.sha256(file.read_bytes()).hexdigest() + '\t' + str(file.stat().st_size))
review = '''# GUI-C-255 스크린샷별 관찰표

최종 실행의 모든 PNG를 나열한다. DPI는 실제 GetDpiForWindow 값이며 이미지를 확대해 만든 조건이 아니다. 본문 흰색 비율은 제목 표시줄을 제외한 영역에서 RGB 채널 모두 245 초과인 화소의 비율이다. 화면 밖 수는 UIA IsOffscreen 버튼/메뉴 수이며 실제 잘림 확정 수가 아니다. 글꼴은 UIA TextPattern FontSize(pt)를 읽은 경우만 기록했다. 미지원 값으로 글자 크기를 추정하지 않는다. □/U+FFFD 부재도 화면의 한글 렌더 정상 판정은 아니다. ? 개수에는 정상 Help 버튼도 포함된다. … 개수는 UIA 이름에 이미 들어 있는 문자 수이며 렌더 중 발생한 말줄임 수가 아니다. 인코딩 의심은 Ã/Â/ì/ë/ï¿½ 표식이 있는 이름 수일 뿐 자동 깨짐 판정이 아니다.

| PNG | 실제 DPI | 크기(px) | 화면 구성/빈 영역 | 숨은 컨트롤(UIA) | 글자/깨짐 | 9pt 미만 및 읽기 범위 |
|---|---:|---|---|---|---|---|
''' + '\n'.join(rows) + '''

앱 종료 단계에는 창이 없어 PNG가 없으며 각 08-exit.uia.json은 빈 배열이다. 종료 직전 상태는 07-saved PNG이고 종료 코드와 프로세스 잔여 수는 flow.log에 있다. 최종 실행의 screen PNG는 전경 창이 해당 앱 소유가 아니어서 생략되었다. PrintWindow PNG의 본문만으로 레이아웃과 한글 가독성을 입증할 수 없다.
'''
summary = dict(flows=flows, pngCount=len(records), screenPngCount=sum(r['path'].endswith('.screen.png') for r in records),
               whiteClientCount=sum(r['clientWhiteFraction'] > .99 for r in records), artifactsIndexed=len(hashes), screenshots=records)
payload = dict(review=review, sha256='path\tsha256\tbytes\n' + '\n'.join(hashes) + '\n',
               summary=json.dumps(summary, ensure_ascii=False, indent=2))
print('DATA_START')
print(json.dumps(payload, ensure_ascii=False))
print('DATA_END')
