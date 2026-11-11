'use strict';
let pairing='';
function qr(id,value){const code=qrcode(0,'M');code.addData(value);code.make();document.getElementById(id).innerHTML=code.createImgTag(5,4,'QR-код');}
async function load(){try{const response=await fetch('/api/pair',{cache:'no-store'});if(!response.ok)throw Error('Откройте страницу на компьютере с OBS.');const p=await response.json();const address=`${p.address}:${p.port}`;pairing=`obslan://${address}#${p.token}`;document.getElementById('address').textContent=address;document.getElementById('token').textContent=p.token;document.getElementById('state').textContent=p.connected?'Смартфон подключён':'OBS готов. Ожидаем смартфон.';document.getElementById('download').href=p.apk_url;qr('downloadqr',p.apk_url);qr('pairqr',pairing);}catch(e){document.getElementById('error').textContent=e.message;}}
document.getElementById('copy').addEventListener('click',async()=>{try{await navigator.clipboard.writeText(pairing);document.getElementById('copy').textContent='Скопировано';}catch{document.getElementById('copy').textContent='Скопируйте адрес и ключ из полей выше';}});
load();setInterval(load,5000);
