from pathlib import Path
import ast
root=Path(__file__).resolve().parent.parent
src=(root/'application.fam').read_text(encoding='utf8')
body=ast.parse(src)
app=body.body[0]
assert isinstance(app,ast.Expr) and isinstance(app.value,ast.Call)
kw={x.arg:x.value for x in app.value.keywords}
assert ast.literal_eval(kw['fap_version'])=='0.7.3'
assert ast.literal_eval(kw['appid'])=='flipper_gp_manager'
sources=ast.literal_eval(kw['sources'])
assert len(sources)==len(set(sources))
for source in sources:
 assert (root/source).is_file(),f'Missing source: {source}'
assert len(sources)>=8
print('PASS: application.fam valid, 0.7.3 id/version, source files present')
