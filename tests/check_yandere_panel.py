import json
import struct
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'Telegram'
qrc = root / 'Resources/qrc/telegram/telegram.qrc'
res = {e.attrib['alias']: qrc.parent / e.text for e in ET.parse(qrc).iter('file')}
for role, width in [('founder', 23), ('user', 19)]:
    png = res[f'art/badge_{role}.png'].read_bytes()
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II', png[16:24]) == (8 * 4 * width, 8 * 4 * 19)
assert res['art/telegram_service.png'].read_bytes() != res['art/logo_256_no_margin.png'].read_bytes()
langs = root / 'Resources/langs'
en = json.loads((langs / 'localization/en.json').read_text())
ru = (langs / 'lang.strings').read_text()
for key in ('role_user', 'role_label', 'publish_badge', 'panel_search', 'panel_empty', 'panel_favorites'):
    assert en[f'lng_yg_{key}'] and ru.count(f'"lng_yg_{key}" =') == 1
print('Badge sheets, separate service icon and panel translations OK')
