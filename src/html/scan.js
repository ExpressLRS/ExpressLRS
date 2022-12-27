document.addEventListener("DOMContentLoaded", get_mode, false);
var scanTimer = undefined;

function _(el) {
    return document.getElementById(el);
}

function getPwmFormData()
{
    let ch = 0;
    let inField;
    let outData = [];
    while (inField = _(`pwm_${ch}_ch`))
    {
        let inChannel = inField.value;
        let invert = _(`pwm_${ch}_inv`).checked ? 1 : 0;
        let failsafeField = _(`pwm_${ch}_fs`);
        let failsafe = failsafeField.value;
        if (failsafe > 2011) failsafe = 2011;
        if (failsafe < 988) failsafe = 988;
        failsafeField.value = failsafe;

        let raw = (invert << 14) | (inChannel << 10) | (failsafe - 988);
        //console.log(`PWM ${ch} input=${inChannel} fs=${failsafe} inv=${invert} raw=${raw}`);
        outData.push(raw);
        ++ch;
    }

    let outForm = new FormData();
    outForm.append('pwm', outData.join(','));
    return outForm;
}

function updatePwmSettings(arPwm)
{
    if (arPwm === undefined)
        return;
    // arPwm is an array of raw integers [49664,50688,51200]. 10 bits of failsafe position, 4 bits of input channel, 1 bit invert
    let htmlFields = ['<table class="pwmtbl"><tr><th>Output</th><th>Input</th><th>Invert?</th><th>Failsafe</th></tr>'];
    arPwm.forEach((item, index) => {
        let failsafe = (item & 1023) + 988; // 10 bits
        let ch = (item >> 10) & 15; // 4 bits
        let inv = (item >> 14) & 1;
        htmlFields.push(`<tr><th>${index+1}</th><td><select id="pwm_${index}_ch">
          <option value="0"${(ch===0) ? ' selected' : ''}>ch1</option>
          <option value="1"${(ch===1) ? ' selected' : ''}>ch2</option>
          <option value="2"${(ch===2) ? ' selected' : ''}>ch3</option>
          <option value="3"${(ch===3) ? ' selected' : ''}>ch4</option>
          <option value="4"${(ch===4) ? ' selected' : ''}>ch5 (AUX1)</option>
          <option value="5"${(ch===5) ? ' selected' : ''}>ch6 (AUX2)</option>
          <option value="6"${(ch===6) ? ' selected' : ''}>ch7 (AUX3)</option>
          <option value="7"${(ch===7) ? ' selected' : ''}>ch8 (AUX4)</option>
          <option value="8"${(ch===8) ? ' selected' : ''}>ch9 (AUX5)</option>
          <option value="9"${(ch===9) ? ' selected' : ''}>ch10 (AUX6)</option>
          <option value="10"${(ch===10) ? ' selected' : ''}>ch11 (AUX7)</option>
          <option value="11"${(ch===11) ? ' selected' : ''}>ch12 (AUX8)</option>
        </select></td><td><input type="checkbox" id="pwm_${index}_inv"${(inv) ? ' checked' : ''}></td>
        <td><input id="pwm_${index}_fs" value="${failsafe}" size="4"/></td></tr>`);
    });
    htmlFields.push('<tr><td colspan="4"><input type="submit" value="Set PWM Output"></td></tr></table>');

<<<<<<< HEAD
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
=======
    let grp = document.createElement('DIV');
    grp.setAttribute('class', 'group');
    grp.innerHTML = htmlFields.join('');
>>>>>>> parent of 4fb6474b (Merge branch 'master' of https://github.com/SunjunKim/ExpressLRS)

    _('pwm').appendChild(grp);
    _('pwm').addEventListener('submit', callback('Set PWM Output', 'Unknown error', '/pwm', getPwmFormData));
    _('pwm_container').style.display = 'block';
}

function get_mode() {
    var json_url = 'mode.json';
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            var data = JSON.parse(this.responseText);
            if (data.mode==="STA") {
                _('stamode').style.display = 'block';
                _('ssid').textContent = data.ssid;
            } else {
                _('apmode').style.display = 'block';
                if (data.ssid) {
                    _('homenet').textContent = data.ssid;
                } else {
                    _('connect').style.display = 'none';
                }
                scanTimer = setInterval(get_networks, 2000);
            }
            if (data.modelid !== undefined)
                _('modelid').value = data.modelid;
            updatePwmSettings(data.pwm);
        }
    };
    xmlhttp.open("POST", json_url, true);
    xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xmlhttp.send();
}

