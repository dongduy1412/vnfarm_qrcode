"""Verify redesigned lab forms and responsive states with isolated mock APIs."""
from pathlib import Path
from urllib.parse import urlsplit
from playwright.sync_api import sync_playwright

root=Path(__file__).resolve().parents[1]/'web/static'
with sync_playwright() as p:
    browser=p.chromium.launch()
    context=browser.new_context(permissions=['clipboard-read','clipboard-write'])
    page=context.new_page();errors=[];writes=[]
    state={'online':True,'valid':True,'session':None,'registered':False}
    base='https://example.test/checkin'
    page.on('pageerror',lambda error:errors.append(str(error)))
    def route(r):
        path=urlsplit(r.request.url).path
        if path=='/api/status':
            if not state['online']:r.fulfill(status=503,json={'error':'offline'});return
            body={'can_register':True,'base_url':base,'usb':{'supported':True,'ports':[{'port':'COM5'}]},'devices':[], 'session':state['session']}
        elif path=='/api/wifi/commands':
            assert r.request.method=='GET','Never send real/reset commands in this test'
            body={'commands':[]}
        elif path=='/api/checkin':
            writes.append((path,r.request.post_data_json))
            if not state['valid']:r.fulfill(status=401,json={'error':'invalid_signature'});return
            state['session']={'device_id':'fixture','checked_in_at':1793866400,'issued_at':1793866380,'expires_at':1793866440}
            body={'ok':True}
        elif path=='/api/logout':
            state['session']=None;body={'ok':True}
        elif path=='/api/register':
            writes.append((path,r.request.post_data_json));state['registered']=True;body={'device_id':'fixture'}
        elif path.startswith('/api/'):raise AssertionError('Unexpected API: '+path)
        else:
            r.fulfill(path=str(root/('index.html' if path=='/' else path.lstrip('/'))));return
        r.fulfill(json=body)
    page.route('**/*',route)
    page.goto('http://127.0.0.1:8765/');page.wait_for_load_state('networkidle')
    for width in (320,390,768,1024,1440):
        page.set_viewport_size({'width':width,'height':900})
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),width
    page.locator('#qr-url').fill(base+'?fixture=1');page.locator('#verify-button').click()
    page.wait_for_function('document.querySelector("#session-title").textContent === "Chữ ký hợp lệ"')
    assert page.locator('#session-details dd').count()==4
    page.locator('#logout').click();page.locator('#logout').wait_for(state='hidden')
    state['valid']=False
    page.locator('#verify-button').click();page.locator('#verify-result.error').wait_for()
    assert not page.locator('#verify-button').is_disabled()
    page.locator('#copy-url').click()
    assert page.evaluate('navigator.clipboard.readText()')==base
    page.locator('#key-file').set_input_files({'name':'fixture.txt','mimeType':'text/plain','buffer':b'fixture-public-key'})
    assert page.locator('#public-key').input_value()=='fixture-public-key'
    page.locator('#device-id').fill('fixture');page.locator('#ttl').fill('60');page.locator('#register-button').click()
    page.locator('#register-result.success').wait_for()
    assert writes[-1]==('/api/register',{'device_id':'fixture','public_key':'fixture-public-key','ttl':60})
    assert state['registered']
    page.locator('nav a[href="#keys"]').click()
    assert page.locator('nav a[href="#keys"]').get_attribute('class').endswith('active')
    state['online']=False;page.reload();page.locator('#connection.offline').wait_for()
    assert 'active' not in page.locator('#server-dot').get_attribute('class')
    assert not errors,errors
    browser.close()
print('PASS: 5 viewport sizes, check-in success/error, logout, copy, file import, key form, navigation and offline state.')
