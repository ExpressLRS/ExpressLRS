let storedModelId = 255;
let modeSelectionInit = true;

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
    const failsafeModeField = _(`pwm_${ch}_fsmode`);
    let failsafe = failsafeField.value;
    if (failsafe > 2011) failsafe = 2011;
    if (failsafe < 988) failsafe = 988;
    failsafeField.value = failsafe;
    let failsafeMode = failsafeModeField.value;

    const raw = (narrow << 19) | (mode << 15) | (invert << 14) | (inChannel << 10) | (failsafeMode << 20) | (failsafe - 988);
    // console.log(`PWM ${ch} mode=${mode} input=${inChannel} fs=${failsafe} fsmode=${failsafeMode} inv=${invert} nar=${narrow} raw=${raw}`);
    outData.push(raw);
    ++ch;
  }
  return outData;
}

function enumSelectGenerate(id, val, arOptions) {
  // Generate a <select> item with every option in arOptions, and select the val element (0-based)
  const retVal = `<div class="mui-select compact"><select id="${id}" class="pwmitm">` +
        arOptions.map((item, idx) => {
          if (item) return `<option value="${idx}"${(idx === val) ? ' selected' : ''} ${item === 'Disabled' ? 'disabled' : ''}>${item}</option>`;
          return '';
        }).join('') + '</select></div>';
  return retVal;
}

function generateFeatureBadges(features) {
  let str = '';
  if (!!(features & 1)) str += `<span style="color: #696969; background-color: #a8dcfa" class="badge">TX</span>`;
  else if (!!(features & 2)) str += `<span style="color: #696969; background-color: #d2faa8" class="badge">RX</span>`;
  if ((features & 12) === 12) str += `<span style="color: #696969; background-color: #fab4a8" class="badge">I2C</span>`;
  else if (!!(features & 4)) str += `<span style="color: #696969; background-color: #fab4a8" class="badge">SCL</span>`;
  else if (!!(features & 8)) str += `<span style="color: #696969; background-color: #fab4a8" class="badge">SDA</span>`;

  // Serial2
  if ((features & 96) === 96) str += `<span style="color: #696969; background-color: #36b5ff" class="badge">Serial2</span>`;
  else if (!!(features & 32)) str += `<span style="color: #696969; background-color: #36b5ff" class="badge">RX2</span>`;
  else if (!!(features & 64)) str += `<span style="color: #696969; background-color: #36b5ff" class="badge">TX2</span>`;

  return str;
}

