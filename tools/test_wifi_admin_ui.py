"""Mock every API: exercise operator buttons without sending real commands."""
import json
from pathlib import Path
from urllib.parse import urlsplit
from playwright.sync_api import sync_playwright

root=Path(__file__).resolve().parents[1]/'web/static'
with sync_playwright() as p:
    browser=p.chromium.launch()
    page=browser.new_page()
    state={'local':True,'conflict':False};sent=[];errors=[]
    page.on('pageerror',lambda error:errors.append(str(error)))
    def route(r):
        path=urlsplit(r.request.url).path
        if path=='/api/status':
            body={'can_register':state['local'],'base_url':'https://example.test/checkin','usb':{'supported':True,'ports':[]},'devices':[],'session':None}
        elif path=='/api/wifi/commands':
            if r.request.method=='POST':
                sent.append(r.request.post_data_json)
                if state['conflict']:
                    r.fulfill(status=409,json={'error':'command_pending'});return
                body={'command':sent[-1]['command'],'cmdId':'c_test'}
            else:body={'commands':[]}
        elif path.startswith('/api/'):
            raise AssertionError('Unexpected API: '+path)
        else:
            file=root/('index.html' if path=='/' else path.lstrip('/'))
            r.fulfill(path=str(file));return
        r.fulfill(json=body)
    page.route('**/*',route)
    for width in (390,1120):
        page.set_viewport_size({'width':width,'height':900})
        page.goto('http://127.0.0.1:8765/')
        page.locator('#wifi-control').wait_for(state='visible')
        for command in ('reconnect_wifi','forget_current_wifi','wipe_wifi'):
            page.locator(f'[data-wifi-command="{command}"]').click()
            page.wait_for_function('!document.querySelector("[data-wifi-command]").disabled')
            assert sent[-1]=={'command':command}
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth')
    state['conflict']=True
    page.locator('[data-wifi-command="reconnect_wifi"]').click()
    page.locator('#wifi-command-result.error').wait_for()
    assert 'chưa xác nhận' in page.locator('#wifi-command-result').inner_text()
    state['local']=False
    page.reload()
    page.wait_for_function('document.querySelector("#connection").textContent==="Server đang chạy"')
    assert not page.locator('#wifi-control').is_visible()
    assert not errors,errors
    browser.close()
print('PASS: three mocked commands, pending error, desktop/mobile layout, remote controls hidden.')
