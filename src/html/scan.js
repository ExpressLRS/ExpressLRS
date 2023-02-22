@@require(isTX)

/* eslint-disable comma-dangle */
/* eslint-disable max-len */
/* eslint-disable require-jsdoc */

document.addEventListener('DOMContentLoaded', init, false);
let colorTimer = undefined;
let colorUpdated  = false;
let storedModelId = 255;
let buttonActions = [];

function _(el) {
  return document.getElementById(el);
}

function getPwmFormData() {
  let ch = 0;
  let inField;
  const outData = [];
  while (inField = _(`pwm_${ch}_ch`)) {
    const inChannel = inField.value;
    const mode = _(`pwm_${ch}_mode`).value;
    const invert = _(`pwm_${ch}_inv`).checked ? 1 : 0;
    const narrow = _(`pwm_${ch}_nar`).checked ? 1 : 0;
    const failsafeField = _(`pwm_${ch}_fs`);
    let failsafe = failsafeField.value;
    if (failsafe > 2011) failsafe = 2011;
    if (failsafe < 988) failsafe = 988;
    failsafeField.value = failsafe;

    const raw = (narrow << 19) | (mode << 15) | (invert << 14) | (inChannel << 10) | (failsafe - 988);
    // console.log(`PWM ${ch} mode=${mode} input=${inChannel} fs=${failsafe} inv=${invert} nar=${narrow} raw=${raw}`);
    outData.push(raw);
    ++ch;
  }

  const outForm = new FormData();
  outForm.append('pwm', outData.join(','));
  return outForm;
}

function enumSelectGenerate(id, val, arOptions) {
  // Generate a <select> item with every option in arOptions, and select the val element (0-based)
  const retVal = `<div class="mui-select"><select id="${id}">` +
        arOptions.map((item, idx) => {
          return `<option value="${idx}"${(idx == val) ? ' selected' : ''}>${item}</option>`;
        }).join('') + '</select></div>';
  return retVal;
}

function updatePwmSettings(arPwm) {
  if (arPwm === undefined) {
    if (_('pwm_tab')) _('pwm_tab').style.display = 'none';
    return;
  }
  // arPwm is an array of raw integers [49664,50688,51200]. 10 bits of failsafe position, 4 bits of input channel, 1 bit invert, 4 bits mode, 1 bit for narrow/750us
  const htmlFields = ['<div class="mui-panel"><table class="pwmtbl mui-table"><tr><th class="mui--text-center">Output</th><th>Mode</th><th>Input</th><th class="mui--text-center">Invert?</th><th class="mui--text-center">750us?</th><th>Failsafe</th></tr>'];
  arPwm.forEach((item, index) => {
    const failsafe = (item & 1023) + 988; // 10 bits
    const ch = (item >> 10) & 15; // 4 bits
    const inv = (item >> 14) & 1;
    const mode = (item >> 15) & 15; // 4 bits
    const narrow = (item >> 19) & 1;
    const modeSelect = enumSelectGenerate(`pwm_${index}_mode`, mode,
        ['50Hz', '60Hz', '100Hz', '160Hz', '333Hz', '400Hz', '10KHzDuty', 'On/Off']);
    const inputSelect = enumSelectGenerate(`pwm_${index}_ch`, ch,
        ['ch1', 'ch2', 'ch3', 'ch4',
          'ch5 (AUX1)', 'ch6 (AUX2)', 'ch7 (AUX3)', 'ch8 (AUX4)',
          'ch9 (AUX5)', 'ch10 (AUX6)', 'ch11 (AUX7)', 'ch12 (AUX8)',
          'ch13 (AUX9)', 'ch14 (AUX10)', 'ch15 (AUX11)', 'ch16 (AUX12)']);
    htmlFields.push(`<tr><th class="mui--text-center">${index+1}</th>
            <td>${modeSelect}</td>
            <td>${inputSelect}</td>
            <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_inv"${(inv) ? ' checked' : ''}></div></td>
            <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_nar"${(narrow) ? ' checked' : ''}></div></td>
            <td><div class="mui-textfield"><input id="pwm_${index}_fs" value="${failsafe}" size="6"/></div></td></tr>`);
  });
  htmlFields.push('</table></div><button type="submit" class="mui-btn mui-btn--primary">Set PWM Output</button>');

  const grp = document.createElement('DIV');
  grp.setAttribute('class', 'group');
  grp.innerHTML = htmlFields.join('');

@@if not isTX:
  _('pwm').appendChild(grp);
  _('pwm').addEventListener('submit', callback('Set PWM Output', 'Unknown error', '/pwm', getPwmFormData));
@@end
}