function get_networks() {
    var json_url = 'networks.json';
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            var data = JSON.parse(this.responseText);
            _('loader').style.display = 'none';
            autocomplete(_('network'), data);
            clearInterval(scanTimer);
        }
    };
    xmlhttp.open("POST", json_url, true);
    xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xmlhttp.send();
}

function hasErrorParameter() {
    var tmp = [], result = false;
    location.search
        .substr(1)
        .split("&")
        .forEach(function (item) {
            tmp = item.split("=");
            if (tmp[0] === "error") result = true;
        });
    return result;
}

function show(elements, specifiedDisplay) {
    elements = elements.length ? elements : [elements];
    for (var index = 0; index < elements.length; index++) {
        elements[index].style.display = specifiedDisplay || 'block';
    }
}

var elements = document.querySelectorAll('#failed');
if (hasErrorParameter()) show(elements);

function autocomplete(inp, arr) {
    /*the autocomplete function takes two arguments,
    the text field element and an array of possible autocompleted values:*/
    var currentFocus;

    /*execute a function when someone writes in the text field:*/
    function handler(e) {
        var a, b, i, val = this.value;
        /*close any already open lists of autocompleted values*/
        closeAllLists();
        currentFocus = -1;
        /*create a DIV element that will contain the items (values):*/
        a = document.createElement("DIV");
        a.setAttribute("id", this.id + "autocomplete-list");
        a.setAttribute("class", "autocomplete-items");
        /*append the DIV element as a child of the autocomplete container:*/
        this.parentNode.appendChild(a);
        /*for each item in the array...*/
        for (i = 0; i < arr.length; i++) {
            /*check if the item starts with the same letters as the text field value:*/
            if (arr[i].substr(0, val.length).toUpperCase() == val.toUpperCase()) {
                /*create a DIV element for each matching element:*/
                b = document.createElement("DIV");
                /*make the matching letters bold:*/
                b.innerHTML = "<strong>" + arr[i].substr(0, val.length) + "</strong>";
                b.innerHTML += arr[i].substr(val.length);
                /*insert a input field that will hold the current array item's value:*/
                b.innerHTML += "<input type='hidden' value='" + arr[i] + "'>";
                /*execute a function when someone clicks on the item value (DIV element):*/
                b.addEventListener("click", ((arg) => (e) => {
                    /*insert the value for the autocomplete text field:*/
                    inp.value = arg.getElementsByTagName("input")[0].value;
                    /*close the list of autocompleted values,
                    (or any other open lists of autocompleted values:*/
                    closeAllLists();
                })(b));
                a.appendChild(b);
            }
        }
    }
    inp.addEventListener("input", handler);
    inp.addEventListener("click", handler);

    /*execute a function presses a key on the keyboard:*/
    inp.addEventListener("keydown", (e) => {
        var x = _(this.id + "autocomplete-list");
        if (x) x = x.getElementsByTagName("div");
        if (e.keyCode == 40) {
            /*If the arrow DOWN key is pressed,
            increase the currentFocus variable:*/
            currentFocus++;
            /*and and make the current item more visible:*/
            addActive(x);
        } else if (e.keyCode == 38) { //up
            /*If the arrow UP key is pressed,
            decrease the currentFocus variable:*/
            currentFocus--;
            /*and and make the current item more visible:*/
            addActive(x);
        } else if (e.keyCode == 13) {
            /*If the ENTER key is pressed, prevent the form from being submitted,*/
            e.preventDefault();
            if (currentFocus > -1) {
                /*and simulate a click on the "active" item:*/
                if (x) x[currentFocus].click();
            }
        }
    });
    function addActive(x) {
        /*a function to classify an item as "active":*/
        if (!x) return false;
        /*start by removing the "active" class on all items:*/
        removeActive(x);
        if (currentFocus >= x.length) currentFocus = 0;
        if (currentFocus < 0) currentFocus = (x.length - 1);
        /*add class "autocomplete-active":*/
        x[currentFocus].classList.add("autocomplete-active");
    }
    function removeActive(x) {
        /*a function to remove the "active" class from all autocomplete items:*/
        for (var i = 0; i < x.length; i++) {
            x[i].classList.remove("autocomplete-active");
        }
    }
    function closeAllLists(elmnt) {
        /*close all autocomplete lists in the document,
        except the one passed as an argument:*/
        var x = document.getElementsByClassName("autocomplete-items");
        for (var i = 0; i < x.length; i++) {
            if (elmnt != x[i] && elmnt != inp) {
                x[i].parentNode.removeChild(x[i]);
            }
        }
    }
    /*execute a function when someone clicks in the document:*/
    document.addEventListener("click", (e) => {
        closeAllLists(e.target);
    });
}

//=========================================================