function updatePwmSettings(arPwm) {
  if (arPwm === undefined) {
    if (_('model_tab')) _('model_tab').style.display = 'none';
    return;
  }
  var pinRxIndex = undefined;
  var pinTxIndex = undefined;
  var pinModes = []
  // arPwm is an array of raw integers [49664,50688,51200]. 10 bits of failsafe position, 4 bits of input channel, 1 bit invert, 4 bits mode, 1 bit for narrow/750us
  const htmlFields = ['<div class="mui-panel pwmpnl"><table class="pwmtbl mui-table"><tr><th class="fixed-column">Output</th><th class="mui--text-center fixed-column">Features</th><th>Mode</th><th>Input</th><th class="mui--text-center fixed-column">Invert?</th><th class="mui--text-center fixed-column">750us?</th><th class="mui--text-center fixed-column pwmitm">Failsafe Mode</th><th class="mui--text-center fixed-column pwmitm">Failsafe Pos</th></tr>'];
  arPwm.forEach((item, index) => {
    const failsafe = (item.config & 1023) + 988; // 10 bits
    const failsafeMode = (item.config >> 20) & 3; // 2 bits
    const ch = (item.config >> 10) & 15; // 4 bits
    const inv = (item.config >> 14) & 1;
    const mode = (item.config >> 15) & 15; // 4 bits
    const narrow = (item.config >> 19) & 1;
    const features = item.features;
    const modes = ['50Hz', '60Hz', '100Hz', '160Hz', '333Hz', '400Hz', '10KHzDuty', 'On/Off'];
    if (features & 16) {
      modes.push('DShot');
    } else {
      modes.push(undefined);
    }
    if (features & 1) {
      modes.push('Serial TX');
      modes.push(undefined);  // SCL
      modes.push(undefined);  // SDA
      modes.push(undefined);  // true PWM
      pinRxIndex = index;
    } else if (features & 2) {
      modes.push('Serial RX');
      modes.push(undefined);  // SCL
      modes.push(undefined);  // SDA
      modes.push(undefined);  // true PWM
      pinTxIndex = index;
    } else {
      modes.push(undefined);  // Serial
      if (features & 4) {
        modes.push('I2C SCL');
      } else {
        modes.push(undefined);
      }
      if (features & 8) {
        modes.push('I2C SDA');
      } else {
        modes.push(undefined);
      }
      modes.push(undefined);  // true PWM
    }

    if (features & 32) {
      modes.push('Serial2 RX');
    } else {
      modes.push(undefined);
    }
    if (features & 64) {
      modes.push('Serial2 TX');
    } else {
      modes.push(undefined);
    }

    const modeSelect = enumSelectGenerate(`pwm_${index}_mode`, mode, modes);
    const inputSelect = enumSelectGenerate(`pwm_${index}_ch`, ch,
        ['ch1', 'ch2', 'ch3', 'ch4',
          'ch5 (AUX1)', 'ch6 (AUX2)', 'ch7 (AUX3)', 'ch8 (AUX4)',
          'ch9 (AUX5)', 'ch10 (AUX6)', 'ch11 (AUX7)', 'ch12 (AUX8)',
          'ch13 (AUX9)', 'ch14 (AUX10)', 'ch15 (AUX11)', 'ch16 (AUX12)']);
    const failsafeModeSelect = enumSelectGenerate(`pwm_${index}_fsmode`, failsafeMode,
        ['Set Position', 'No Pulses', 'Last Position']); // match eServoOutputFailsafeMode
    htmlFields.push(`<tr><td class="mui--text-center mui--text-title">${index + 1}</td>
            <td>${generateFeatureBadges(features)}</td>
            <td>${modeSelect}</td>
            <td>${inputSelect}</td>
            <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_inv"${(inv) ? ' checked' : ''}></div></td>
            <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_nar"${(narrow) ? ' checked' : ''}></div></td>
            <td>${failsafeModeSelect}</td>
            <td><div class="mui-textfield compact"><input id="pwm_${index}_fs" value="${failsafe}" size="6" class="pwmitm" /></div></td></tr>`);
    pinModes[index] = mode;
  });
  htmlFields.push('</table></div>');

  const grp = document.createElement('DIV');
  grp.setAttribute('class', 'group');
  grp.innerHTML = htmlFields.join('');

  _('pwm').appendChild(grp);

  const setDisabled = (index, onoff) => {
    _(`pwm_${index}_ch`).disabled = onoff;
    _(`pwm_${index}_inv`).disabled = onoff;
    _(`pwm_${index}_nar`).disabled = onoff;
    _(`pwm_${index}_fs`).disabled = onoff;
    _(`pwm_${index}_fsmode`).disabled = onoff;
  }
  arPwm.forEach((item,index)=>{
    const pinMode = _(`pwm_${index}_mode`)
    pinMode.onchange = () => {
      setDisabled(index, pinMode.value > 9);
      const updateOthers = (value, enable) => {
        if (value > 9) { // disable others
          arPwm.forEach((item, other) => {
            if (other != index) {
              document.querySelectorAll(`#pwm_${other}_mode option`).forEach(opt => {
                if (opt.value == value) {
                  if (modeSelectionInit)
                    opt.disabled = true;
                  else
                    opt.disabled = enable;
                }
              });
            }
          })
        }
      }
      updateOthers(pinMode.value, true); // disable others
      updateOthers(pinModes[index], false); // enable others
      pinModes[index] = pinMode.value;

      // show Serial2 protocol selection only if Serial2 TX is assigned
      _('serial1-config').style.display = 'none';
      if (pinMode.value == 14) // Serial2 TX
        _('serial1-config').style.display = 'block';
    }
    pinMode.onchange();

    // disable and hide the failsafe position field if not using the set-position failsafe mode
    const failsafeMode = _(`pwm_${index}_fsmode`);
    failsafeMode.onchange = () => {
      const failsafeField = _(`pwm_${index}_fs`);
      if (failsafeMode.value == 0) {
        failsafeField.disabled = false;
        failsafeField.style.display = 'block';
      }
      else {
        failsafeField.disabled = true;
        failsafeField.style.display = 'none';
      }
    };
    failsafeMode.onchange();
  });

  modeSelectionInit = false;

  // put some constraints on pinRx/Tx mode selects
  if (pinRxIndex !== undefined && pinTxIndex !== undefined) {
    const pinRxMode = _(`pwm_${pinRxIndex}_mode`);
    const pinTxMode = _(`pwm_${pinTxIndex}_mode`);
    pinRxMode.onchange = () => {
      if (pinRxMode.value == 9) { // Serial
        pinTxMode.value = 9;
        setDisabled(pinRxIndex, true);
        setDisabled(pinTxIndex, true);
        pinTxMode.disabled = true;
        _('serial-config').style.display = 'block';
        _('baud-config').style.display = 'block';
      }
      else {
        pinTxMode.value = 0;
        setDisabled(pinRxIndex, false);
        setDisabled(pinTxIndex, false);
        pinTxMode.disabled = false;
        _('serial-config').style.display = 'none';
        _('baud-config').style.display = 'none';
      }
    }
    pinTxMode.onchange = () => {
      if (pinTxMode.value == 9) { // Serial
        pinRxMode.value = 9;
        setDisabled(pinRxIndex, true);
        setDisabled(pinTxIndex, true);
        pinTxMode.disabled = true;
        _('serial-config').style.display = 'block';
        _('baud-config').style.display = 'block';
      }
    }
    const pinTx = pinTxMode.value;
    pinRxMode.onchange();
    if (pinRxMode.value != 9) pinTxMode.value = pinTx;
  }
}