function init() {
  // setup network radio button handling
  _('nt0').onclick = () => _('credentials').style.display = 'block';
  _('nt1').onclick = () => _('credentials').style.display = 'block';
  _('nt2').onclick = () => _('credentials').style.display = 'none';
  _('nt3').onclick = () => _('credentials').style.display = 'none';
@@if not isTX:
  // setup model match checkbox handler
  _('model-match').onclick = () => {
    if (_('model-match').checked) {
      _('modelid').style.display = 'block';
      if (storedModelId == 255) {
        _('modelid').value = '';
      } else {
        _('modelid').value = storedModelId;
      }
    } else {
      _('modelid').style.display = 'none';
      _('modelid').value = '255';
    }
  };
@@end
  initOptions();
}

function changeCurrentColors() {
  if (colorTimer === undefined) {
    sendCurrentColors();
    colorTimer = setInterval(timeoutCurrentColors, 50);
  } else {
    colorUpdated = true;
  }
}

function sendCurrentColors() {
  const formData = new FormData(_('upload_options'));
  const data = Object.fromEntries(formData);
  colors = [];
  for (const [k, v] of Object.entries(data)) {
    if (_(k) && _(k).type == 'color') {
      const index = parseInt(k.substring('6')) - 1;
      if (_(k + '-div').style.display === 'none') colors[index] = -1;
      else colors[index] = parseInt(v.substring(1), 16);
    }
  }
  const xmlhttp = new XMLHttpRequest();
  xmlhttp.open('POST', '/buttons', true);
  xmlhttp.setRequestHeader('Content-type', 'application/json');
  xmlhttp.send(JSON.stringify(colors));
  colorUpdated = false;
}

function timeoutCurrentColors() {
  if (colorUpdated) {
    sendCurrentColors();
  } else {
    clearInterval(colorTimer);
    colorTimer = undefined;
  }
}

function updateConfig(data) {
  if (data.mode==='STA') {
    _('stamode').style.display = 'block';
    _('ssid').textContent = data.ssid;
  } else {
    _('apmode').style.display = 'block';
  }
@@if not isTX:
  if (data.hasOwnProperty('modelid') && data.modelid != 255) {
    _('modelid').style.display = 'block';
    _('model-match').checked = true;
    storedModelId = data.modelid;
  } else {
    _('modelid').style.display = 'none';
    _('model-match').checked = false;
    storedModelId = 255;
  }
  _('modelid').value = storedModelId;

  _('force-tlm').checked = data.hasOwnProperty('forcetlm') && data.forcetlm;
@@end
  if (data.product_name) _('product_name').textContent = data.product_name;
  if (data.reg_domain) _('reg_domain').textContent = data.reg_domain;
  if (data.uid) _('uid').value = data.uid.toString();
  if (data.uidtype) _('uid-type').textContent = data.uidtype;
  updatePwmSettings(data.pwm);
@@if isTX:
  if (data.hasOwnProperty['button-colors']) {
    if (_('button1-color')) _('button1-color').oninput = changeCurrentColors;
    if (data['button-colors'][0] === -1) _('button1-color-div').style.display = 'none';
    else _('button1-color').value = color(data['button-colors'][0]);

    if (_('button2-color')) _('button2-color').oninput = changeCurrentColors;
    if (data['button-colors'][1] === -1) _('button2-color-div').style.display = 'none';
    else _('button2-color').value = color(data['button-colors'][1]);
  }
  if (data.hasOwnProperty('button-actions')) {
    updateButtons(data['button-actions']);
  } else {
    _('button-tab').style.display = 'none';
  }
  if (data['has-highpower'] === true) _('has-highpower').style.display = 'block';
@@end
}

