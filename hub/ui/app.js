'use strict';
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const host=window.chrome?.webview;
const send=(action,data={})=>host?.postMessage({action,...data});
let language=1,motion=true,state=null,pair={slot:0,mode:0,adapter:0},pairData=null,theme=localStorage.getItem('theme')==='light'?'light':'dark';
const tr=key=>(window.MC_TEXT[key]||[key,key,key,key,key])[language];
const svg=name=>`<svg aria-hidden="true"><use href="#i-${name}"/></svg>`;
function toast(key){$('#toast').textContent=tr(key);$('#toast').classList.add('visible');clearTimeout(toast.timeout);toast.timeout=setTimeout(()=>$('#toast').classList.remove('visible'),3200);}
function setTheme(value){theme=value;document.documentElement.dataset.theme=value;localStorage.setItem('theme',value);$('#themeLabel').textContent=tr(value);$$('[data-theme-choice]').forEach(b=>b.classList.toggle('selected',b.dataset.themeChoice===value));send('theme',{value});}
function translate(){
 document.documentElement.lang=['en','ru','ja','es','zh-Hans'][language];$$('[data-t]').forEach(node=>node.textContent=tr(node.dataset.t));$('#language').value=language;setTheme(theme);
 $$('.close-dialog.icon-button').forEach(button=>button.setAttribute('aria-label',tr('close')));$('#language').setAttribute('aria-label',tr('language'));
 if(state)renderState();if(pairData)renderPair(pairData);$('#cameras').querySelectorAll('.camera-card').forEach((card,i)=>{card.querySelector('.card-name').textContent=tr('phone')+' '+(i+1);});
}
function makeCards(){
 for(let i=0;i<5;i++){
  const card=document.createElement('article');card.className='camera-card';card.dataset.slot=i;
  card.innerHTML=`<div class="card-heading"><div><h2 class="card-name"></h2><div class="device-label">MultiCamera T.E. ${i+1}</div></div><span class="camera-number">0${i+1}</span></div><div class="view"><div class="empty-view"><img src="assets/preview-lens.png" alt=""><span></span></div><img class="preview" alt=""><span class="live-indicator"><span class="status-dot"></span>LIVE</span><span class="mic-indicator">MIC</span></div><div class="card-bottom"><div class="card-state"><span class="state-text"></span><span class="resolution"></span></div><button class="connect-button">${svg('plus')}<span></span></button></div>`;
  card.querySelector('button').addEventListener('click',()=>openPair(i));$('#cameras').appendChild(card);
 }
 const guide=document.createElement('article');guide.className='onboarding';guide.innerHTML=`<div class="intro"><div class="smallcaps">MULTICAMERA T.E.</div><h3 data-t="your_studio"></h3><p data-t="onboarding_hint"></p></div><img class="phone-art" src="assets/connect-phone.png" alt=""><button class="text-button">${svg('help')}<span data-t="quick_guide"></span>${svg('arrow')}</button>`;guide.querySelector('button').onclick=()=>openDialog('guideDialog');$('#cameras').appendChild(guide);
}
function renderState(){
 const count=state.slots.filter(s=>s.connected).length;$('#count').textContent=tr('connected_count').replace('{n}',count);$('#networkAddress').textContent='LAN · '+state.address+':34872';
 const cameraKey=state.camera===1?'camera_ready':state.camera===0?'camera_starting':'camera_error';$('#sideStatus').textContent=tr(cameraKey);$('#sideDot').classList.toggle('warning',state.camera!==1);$('#installCameras').hidden=state.camera===1;
 $('#relayStatus').textContent=tr(state.relay<0?'relay_off':state.relay===5?'relay_ready':'relay_connecting');
 state.slots.forEach((slot,i)=>{
  const card=$(`[data-slot="${i}"]`);card.classList.toggle('connected',slot.connected);card.querySelector('.card-state').classList.toggle('connected',slot.connected);
  card.querySelector('.state-text').textContent=tr(slot.connected?'connected':'available');card.querySelector('.empty-view span').textContent=tr('waiting');card.querySelector('.resolution').textContent=slot.connected&&slot.width?slot.width+' × '+slot.height:'';
  card.querySelector('.connect-button span').textContent=tr(slot.connected?'connection':'connect_short');card.querySelector('.connect-button use').setAttribute('href',slot.connected?'#i-qr':'#i-plus');card.querySelector('.mic-indicator').style.display=slot.connected&&slot.audio?'block':'none';
  if(!slot.connected){card.querySelector('.preview').removeAttribute('src');card.querySelector('.preview').alt='';}
 });
}
function openDialog(id){
 const dialog=$('#'+id);$$('dialog[open]').forEach(d=>{if(d!==dialog)d.close();});if(!dialog.open)dialog.showModal();
}
function openPair(slot){pair={slot,mode:pair.mode,adapter:pair.adapter};$('#pairHeading').textContent=tr('phone')+' '+(slot+1);openDialog('pairDialog');requestPair();}
function requestPair(){send('pair',pair);}
function qr(canvas,data){
 const context=canvas.getContext('2d');const n=data?data.size:0,scale=n?Math.max(1,Math.floor(224/(n+8))):1,size=n?(n+8)*scale:224;canvas.width=size;canvas.height=size;context.fillStyle='#fff';context.fillRect(0,0,size,size);if(!data)return;context.fillStyle='#14221c';for(let y=0;y<n;y++)for(let x=0;x<n;x++)if(data.bits[y*n+x]==='1')context.fillRect((x+4)*scale,(y+4)*scale,scale,scale);
}
function renderPair(data){
 pairData=data;pair={slot:data.slot,mode:data.mode,adapter:data.adapter};$('#pairHeading').textContent=tr('phone')+' '+(data.slot+1);
 $$('[data-mode]').forEach(b=>b.setAttribute('aria-selected',Number(b.dataset.mode)===pair.mode));qr($('#apkQr'),data.apk);qr($('#pairQr'),data.connection);$('#qrUnavailable').style.display=data.connection?'none':'flex';$('#copyConnection').disabled=!data.connection;
 const usb=pair.mode===2,internet=pair.mode===1;
 $('#connectionNote').textContent=tr(usb?'usb_missing':internet?'internet_note':'same_network');$('#pairHint').textContent=tr(usb?(data.connection?'usb_pair_hint':'usb_missing'):internet&&!data.connection?(data.server?'relay_off':'relay_missing'):'pair_hint');$('#pairAddress').textContent=data.connection?(internet?'WSS · ENCRYPTED':data.address):tr('not_ready');$('#usbControls').style.display=usb?'flex':'none';
 const select=$('#usbAdapters');select.replaceChildren();if(!data.adapters.length){const option=new Option(tr('usb_refresh'),'0');select.add(option);}else data.adapters.forEach(a=>select.add(new Option(a.label,a.index)));select.value=pair.adapter;$('#allowUsb').disabled=!data.adapters.length;
}
function applyVisibility(){document.body.classList.toggle('motion-off',!motion);}
makeCards();translate();applyVisibility();
$('#theme').onclick=()=>setTheme(theme==='dark'?'light':'dark');$$('[data-theme-choice]').forEach(button=>button.onclick=()=>setTheme(button.dataset.themeChoice));
$('#language').onchange=()=>{language=Number($('#language').value);translate();send('preferences',{language,animation:motion});};
$('#navSettings').onclick=()=>{send('settings');};$('#navGuide').onclick=()=>openDialog('guideDialog');$('#guideButton').onclick=()=>openPair(Math.max(0,state?state.slots.findIndex(s=>!s.connected):0));$('#navCameras').onclick=()=>{$$('dialog[open]').forEach(d=>d.close());window.scrollTo({top:0});};
$('#release').onclick=()=>send('release');$('#guideConnect').onclick=()=>openPair(Math.max(0,state?state.slots.findIndex(s=>!s.connected):0));$('#installCameras').onclick=()=>send('install');
$$('.close-dialog').forEach(button=>button.onclick=()=>button.closest('dialog').close());$$('dialog').forEach(dialog=>{dialog.addEventListener('click',event=>{if(event.target===dialog){const r=dialog.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)dialog.close();}});});
$$('[data-mode]').forEach(button=>button.onclick=()=>{pair.mode=Number(button.dataset.mode);requestPair();});$('#usbAdapters').onchange=()=>{pair.adapter=Number($('#usbAdapters').value);requestPair();};$('#refreshUsb').onclick=requestPair;$('#allowUsb').onclick=()=>send('usb',{adapter:pair.adapter});$('#copyConnection').onclick=()=>send('copy',pair);
$('#saveSettings').onclick=()=>{motion=$('#animation').checked;send('save',{server:$('#server').value,key:$('#hostKey').value,enabled:$('#internetEnabled').checked,animation:motion});};
document.addEventListener('visibilitychange',()=>document.body.classList.toggle('paused',document.hidden));
if(host)host.addEventListener('message',event=>{
 const data=event.data;
 switch(data.type){
  case 'init':language=data.language;motion=data.animation;$('#version').textContent=data.version;translate();applyVisibility();break;
  case 'state':state=data;renderState();break;
  case 'preview':{const image=$(`[data-slot="${data.slot}"] .preview`);image.src=data.image;image.alt=tr('phone')+' '+(data.slot+1);break;}
  case 'pair':renderPair(data);break;
  case 'settings':$('#server').value=data.server;$('#hostKey').value=data.key;$('#internetEnabled').checked=data.enabled;$('#animation').checked=motion=data.animation;$('#settingsError').style.display='none';openDialog('settingsDialog');break;
  case 'saved':applyVisibility();$('#settingsDialog').close();$('#hostKey').value='';toast('saved');break;
  case 'copied':toast('copied');break;
  case 'visibility':document.body.classList.toggle('paused',!data.active||document.hidden);break;
  case 'error':if($('#settingsDialog').open){$('#settingsError').textContent=tr(data.key);$('#settingsError').style.display='block';}else toast(data.key);break;
 }
});
// Poll adapters only while a pairing window is visible; no QR data is logged or persisted.
setInterval(()=>{if($('#pairDialog').open&&pair.mode===2)requestPair();},3000);
send('ready');
