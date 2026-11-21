"""Check the appearance schema against native controls and both language packs."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / 'Telegram'
src = root / 'SourceFiles'
state = (src / 'core/appearance.cpp').read_text()
controls = (src / 'settings/sections/settings_appearance.cpp').read_text()
names = re.findall(r'"(\w+)"', re.search(r'auto Names = .*?;', state)[0])
sizes = list(map(int, re.findall(r'\d+', re.search(r'auto Sizes = .*?;', state)[0])))
choices = {key: len(re.findall(r'&tr::lng_', labels)) for key, labels in re.findall(
    r'{ Key::(\w+), Group::\w+, &tr::\w+,\s*{([^}]+)}', controls)}
assert choices == dict(zip(names, sizes, strict=True)), choices
ru = re.findall(r'^"(lng_look_\w+)"\s*=', (root/'Resources/langs/lang.strings').read_text(), re.M)
en = json.loads((root/'Resources/langs/localization/en.json').read_text())
assert len(set(ru)) == len(ru)
assert set(ru) == {k for k in en if k.startswith('lng_look_')}
assert set(re.findall(r'tr::(lng_look_\w+)', controls)) <= set(ru)
print(f'{len(choices)} native choices, {len(ru)} bilingual phrases')