function initOptions() {
  initBindingPhraseGen();
  const xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const data = JSON.parse(this.responseText);
      updateOptions(data['options']);
      updateConfig(data['config']);
      setTimeout(getNetworks, 2000);
    }
  };
  xmlhttp.open('GET', '/config', true);
  xmlhttp.send();
}

function getNetworks() {
  const xmlhttp = new XMLHttpRequest();
  xmlhttp.onload = function() {
    if (this.status == 204) {
      setTimeout(getNetworks, 2000);
    } else {
      console.log(this.responseText);
      const data = JSON.parse(this.responseText);
      if (data.length > 0) {
        _('loader').style.display = 'none';
        autocomplete(_('network'), data);
      }
    }
  };
  xmlhttp.onerror = function() {
    setTimeout(getNetworks, 2000);
  };
  xmlhttp.open('GET', 'networks.json', true);
  xmlhttp.send();
}

// =========================================================

function uploadFile() {
  _('upload_btn').disabled = true
  try {
    const file = _('firmware_file').files[0];
    const formdata = new FormData();
    formdata.append('upload', file, file.name);
    const ajax = new XMLHttpRequest();
    ajax.upload.addEventListener('progress', progressHandler, false);
    ajax.addEventListener('load', completeHandler, false);
    ajax.addEventListener('error', errorHandler, false);
    ajax.addEventListener('abort', abortHandler, false);
    ajax.open('POST', '/update');
    ajax.setRequestHeader('X-FileSize', file.size);
    ajax.send(formdata);
  }
  catch (e) {
    _('upload_btn').disabled = false
  }
}

function progressHandler(event) {
  // _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total;
  const percent = Math.round((event.loaded / event.total) * 100);
  _('progressBar').value = percent;
  _('status').innerHTML = percent + '% uploaded... please wait';
}

function completeHandler(event) {
  _('status').innerHTML = '';
  _('progressBar').value = 0;
  _('upload_btn').disabled = false
  const data = JSON.parse(event.target.responseText);
  if (data.status === 'ok') {
    function showMessage() {
      cuteAlert({
        type: 'success',
        title: 'Update Succeeded',
        message: data.msg
      });
    }
    // This is basically a delayed display of the success dialog with a fake progress
    let percent = 0;
    const interval = setInterval(()=>{
      percent = percent + 2;
      _('progressBar').value = percent;
      _('status').innerHTML = percent + '% flashed... please wait';
      if (percent == 100) {
        clearInterval(interval);
        _('status').innerHTML = '';
        _('progressBar').value = 0;
        showMessage();
      }
    }, 100);
  } else if (data.status === 'mismatch') {
    cuteAlert({
      type: 'question',
      title: 'Targets Mismatch',
      message: data.msg,
      confirmText: 'Flash anyway',
      cancelText: 'Cancel'
    }).then((e)=>{
      const xmlhttp = new XMLHttpRequest();
      xmlhttp.onreadystatechange = function() {
        if (this.readyState == 4) {
          _('status').innerHTML = '';
          _('progressBar').value = 0;
          if (this.status == 200) {
            const data = JSON.parse(this.responseText);
            cuteAlert({
              type: 'info',
              title: 'Force Update',
              message: data.msg
            });
          } else {
            cuteAlert({
              type: 'error',
              title: 'Force Update',
              message: 'An error occurred trying to force the update'
            });
          }
        }
      };
      xmlhttp.open('POST', '/forceupdate', true);
      const data = new FormData();
      data.append('action', e);
      xmlhttp.send(data);
    });
  } else {
    cuteAlert({
      type: 'error',
      title: 'Update Failed',
      message: data.msg
    });
  }
}

function errorHandler(event) {
  _('status').innerHTML = '';
  _('progressBar').value = 0;
  _('upload_btn').disabled = false
  cuteAlert({
    type: 'error',
    title: 'Update Failed',
    message: event.target.responseText
  });
}

