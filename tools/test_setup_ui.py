"""Exercise the exact embedded setup HTML at mobile/desktop and HTTP origins.

Runs an isolated headless browser with mocked Wi-Fi responses; never sends
test network credentials to the physical board.
"""
import html
import json
from pathlib import Path
from urllib.parse import parse_qs, urlsplit
from playwright.sync_api import sync_playwright

root=Path(__file__).resolve().parents[1]
template=(root/'web/setup/index.html').read_text(encoding='utf-8')
public=json.loads((root/'docs/contract-vectors.json').read_text())['publicKey']
names=['VN FARM Office','Home & <guest> "network"','{{DEVICE_ID}}']
def render(saved=True):
    page=template
    values={'DEVICE_ID':'7ce8b1b17e60','PUBLIC_KEY':public,'INTERVAL':'30',
      'CHECKIN_URL':'https://vnfarm-qrcode-device-link.dong141220047.workers.dev/checkin',
      'WORKER_ORIGIN':'https://vnfarm-qrcode-device-link.dong141220047.workers.dev',
      'HEARTBEAT_CHECKED':'checked','SAVED_HIDDEN':'' if saved else 'hidden'}
    for key,value in values.items():page=page.replace('{{'+key+'}}',html.escape(value,quote=True))
    options=''.join(f'<option value="{html.escape(name,quote=True)}">{html.escape(name)}</option>' for name in names) if saved else ''
    return page.replace('{{SAVED_OPTIONS}}',options)

