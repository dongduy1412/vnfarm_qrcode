"""PlatformIO pre-build: embed the local-only setup UI in firmware flash."""
from pathlib import Path
Import('env')
root=Path(env.subst('$PROJECT_DIR'))
generated=Path(env.subst('$BUILD_DIR'))/'generated'
generated.mkdir(parents=True,exist_ok=True)
html=(root/'web/setup/index.html').read_text(encoding='utf-8')
if ')PORTAL"' in html: raise ValueError('Setup page contains reserved C++ raw-string delimiter')
header='#pragma once\n#include <pgmspace.h>\nstatic const char SETUP_PAGE[] PROGMEM = R"PORTAL('+html+')PORTAL";\n'
target=generated/'SetupPageAsset.h'
if not target.exists() or target.read_text(encoding='utf-8')!=header: target.write_text(header,encoding='utf-8')
env.Append(CPPPATH=[str(generated)])