function uploadFile() {
    var file = _("firmware_file").files[0];
    var formdata = new FormData();
    formdata.append("upload", file, file.name);
    var ajax = new XMLHttpRequest();
    ajax.upload.addEventListener("progress", progressHandler, false);
    ajax.addEventListener("load", completeHandler, false);
    ajax.addEventListener("error", errorHandler, false);
    ajax.addEventListener("abort", abortHandler, false);
    ajax.open("POST", "/update");
    ajax.send(formdata);
}

function progressHandler(event) {
    //_("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total;
    var percent = Math.round((event.loaded / event.total) * 100);
    _("progressBar").value = percent;
    _("status").innerHTML = percent + "% uploaded... please wait";
}

function completeHandler(event) {
    _("status").innerHTML = "";
    _("progressBar").value = 0;
    var data = JSON.parse(event.target.responseText);
    if (data.status === 'ok') {
        function show_message() {
            cuteAlert({
                type: 'success',
                title: "Update Succeeded",
                message: data.msg
            });
        }
        // This is basically a delayed display of the success dialog with a fake progress
        var percent = 0;
        var interval = setInterval(()=>{
            percent = percent + 2;
            _("progressBar").value = percent;
            _("status").innerHTML = percent + "% flashed... please wait";
            if (percent == 100) {
                clearInterval(interval);
                _("status").innerHTML = "";
                _("progressBar").value = 0;
                show_message();
            }
        }, 100);
    } else if (data.status === 'mismatch') {
        cuteAlert({
            type: 'question',
            title: "Targets Mismatch",
            message: data.msg,
            confirmText: "Flash anyway",
            cancelText: "Cancel"
        }).then((e)=>{
            xmlhttp = new XMLHttpRequest();
            xmlhttp.onreadystatechange = function () {
                if (this.readyState == 4) {
                    _("status").innerHTML = "";
                    _("progressBar").value = 0;
                    if (this.status == 200) {
                        var data = JSON.parse(this.responseText);
                        cuteAlert({
                            type: "info",
                            title: "Force Update",
                            message: data.msg
                        });
                    }
                    else {
                        cuteAlert({
                            type: "error",
                            title: "Force Update",
                            message: "An error occurred trying to force the update"
                        });
                    }
                }
            };
            xmlhttp.open("POST", "/forceupdate", true);
            var data = new FormData();
            data.append("action", e);
            xmlhttp.send(data);
        });
    } else {
        cuteAlert({
            type: 'error',
            title: "Update Failed",
            message: data.msg
        });
    }
}

function errorHandler(event) {
    _("status").innerHTML = "";
    _("progressBar").value = 0;
    cuteAlert({
        type: "error",
        title: "Update Failed",
        message: event.target.responseText
    });
}

function abortHandler(event) {
    _("status").innerHTML = "";
    _("progressBar").value = 0;
    cuteAlert({
        type: "info",
        title: "Update Aborted",
        message: event.target.responseText
    });
}

_('upload_form').addEventListener('submit', (e) => {
    e.preventDefault();
    uploadFile();
});

//=========================================================