function abortHandler(event) {
  _('status').innerHTML = '';
  _('progressBar').value = 0;
  _('upload_btn').disabled = false
  cuteAlert({
    type: 'info',
    title: 'Update Aborted',
    message: event.target.responseText
  });
}

_('upload_form').addEventListener('submit', (e) => {
  e.preventDefault();
  uploadFile();
});

_('fileselect').addEventListener('change', (e) => {
  const files = e.target.files || e.dataTransfer.files;
  const reader = new FileReader();
  reader.onload = function(x) {
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
      _('fileselect').value = '';
      if (this.readyState == 4) {
        if (this.status == 200) {
          cuteAlert({
            type: 'info',
            title: 'Upload Model Configuration',
            message: this.responseText
          });
        } else {
          cuteAlert({
            type: 'error',
            title: 'Upload Model Configuration',
            message: 'An error occurred while uploading model configuration file'
          });
        }
      }
    };
    xmlhttp.open('POST', '/import', true);
    xmlhttp.setRequestHeader('Content-Type', 'application/json');
    xmlhttp.send(x.target.result);
  }
  reader.readAsText(files[0]);
}, false);

// =========================================================

function callback(title, msg, url, getdata, success) {
  return function(e) {
    e.stopPropagation();
    e.preventDefault();
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
      if (this.readyState == 4) {
        if (this.status == 200) {
          if (success) success();
          cuteAlert({
            type: 'info',
            title: title,
            message: this.responseText
          });
        } else {
          cuteAlert({
            type: 'error',
            title: title,
            message: msg
          });
        }
      }
    };
    xmlhttp.open('POST', url, true);
    if (getdata) data = getdata();
    else data = null;
    xmlhttp.send(data);
  };
}

function setupNetwork(event) {
  if (_('nt0').checked) {
    callback('Set Home Network', 'An error occurred setting the home network', '/sethome?save', function() {
      return new FormData(_('sethome'));
    }, function() {
      _('wifi-ssid').value = _('network').value;
      _('wifi-password').value = _('password').value;
    })(event);
  }
  if (_('nt1').checked) {
    callback('Connect To Network', 'An error occurred connecting to the network', '/sethome', function() {
      return new FormData(_('sethome'));
    })(event);
  }
  if (_('nt2').checked) {
    callback('Start Access Point', 'An error occurred starting the Access Point', '/access', null)(event);
  }
  if (_('nt3').checked) {
    callback('Forget Home Network', 'An error occurred forgetting the home network', '/forget', null)(event);
  }
}

@@if not isTX:
_('reset-model').addEventListener('click', callback('Reset Model Settings', 'An error occurred reseting model settings', '/reset?model', null));
@@end
_('reset-options').addEventListener('click', callback('Reset Runtime Options', 'An error occurred reseting runtime options', '/reset?options', null));

_('sethome').addEventListener('submit', setupNetwork);
_('connect').addEventListener('click', callback('Connect to Home Network', 'An error occurred connecting to the Home network', '/connect', null));
if (_('modelmatch') != undefined) {
  _('modelmatch').addEventListener('submit', callback('Set Model Match', 'An error occurred updating the model match number', '/model',
      () => {
        return new FormData(_('modelmatch'));
      }));
}
if (_('forcetlm') != undefined) {
  _('forcetlm').addEventListener('submit', callback('Set force telemetry', 'An error occurred updating the force telemetry setting', '/forceTelemetry',
    () => {
      return new FormData(_('forcetlm'));
    }));
}

