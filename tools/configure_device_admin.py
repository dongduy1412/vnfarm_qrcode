"""Create/reuse ignored operator credentials; never print the token."""
import json
import secrets
from pathlib import Path

runtime=Path(__file__).resolve().parents[1]/'cloudflare/.runtime'
runtime.mkdir(exist_ok=True)
token_file=runtime/'admin-token.txt'
if not token_file.exists():
    with token_file.open('x',encoding='ascii') as out:out.write(secrets.token_urlsafe(48))
token=token_file.read_text(encoding='ascii').strip()
if not 32<=len(token)<=256:raise ValueError('Existing admin token has invalid length; not overwriting')
(runtime/'admin-secrets.json').write_text(json.dumps({'ADMIN_API_TOKEN':token})+'\n',encoding='ascii')
print('Operator credentials ready in ignored cloudflare/.runtime files. Token not printed.')
