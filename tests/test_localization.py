from pathlib import Path
import re,ast
root=Path(__file__).resolve().parent.parent
src=(root/'flipper_gp_manager.c').read_text(encoding='utf8')
assert 'ЭКРАНЬ' not in src
assert 'ЭКРАНЫ' in src
assert 'СТАРТОВАЯ РЕШЁТКА' in src
assert 'КОМАНДА' in src and 'ПОЗИЦИИ' in src and 'ПОПЫТКА' in src and 'РЕЗУЛЬТАТЫ ПРОПАДУТ' in src
assert 'ЭКР OK' not in src and 'СПИС OK' not in src
# Every Cyrillic codepoint used in the UI is representable by bundled glyph renderer.
for c in set(src):
 if 0x0400<=ord(c)<=0x04FF:
  assert ('А'<=c<='Я') or ('а'<=c<='я') or c in 'Ёё',c
# The font contains independent shapes for letters formerly duplicated.
text=(root/'ru_font.c').read_text()
needle=re.compile(r'\{([^{}]+)\}, /\* U\+([0-9A-F]+) \*/')
glyph={int(k,16):tuple(int(s,16) for s in v.split(',')) for v,k in needle.findall(text)}
for a,b in [('Ш','Щ'),('Ы','Ь'),('Й','И'),('Ь','Ъ'),('Е','Ё')]:
 assert glyph[ord(a)]!=glyph[ord(b)],f'Indistinct Cyrillic: {a} / {b}'
print('PASS: all visible Russian labels supported, 5 distinct glyph pairs, no bad shortening')