function submitOptions(e) {
  e.stopPropagation();
  e.preventDefault();
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/options.json');
  xhr.setRequestHeader('Content-Type', 'application/json');
  // Convert the DOM element into a JSON object containing the form elements
  const formElem = _('upload_options');
  const formObject = Object.fromEntries(new FormData(formElem));
  // Add in all the unchecked checkboxes which will be absent from a FormData object
  formElem.querySelectorAll('input[type=checkbox]:not(:checked)').forEach((k) => formObject[k.name] = false);

  // Serialize and send the formObject
  xhr.send(JSON.stringify(formObject, function(k, v) {
    if (v === '') return undefined;
    if (_(k) && _(k).type == 'color') return undefined;
    if (_(k) && _(k).type == 'checkbox') {
      return v == 'on' ? true : false;
    }
    if (_(k) && _(k).classList.contains('array')) {
      const arr = v.split(',').map((element) => {
        return Number(element);
      });
      return arr.length == 0 ? undefined : arr;
    }
    if (typeof v === 'boolean') return v;
    return isNaN(v) ? v : +v;
  }));

  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      cuteAlert({
        type: 'question',
        title: 'Upload Succeeded',
        message: 'Reboot to take effect',
        confirmText: 'Reboot',
        cancelText: 'Close'
      }).then((e) => {
        if (e == 'confirm') {
          const xhr = new XMLHttpRequest();
          xhr.open('POST', '/reboot');
          xhr.setRequestHeader('Content-Type', 'application/json');
          xhr.onreadystatechange = function() {};
          xhr.send();
        }
      });
    }
  };
}

_('submit-options').addEventListener('click', submitOptions);

@@if isTX:
function submitButtonActions(e) {
  e.stopPropagation();
  e.preventDefault();
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/config');
  xhr.setRequestHeader('Content-Type', 'application/json');
  // put in the colors
  buttonActions[0].color = to8bit(_(`button1-color`).value)
  buttonActions[1].color = to8bit(_(`button2-color`).value)
  xhr.send(JSON.stringify({'button-actions': buttonActions}));

  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 204) {
      cuteAlert({
        type: 'info',
        title: 'Success',
        message: 'Button actions have been saved'
      });
    }
  };
}
_('submit-actions').addEventListener('click', submitButtonActions);
@@end

function updateOptions(data) {
  for (const [key, value] of Object.entries(data)) {
    if (_(key)) {
      if (_(key).type == 'checkbox') {
        _(key).checked = value;
      } else {
        if (Array.isArray(value)) _(key).value = value.toString();
        else _(key).value = value;
      }
    }
  }
  if (data['wifi-ssid']) _('homenet').textContent = data['wifi-ssid'];
  else _('connect').style.display = 'none';
  if (data['customised']) _('reset-options').style.display = 'block';
}

@@if isTX:
function toRGB(c)
{
  r = c & 0xE0 ;
  r = ((r << 16) + (r << 13) + (r << 10)) & 0xFF0000;
  g = c & 0x1C;
  g = ((g<< 11) + (g << 8) + (g << 5)) & 0xFF00;
  b = ((c & 0x3) << 1) + ((c & 0x3) >> 1);
  b = (b << 5) + (b << 2) + (b >> 1);
  s = (r+g+b).toString(16);
  return '#' + "000000".substring(0, 6-s.length) + s;
}

function updateButtons(data) {
  buttonActions = data;
  for (const [b, _v] of Object.entries(data)) {
    for (const [p, v] of Object.entries(_v['action'])) {
      appendRow(parseInt(b), parseInt(p), v);
    }
    _(`button${parseInt(b)+1}-color-div`).style.display = 'block';
    _(`button${parseInt(b)+1}-color`).value = toRGB(_v['color']);
  }
  _('button1-color').oninput = changeCurrentColors;
  _('button2-color').oninput = changeCurrentColors;
}

function changeCurrentColors() {
  if (colorTimer === undefined) {
    sendCurrentColors();
    colorTimer = setInterval(timeoutCurrentColors, 50);
  } else {
    colorUpdated = true;
  }
}

function to8bit(v)
{
  v = parseInt(v.substring(1), 16)
  return ((v >> 16) & 0xE0) + ((v >> (8+3)) & 0x1C) + ((v >> 6) & 0x3)
}

function sendCurrentColors() {
  const formData = new FormData(_('button_actions'));
  const data = Object.fromEntries(formData);
  colors = [];
  for (const [k, v] of Object.entries(data)) {
    if (_(k) && _(k).type == 'color') {
      const index = parseInt(k.substring('6')) - 1;
      if (_(k + '-div').style.display === 'none') colors[index] = -1;
      else colors[index] = to8bit(v);
    }
  }
  const xmlhttp = new XMLHttpRequest();
  xmlhttp.open('POST', '/buttons', true);
  xmlhttp.setRequestHeader('Content-type', 'application/json');
  xmlhttp.send(JSON.stringify(colors));
  colorUpdated = false;
}