with sync_playwright() as p:
    browser=p.chromium.launch(headless=True)
    context=browser.new_context(viewport={'width':1120,'height':900},device_scale_factor=1,accept_downloads=True)
    state={'saved':True,'code':400}
    requests=[]; errors=[]
    def route(request_route):
        path=urlsplit(request_route.request.url).path
        if path=='/save':
            requests.append(parse_qs(request_route.request.post_data or '',keep_blank_values=True))
            request_route.fulfill(status=state['code'],content_type='text/plain',body='Wi-Fi response')
        elif path=='/public-key':request_route.fulfill(content_type='text/plain',headers={'Content-Disposition':'attachment; filename="public-key.txt"'},body=public)
        else:request_route.fulfill(content_type='text/html',body=render(state['saved']))
    context.route('**/*',route)
    page=context.new_page()
    page.on('pageerror',lambda error:errors.append(str(error)))
    page.goto('http://192.168.4.1/')
    page.wait_for_load_state('networkidle')
    assert not page.evaluate('window.isSecureContext')
    assert page.locator('#public-key').input_value()==public
    assert page.locator('#saved-network option').all_text_contents()==names
    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
    (root/'docs/previews').mkdir(exist_ok=True)
    page.screenshot(path=str(root/'docs/previews/setup-desktop.png'),full_page=True)
    page.set_viewport_size({'width':390,'height':844})
    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
    page.screenshot(path=str(root/'docs/previews/setup-mobile.png'),full_page=True)
    page.evaluate("document.addEventListener('copy',()=>{const el=document.activeElement;window.copiedSelection=el.value.slice(el.selectionStart,el.selectionEnd)})")
    page.locator('#copy-key').click()
    assert page.evaluate('window.copiedSelection')==public
    assert page.locator('#copy-status').inner_text()=='Đã sao chép'
    assert page.locator('#copy-label').inner_text()=='Sao chép public key'
    assert page.locator('#public-key').evaluate('(el)=>el.selectionEnd===el.selectionStart')
    assert page.locator('textarea').count()==1
    assert not requests,'Copy must never submit Wi-Fi settings'
    page.evaluate("document.execCommand=()=>false")
    page.locator('#copy-key').click()
    assert 'error' in page.locator('#copy-status').get_attribute('class')
    assert page.locator('#public-key').evaluate('(el)=>el.selectionEnd===el.selectionStart')
    assert page.locator('textarea').count()==1
    with page.expect_download() as download:
        page.get_by_role('link',name='Tải file').click()
    assert Path(download.value.path()).read_text()==public
    assert download.value.suggested_filename=='public-key.txt'
    page.locator('#new-network button[type=submit]').click()
    assert not requests,'Empty SSID must not submit'
    assert 'Chưa gửi' in page.locator('#save-status').inner_text()
    page.locator('#ssid').fill('Test network')
    page.locator('#password').fill('test-password-only')
    page.locator('#show-password').click()
    assert page.locator('#password').get_attribute('type')=='text'
    page.locator('#show-password').click()
    assert page.locator('#password').get_attribute('type')=='password'
    page.locator('#new-network button[type=submit]').click()
    page.locator('#save-status.error').wait_for()
    assert 'HTTP 400' in page.locator('#save-status').inner_text()
    assert 'Wi-Fi response' in page.locator('#save-status').inner_text()
    assert page.locator('#ssid').input_value()=='Test network'
    assert page.locator('#new-network button[type=submit]').is_enabled()
    assert requests[-1]['seconds']==['30']
    page.locator('#saved-network').select_option(names[1])
    state['code']=200
    page.locator('.saved button[type=submit]').click()
    page.locator('#save-status.success').wait_for()
    assert requests[-1]['saved']==[names[1]] and 'password' not in requests[-1]
    assert requests[-1]['worker']==['https://vnfarm-qrcode-device-link.dong141220047.workers.dev']
    assert page.locator('#new-network button[type=submit]').is_disabled()
    state['saved']=False
    page.reload();page.wait_for_load_state('networkidle')
    assert not page.locator('.saved').is_visible()
    assert not page.locator('.divider').is_visible()
    page.locator('#worker').fill('https://project.example.com/')
    assert page.locator('#checkin').input_value()=='https://project.example.com/checkin'
    assert not page.locator('#heartbeat').is_checked()
    page.locator('#ssid').fill('Fixture')
    state['code']=400
    page.locator('#new-network button[type=submit]').click()
    page.locator('#save-status.error').wait_for()
    assert requests[-1]['worker']==['https://project.example.com']
    assert requests[-1]['checkin']==['https://project.example.com/checkin']
    assert requests[-1]['heartbeat']==['0']
    previous=len(requests)
    for invalid in ('http://project.example.com','https://user@project.example.com','https://project.example.com/api','https://project.example.com?x=1'):
        page.locator('#worker').fill(invalid)
        page.locator('#new-network button[type=submit]').click()
        assert len(requests)==previous,'Invalid origin must not submit'
    page.on('dialog',lambda dialog:dialog.accept())
    page.locator('#worker').fill('http://192.168.1.12:8787')
    previous=len(requests)
    page.locator('#new-network button[type=submit]').click()
    assert len(requests)==previous
    assert 'tích ô' in page.locator('#save-status').inner_text()
    page.locator('#allow-http').check()
    page.locator('#new-network button[type=submit]').click()
    page.locator('#save-status.error').wait_for()
    assert requests[-1]['worker']==['http://192.168.1.12:8787']
    assert requests[-1]['checkin']==['http://192.168.1.12:8787/checkin']
    previous=len(requests)
    for invalid in ('http://8.8.8.8:8787','http://localhost:8787','http://192.168.1.12:0','http://192.168.1.12:65536','http://192.168.001.12:8787'):
        page.locator('#worker').fill(invalid)
        page.locator('#new-network button[type=submit]').click()
        assert len(requests)==previous
    page.locator('#worker').fill('https://project.example.com')
    page.evaluate('window.AbortController=undefined')
    page.locator('#new-network button[type=submit]').click()
    assert 'Lỗi trình duyệt' in page.locator('#save-status').inner_text()
    assert page.locator('#new-network button[type=submit]').is_enabled()
    page.set_viewport_size({'width':320,'height':700})
    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
    # Secure-context Clipboard API branch: use the actual browser clipboard.
    context.grant_permissions(['clipboard-read','clipboard-write'],origin='http://localhost:8769')
    page.goto('http://localhost:8769/');page.wait_for_load_state('networkidle')
    page.locator('#copy-key').click()
    page.wait_for_function("document.getElementById('copy-status').textContent==='Đã sao chép'")
    assert page.evaluate('navigator.clipboard.readText()')==public
    assert not errors,errors
    browser.close()
print('PASS: responsive 320/390/1120px, HTTP copy/fallback, Clipboard API, download, password toggle, saved-network escaping, validation and save/retry states.')
