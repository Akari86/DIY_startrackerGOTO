#pragma once
static const char PHONE_PAGE[] PROGMEM=R"PAGE(
<!doctype html><html lang="th"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>StarTracker</title><style>body{font:17px system-ui;background:#091018;color:#e6faff;max-width:540px;margin:30px auto;padding:20px}input,button{box-sizing:border-box;width:100%;padding:12px;margin:6px 0 16px;background:#172936;color:white;border:1px solid #467080;border-radius:8px}button{background:#176375}label{display:block}#status{color:#9de7c9}</style>
<h1>StarTracker</h1><p>ตั้งเวลาจากมือถือเมื่อเปิดหน้านี้ แล้วกรอกพิกัดและตั้งค่าการเชื่อมต่อ</p>
<p id="status">กำลังเชื่อมต่อ…</p><button id="sync">ตั้งเวลาอีกครั้ง</button>
<button id="calibrate">คาลิเบรตทัชหน้าจออุปกรณ์ใหม่</button>
<form id="settings"><label>ละติจูด (ใต้เป็นลบ)<input name="lat" type="number" step="any" min="-90" max="90" required></label>
<label>ลองจิจูด (ตะวันออกเป็นบวก)<input name="lon" type="number" step="any" min="-180" max="180" required></label>
<label>ชดเชยมุมทิศ / ทิศเหนือจริง (องศา)<input name="az" type="number" step="any" min="-360" max="360" value="0" required></label>
<label>ชดเชยมุมเงย (องศา)<input name="alt" type="number" step="any" min="-90" max="90" value="0" required></label>
<label><input name="aligned" type="checkbox" style="width:auto"> ติดตั้งแกน IMU และเทียบทิศเหนือ/แนวกล้องแล้ว</label>
<label>LX200 IPv4 (เว้นว่างหากยังไม่เชื่อม)<input name="host" placeholder="192.168.4.2"></label>
<label>LX200 TCP port<input name="port" type="number" min="1" max="65535" value="4030" required></label>
<p>อุปกรณ์ LX200 แบบ TCP ต้องอยู่ในเครือข่าย StarTracker เดียวกัน</p>
<button>บันทึกการตั้งค่า</button></form><p>ข้อมูล RA/DEC จาก IMU เป็นค่าประมาณ ต้องคาลิเบรตเซ็นเซอร์และแนวติดตั้งก่อนใช้งาน</p>
<script>
const status=document.querySelector('#status'), form=document.querySelector('form');
async function post(path,data){const r=await fetch(path,{method:'POST',body:new URLSearchParams(data)});const t=await r.text();if(!r.ok)throw Error(t);return t;}
async function sync(){try{status.textContent=await post('/time',{epoch:Math.floor(Date.now()/1000),offset:-new Date().getTimezoneOffset()});}catch(e){status.textContent=e.message;}}
document.querySelector('#sync').onclick=sync;
document.querySelector('#calibrate').onclick=async()=>{try{status.textContent=await post('/touch/calibrate',{});}catch(e){status.textContent=e.message;}};
form.onsubmit=async e=>{e.preventDefault();try{status.textContent=await post('/settings',new FormData(form));}catch(e){status.textContent=e.message;}};
fetch('/settings').then(r=>r.json()).then(v=>{for(const k in v){const el=form.elements.namedItem(k);if(el){if(el.type==='checkbox')el.checked=v[k];else el.value=v[k];}}}).catch(e=>status.textContent=e.message);
sync();
</script></html>
)PAGE";