function timeoutCurrentColors() {
  if (colorUpdated) {
    sendCurrentColors();
  } else {
    clearInterval(colorTimer);
    colorTimer = undefined;
  }
}

function checkEnableButtonActionSave() {
  let disable = false;
  for (const [b, _v] of Object.entries(buttonActions)) {
    for (const [p, v] of Object.entries(_v['action'])) {
      if (v['action'] !== 0 && (_(`select-press-${b}-${p}`).value === '' || _(`select-long-${b}-${p}`).value === '' || _(`select-short-${b}-${p}`).value === '')) {
        disable = true;
      }
    }
  }
  _('submit-actions').disabled = disable;
}

function changeAction(b, p, value) {
  buttonActions[b]['action'][p]['action'] = value;
  if (value === 0) {
    _(`select-press-${b}-${p}`).value = '';
    _(`select-long-${b}-${p}`).value = '';
    _(`select-short-${b}-${p}`).value = '';
  }
  checkEnableButtonActionSave();
}

function changePress(b, p, value) {
  buttonActions[b]['action'][p]['is-long-press'] = (value==='true');
  _(`mui-long-${b}-${p}`).style.display = value==='true' ? 'block' : 'none';
  _(`mui-short-${b}-${p}`).style.display = value==='true' ? 'none' : 'block';
  checkEnableButtonActionSave();
}

function changeCount(b, p, value) {
  buttonActions[b]['action'][p]['count'] = parseInt(value);
  _(`select-long-${b}-${p}`).value = value;
  _(`select-short-${b}-${p}`).value = value;
  checkEnableButtonActionSave();
}

function appendRow(b,p,v) {
  const row = _('button-actions').insertRow();
  row.innerHTML = `
<td>
  Button ${parseInt(b)+1}
</td>
<td>
  <div class="mui-select">
    <select onchange="changeAction(${b}, ${p}, parseInt(this.value));">
      <option value='0' ${v['action']===0 ? 'selected' : ''}>Unused</option>
      <option value='1' ${v['action']===1 ? 'selected' : ''}>Increase Power</option>
      <option value='2' ${v['action']===2 ? 'selected' : ''}>Go to VTX Band Menu</option>
      <option value='3' ${v['action']===3 ? 'selected' : ''}>Go to VTX Channel Menu</option>
      <option value='4' ${v['action']===4 ? 'selected' : ''}>Send VTX Settings</option>
      <option value='5' ${v['action']===5 ? 'selected' : ''}>Start WiFi</option>
      <option value='6' ${v['action']===6 ? 'selected' : ''}>Enter Binding Mode</option>
    </select>
    <label>Action</label>
  </div>
</td>
<td>
  <div class="mui-select">
    <select id="select-press-${b}-${p}" onchange="changePress(${b}, ${p}, this.value);">
      <option value='' disabled hidden ${v['action']===0 ? 'selected' : ''}></option>
      <option value='false' ${v['is-long-press']===false ? 'selected' : ''}>Short press (click)</option>
      <option value='true' ${v['is-long-press']===true ? 'selected' : ''}>Long press (hold)</option>
    </select>
    <label>Press</label>
  </div>
</td>
<td>
  <div class="mui-select" id="mui-long-${b}-${p}" style="display:${buttonActions[b]['action'][p]['is-long-press'] ? "block": "none"};">
    <select id="select-long-${b}-${p}" onchange="changeCount(${b}, ${p}, this.value);">
      <option value='' disabled hidden ${v['action']===0 ? 'selected' : ''}></option>
      <option value='0' ${v['count']===0 ? 'selected' : ''}>for 0.5 seconds</option>
      <option value='1' ${v['count']===1 ? 'selected' : ''}>for 1 second</option>
      <option value='2' ${v['count']===2 ? 'selected' : ''}>for 1.5 seconds</option>
      <option value='3' ${v['count']===3 ? 'selected' : ''}>for 2 seconds</option>
      <option value='4' ${v['count']===4 ? 'selected' : ''}>for 2.5 seconds</option>
      <option value='5' ${v['count']===5 ? 'selected' : ''}>for 3 seconds</option>
      <option value='6' ${v['count']===6 ? 'selected' : ''}>for 3.5 seconds</option>
      <option value='7' ${v['count']===7 ? 'selected' : ''}>for 4 seconds</option>
    </select>
    <label>Count</label>
  </div>
  <div class="mui-select" id="mui-short-${b}-${p}" style="display:${buttonActions[b]['action'][p]['is-long-press'] ? "none": "block"};">
    <select id="select-short-${b}-${p}" onchange="changeCount(${b}, ${p}, this.value);">
      <option value='' disabled hidden ${v['action']===0 ? 'selected' : ''}></option>
      <option value='0' ${v['count']===0 ? 'selected' : ''}>1 time</option>
      <option value='1' ${v['count']===1 ? 'selected' : ''}>2 times</option>
      <option value='2' ${v['count']===2 ? 'selected' : ''}>3 times</option>
      <option value='3' ${v['count']===3 ? 'selected' : ''}>4 times</option>
      <option value='4' ${v['count']===4 ? 'selected' : ''}>5 times</option>
      <option value='5' ${v['count']===5 ? 'selected' : ''}>6 times</option>
      <option value='6' ${v['count']===6 ? 'selected' : ''}>7 times</option>
      <option value='7' ${v['count']===7 ? 'selected' : ''}>8 times</option>
    </select>
    <label>Count</label>
  </div>
</td>
`
}
@@end

