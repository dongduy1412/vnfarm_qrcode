const $ = (selector) => document.querySelector(selector);
let currentStatus;

function notify(selector, message, kind = 'success') {
  const el = $(selector);
  el.textContent = message;
  el.className = `notice ${kind}`;
}

async function api(path, data, options = {}) {
  const response = await fetch(path, data === undefined ? { cache: 'no-store' } : {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data), ...options
  });
  const body = await response.json();
  const errors = { bad_request: 'Dữ liệu QR không đúng contract hoặc thời gian ở tương lai quá xa.', invalid_signature: 'Chữ ký không hợp lệ.', expired: 'QR đã hết hạn. Hãy quét mã mới.', unknown_device: 'Thiết bị chưa đăng ký public key.', rate_limited: 'Thao tác quá nhanh. Hãy thử lại sau.' };
  if (!response.ok) throw new Error(errors[body.error] || body.error || 'Yêu cầu thất bại.');
  return body;
}

function dateTime(seconds) {
  return new Date(seconds * 1000).toLocaleString('vi-VN');
}

function renderSession(session) {
  const active = Boolean(session);
  $('#session-title').textContent = active ? 'Chữ ký hợp lệ' : 'Chưa xác minh';
  $('#session-description').textContent = active ? 'QR khớp public key và còn hạn tại thời điểm server nhận.' : 'Chưa nhận được QR khớp public key đã đăng ký.';
  $('#session-dot').classList.toggle('active', active);
  $('#session-details').classList.toggle('hidden', !active);
  $('#logout').classList.toggle('hidden', !active);
  $('#device-status').textContent = active ? `Đã xác minh lúc ${dateTime(session.checked_in_at)}` : 'Chưa nhận QR hợp lệ';
  $('#session-details').replaceChildren();
  if (active) {
    for (const [label, value] of [['Thiết bị', session.device_id], ['Mã được tạo lúc', dateTime(session.issued_at)], ['Server xác minh lúc', dateTime(session.checked_in_at)], ['Mã đó hết hạn lúc', dateTime(session.expires_at)]]) {
      const dt = document.createElement('dt'); dt.textContent = label;
      const dd = document.createElement('dd'); dd.textContent = value;
      $('#session-details').append(dt, dd);
    }
  }
}

function renderDevices(devices, canRegister) {
  $('#device-list').replaceChildren();
  if (!devices.length) {
    const hint = document.createElement('p'); hint.className = 'hint';
    hint.textContent = 'Chưa có thiết bị nào được đăng ký.';
    $('#device-list').append(hint);
  }
  devices.forEach((device) => {
    const row = document.createElement('div'); row.className = 'device-row';
    const content = document.createElement('div');
    const name = document.createElement('strong'); name.textContent = device.device_id;
    const detail = document.createElement('p'); detail.textContent = `TTL server ${device.ttl}s · Fingerprint: ${device.kid}`;
    content.append(name, detail); row.append(content);
    if (canRegister) {
      const remove = document.createElement('button'); remove.type = 'button'; remove.className = 'button secondary'; remove.textContent = 'Xóa đăng ký';
      remove.addEventListener('click', async () => {
        try { await api('/api/remove', { device_id: device.device_id }); await refresh(); }
        catch (error) { notify('#register-result', error.message, 'error'); }
      });
      row.append(remove);
    }
    $('#device-list').append(row);
  });
}

async function refresh() {
  try {
    const status = await api('/api/status');
    currentStatus = status;
    $('#connection').textContent = 'Server đang chạy'; $('#connection').className = 'connection online';
    $('#base-url').textContent = status.base_url;
    $('#http-status').textContent = 'Đã kết nối';
    $('#usb-status').textContent = !status.usb.supported ? 'Không đọc được danh sách USB' :
      status.usb.ports.length ? status.usb.ports.map((port) => `${port.port} · USB JTAG/Serial`).join(', ') : 'Chưa nhận diện board USB';
    renderSession(status.session);
    renderDevices(status.devices, status.can_register);
    $('#register-button').disabled = !status.can_register;
    if (!status.can_register) notify('#registration-help', `Đăng ký key trên máy tính bằng http://127.0.0.1:${location.port || '8765'}. Trang điện thoại dùng để quét và xác minh QR.`, 'info');
    return status;
  } catch (error) {
    $('#connection').textContent = 'Mất kết nối server'; $('#connection').className = 'connection offline';
    $('#http-status').textContent = 'Mất kết nối';
    $('#usb-status').textContent = 'Không cập nhật được';
    renderSession(null);
    throw error;
  }
}

async function checkin(url) {
  $('#verify-button').disabled = true;
  $('#verify-button').textContent = 'Đang xác minh…';
  try {
    await api('/api/checkin', { url });
    notify('#verify-result', 'Xác minh thành công: public key khớp, chữ ký hợp lệ và mã còn hạn khi nhận.');
    await refresh();
  } catch (error) {
    notify('#verify-result', error.message, 'error');
    await refresh().catch(() => {});
  } finally {
    $('#verify-button').disabled = false;
    $('#verify-button').textContent = 'Xác minh & check-in ↗';
  }
}

$('#verify-form').addEventListener('submit', (event) => {
  event.preventDefault(); checkin($('#qr-url').value.trim());
});

$('#register-form').addEventListener('submit', async (event) => {
  event.preventDefault(); $('#register-button').disabled = true;
  try {
    const data = await api('/api/register', { device_id: $('#device-id').value, public_key: $('#public-key').value, ttl: Number($('#ttl').value) });
    notify('#register-result', `Đã lưu public key cho ${data.device_id}. Bây giờ quét QR mới trên màn hình thiết bị.`);
    $('#public-key').value = '';
    await refresh();
  } catch (error) {
    notify('#register-result', error.message, 'error');
    $('#register-button').disabled = !currentStatus?.can_register;
  }
});

$('#key-file').addEventListener('change', async (event) => {
  const file = event.target.files[0];
  if (!file) return;
  if (file.size > 4096) return notify('#register-result', 'File public key quá lớn.', 'error');
  $('#public-key').value = await file.text();
});

$('#copy-url').addEventListener('click', async () => {
  if (!currentStatus) return;
  try {
    await navigator.clipboard.writeText(currentStatus.base_url);
    $('#toast').textContent = 'Đã copy URL cho firmware'; $('#toast').classList.remove('hidden');
    setTimeout(() => $('#toast').classList.add('hidden'), 2300);
  } catch {
    const range = document.createRange(); range.selectNodeContents($('#base-url'));
    const selection = window.getSelection(); selection.removeAllRanges(); selection.addRange(range);
    $('#toast').textContent = 'Đã chọn URL. Nhấn giữ để copy.'; $('#toast').classList.remove('hidden');
    setTimeout(() => $('#toast').classList.add('hidden'), 3000);
  }
});

$('#logout').addEventListener('click', async () => {
  try { await api('/api/logout', {}); notify('#verify-result', 'Đã xóa kết quả. Quét một mã mới để thử lại.', 'info'); await refresh(); }
  catch (error) { notify('#verify-result', error.message, 'error'); }
});

async function init() {
  const fromQr = location.pathname === '/checkin' && location.search.length > 1;
  const qrUrl = location.href;
  if (fromQr) {
    $('#qr-url').value = qrUrl;
    history.replaceState(null, '', '/checkin');
  }
  try { await refresh(); if (fromQr) await checkin(qrUrl); }
  catch { notify('#verify-result', 'Chưa kết nối được server test. Hãy kiểm tra server đang chạy.', 'error'); }
  setInterval(() => refresh().catch(() => {}), 15000);
}
init();