<<<<<<< HEAD
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
=======
function callback(title, msg, url, getdata) {
    return function(e) {
        e.stopPropagation();
        e.preventDefault();
        xmlhttp = new XMLHttpRequest();
        xmlhttp.onreadystatechange = function () {
            if (this.readyState == 4) {
                if (this.status == 200) {
                    cuteAlert({
                        type: "info",
                        title: title,
                        message: this.responseText
                    });
                }
                else {
                    cuteAlert({
                        type: "error",
                        title: title,
                        message: msg
                    });
                }
            }
        };
        xmlhttp.open("POST", url, true);
        if (getdata) data = getdata();
        else data = null;
        xmlhttp.send(data);
>>>>>>> parent of 4fb6474b (Merge branch 'master' of https://github.com/SunjunKim/ExpressLRS)
    }
}

_('sethome').addEventListener('submit', callback("Set Home Network", "An error occurred setting the home network", "/sethome", function() {
    return new FormData(_('sethome'));
}));
_('connect').addEventListener('click', callback("Connect to Home Network", "An error occurred connecting to the Home network", "/connect", null));
_('access').addEventListener('click', callback("Access Point", "An error occurred starting the Access Point", "/access", null));
_('forget').addEventListener('click', callback("Forget Home Network", "An error occurred forgetting the home network", "/forget", null));
if (_('modelmatch') != undefined) {
    _('modelmatch').addEventListener('submit', callback("Set Model Match", "An error occurred updating the model match number", "/model",
        () => { return new FormData(_('modelmatch')); }));
}

//=========================================================

// Alert box design by Igor Ferrão de Souza: https://www.linkedin.com/in/igor-ferr%C3%A3o-de-souza-4122407b/

function cuteAlert({
    type,
    title,
    message,
    buttonText = "OK",
    confirmText = "OK",
    cancelText = "Cancel",
    closeStyle,
  }) {
    return new Promise((resolve) => {
      setInterval(() => {}, 5000);
      const body = document.querySelector("body");

      const scripts = document.getElementsByTagName("script");

      let closeStyleTemplate = "alert-close";
      if (closeStyle === "circle") {
        closeStyleTemplate = "alert-close-circle";
      }

      let btnTemplate = `<button class="alert-button ${type}-bg ${type}-btn">${buttonText}</button>`;
      if (type === "question") {
        btnTemplate = `
<div class="question-buttons">
  <button class="confirm-button error-bg error-btn">${confirmText}</button>
  <button class="cancel-button question-bg question-btn">${cancelText}</button>
</div>
`;
      }

      let svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 52 52" xmlns:v="https://vecta.io/nano">
<path d="M26 0C11.664 0 0 11.663 0 26s11.664 26 26 26 26-11.663 26-26S40.336 0 26 0zm0 50C12.767 50 2 39.233 2 26S12.767 2 26 2s24 10.767 24 24-10.767 24-24
24zm9.707-33.707a1 1 0 0 0-1.414 0L26 24.586l-8.293-8.293a1 1 0 0 0-1.414 1.414L24.586 26l-8.293 8.293a1 1 0 0 0 0 1.414c.195.195.451.293.707.293s.512-.098.707
-.293L26 27.414l8.293 8.293c.195.195.451.293.707.293s.512-.098.707-.293a1 1 0 0 0 0-1.414L27.414 26l8.293-8.293a1 1 0 0 0 0-1.414z"/>
</svg>
`;
      if (type === "success") {
        svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 52 52" xmlns:v="https://vecta.io/nano">
<path d="M26 0C11.664 0 0 11.663 0 26s11.664 26 26 26 26-11.663 26-26S40.336 0 26 0zm0 50C12.767 50 2 39.233 2 26S12.767 2 26 2s24 10.767 24 24-10.767 24-24
24zm12.252-34.664l-15.369 17.29-9.259-7.407a1 1 0 0 0-1.249 1.562l10 8a1 1 0 0 0 1.373-.117l16-18a1 1 0 1 0-1.496-1.328z"/>
</svg>
`;
      }
      if (type === "info") {
        svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 64 64" xmlns:v="https://vecta.io/nano">
<path d="M38.535 47.606h-4.08V28.447a1 1 0 0 0-1-1h-4.52a1 1 0 1 0 0 2h3.52v18.159h-5.122a1 1 0 1 0 0 2h11.202a1 1 0 1 0 0-2z"/>
<circle cx="32" cy="18" r="3"/><path d="M32 0C14.327 0 0 14.327 0 32s14.327 32 32 32 32-14.327 32-32S49.673 0 32 0zm0 62C15.458 62 2 48.542 2 32S15.458 2 32 2s30 13.458 30 30-13.458 30-30 30z"/>
</svg>
`;
      }

      const template = `
<div class="alert-wrapper">
  <div class="alert-frame">
    <div class="alert-header ${type}-bg">
      <span class="${closeStyleTemplate}">X</span>
      ${svgTemplate}
    </div>
    <div class="alert-body">
      <span class="alert-title">${title}</span>
      <span class="alert-message">${message}</span>
      ${btnTemplate}
    </div>
  </div>
</div>
`;

      body.insertAdjacentHTML("afterend", template);

      const alertWrapper = document.querySelector(".alert-wrapper");
      const alertFrame = document.querySelector(".alert-frame");
      const alertClose = document.querySelector(`.${closeStyleTemplate}`);

      function resolveIt() {
        alertWrapper.remove();
        resolve();
      }
      function confirmIt() {
        alertWrapper.remove();
        resolve("confirm");
      }
      function stopProp(e) {
        e.stopPropagation();
      }

      if (type === "question") {
        const confirmButton = document.querySelector(".confirm-button");
        const cancelButton = document.querySelector(".cancel-button");

        confirmButton.addEventListener("click", confirmIt);
          cancelButton.addEventListener("click", resolveIt);
      } else {
        const alertButton = document.querySelector(".alert-button");

        alertButton.addEventListener("click", resolveIt);
      }

      alertClose.addEventListener("click", resolveIt);
      alertWrapper.addEventListener("click", resolveIt);
      alertFrame.addEventListener("click", stopProp);
    });
  }