md5 = function() {
  const k = [];
  let i = 0;

  for (; i < 64;) {
    k[i] = 0 | (Math.abs(Math.sin(++i)) * 4294967296);
  }

  function calcMD5(str) {
    let b; let c; let d; let j;
    const x = [];
    const str2 = unescape(encodeURI(str));
    let a = str2.length;
    const h = [b = 1732584193, c = -271733879, ~b, ~c];
    let i = 0;

    for (; i <= a;) x[i >> 2] |= (str2.charCodeAt(i) || 128) << 8 * (i++ % 4);

    x[str = (a + 8 >> 6) * 16 + 14] = a * 8;
    i = 0;

    for (; i < str; i += 16) {
      a = h; j = 0;
      for (; j < 64;) {
        a = [
          d = a[3],
          ((b = a[1] | 0) +
            ((d = (
              (a[0] +
                [
                  b & (c = a[2]) | ~b & d,
                  d & b | ~d & c,
                  b ^ c ^ d,
                  c ^ (b | ~d)
                ][a = j >> 4]
              ) +
              (k[j] +
                (x[[
                  j,
                  5 * j + 1,
                  3 * j + 5,
                  7 * j
                ][a] % 16 + i] | 0)
              )
            )) << (a = [
              7, 12, 17, 22,
              5, 9, 14, 20,
              4, 11, 16, 23,
              6, 10, 15, 21
            ][4 * a + j++ % 4]) | d >>> 32 - a)
          ),
          b,
          c
        ];
      }
      for (j = 4; j;) h[--j] = h[j] + a[j];
    }

    str = [];
    for (; j < 32;) str.push(((h[j >> 3] >> ((1 ^ j++ & 7) * 4)) & 15) * 16 + ((h[j >> 3] >> ((1 ^ j++ & 7) * 4)) & 15));

    return new Uint8Array(str);
  }
  return calcMD5;
}();

function uidBytesFromText(text) {
  const bindingPhraseFull = `-DMY_BINDING_PHRASE="${text}"`;
  const bindingPhraseHashed = md5(bindingPhraseFull);
  const uidBytes = bindingPhraseHashed.subarray(0, 6);

  return uidBytes;
}

function initBindingPhraseGen() {
  const uid = _('uid');

  function setOutput(text) {
    const uidBytes = uidBytesFromText(text);
    uid.value = uidBytes;
  }

  function updateValue(e) {
    setOutput(e.target.value);
  }

  phrase.addEventListener('input', updateValue);
  setOutput('');
}

@@include("libs.js")