// =========================================================

if (!FEATURES.IS_TX)
    _('reset-model').addEventListener('click', postWithFeedback('Reset Model Settings', 'An error occurred resetting model settings', '/reset?model', null));


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
  // Force customised to true as this is now customising it
  formObject['customised'] = true;

  // Serialize and send the formObject
  xhr.send(JSON.stringify(formObject, function(k, v) {
    if (v === '') return undefined;
    if (_(k)) {
      if (_(k).type === 'color') return undefined;
      if (_(k).type === 'checkbox') return v === 'on';
      if (_(k).classList.contains('datatype-boolean')) return v === 'true';
      if (_(k).classList.contains('array')) {
        const arr = v.split(',').map((element) => {
          return Number(element);
        });
        return arr.length === 0 ? undefined : arr;
      }
    }
    if (typeof v === 'boolean') return v;
    if (v === 'true') return true;
    if (v === 'false') return false;
    return isNaN(v) ? v : +v;
  }));

  xhr.onreadystatechange = function() {
    if (this.readyState === 4) {
      if (this.status === 200) {
        cuteAlert({
          type: 'question',
          title: 'Upload Succeeded',
          message: 'Reboot to take effect',
          confirmText: 'Reboot',
          cancelText: 'Close'
        }).then((e) => {
            if (FEATURES.IS_TX) {
              originalUID = _('uid').value;
              originalUIDType = 'Overridden';
              _('phrase').value = '';
              updateUIDType(originalUIDType);
            }
        if (e === 'confirm') {
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/reboot');
            xhr.setRequestHeader('Content-Type', 'application/json');
            xhr.onreadystatechange = function() {};
            xhr.send();
          }
        });
      } else {
        cuteAlert({
          type: 'error',
          title: 'Upload Failed',
          message: this.responseText
        });
      }
    }
  };
}
