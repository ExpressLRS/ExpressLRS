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
        let mode = _(`pwm_${ch}_mode`).value;
        let invert = _(`pwm_${ch}_inv`).checked ? 1 : 0;
        let narrow = _(`pwm_${ch}_nar`).checked ? 1 : 0;
        let failsafeField = _(`pwm_${ch}_fs`);
        let failsafe = failsafeField.value;
        if (failsafe > 2011) failsafe = 2011;
        if (failsafe < 988) failsafe = 988;
        failsafeField.value = failsafe;

        let raw = (narrow << 19) | (mode << 15) | (invert << 14) | (inChannel << 10) | (failsafe - 988);
        //console.log(`PWM ${ch} mode=${mode} input=${inChannel} fs=${failsafe} inv=${invert} nar=${narrow} raw=${raw}`);
        outData.push(raw);
        ++ch;
    }

    let outForm = new FormData();
    outForm.append('pwm', outData.join(','));
    return outForm;
}

function enumSelectGenerate(id, val, arOptions)
{
    // Generate a <select> item with every option in arOptions, and select the val element (0-based)
    let retVal = `<div class="mui-select"><select id="${id}">` +
        arOptions.map((item, idx) => {
            return `<option value="${idx}"${(idx == val) ? ' selected' : ''}>${item}</option>`;
        }).join('') + '</select></div>';
    return retVal;
}

function updatePwmSettings(arPwm)
{
    if (arPwm === undefined) {
        if(_('pwm_tab')) _('pwm_tab').style.display = 'none';
        return;
    }
    // arPwm is an array of raw integers [49664,50688,51200]. 10 bits of failsafe position, 4 bits of input channel, 1 bit invert, 4 bits mode, 1 bit for narrow/750us
    let htmlFields = ['<div class="mui-panel"><table class="pwmtbl mui-table"><tr><th class="mui--text-center">Output</th><th>Mode</th><th>Input</th><th class="mui--text-center">Invert?</th><th class="mui--text-center">750us?</th><th>Failsafe</th></tr>'];
    arPwm.forEach((item, index) => {
        let failsafe = (item & 1023) + 988; // 10 bits
        let ch = (item >> 10) & 15; // 4 bits
        let inv = (item >> 14) & 1;
        let mode = (item >> 15) & 15; // 4 bits
        let narrow = (item >> 19) & 1;
        let modeSelect = enumSelectGenerate(`pwm_${index}_mode`, mode,
            ['50Hz', '60Hz', '100Hz', '160Hz', '333Hz', '400Hz', 'On/Off']);
        let inputSelect = enumSelectGenerate(`pwm_${index}_ch`, ch,
            ['ch1', 'ch2', 'ch3', 'ch4',
             'ch5 (AUX1)', 'ch6 (AUX2)', 'ch7 (AUX3)', 'ch8 (AUX4)',
             'ch9 (AUX5)', 'ch10 (AUX6)', 'ch11 (AUX7)', 'ch12 (AUX8)',
             'ch13 (AUX9)', 'ch14 (AUX10)', 'ch15 (AUX11)', 'ch16 (AUX12)']);
        htmlFields.push(`<tr><th class="mui--text-center">${index+1}</th>
            <td>${modeSelect}</td>
            <td>${inputSelect}</td>
            <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_inv"${(inv) ? ' checked' : ''}></div></td>
            <td><div class="mui-checkbox mui--text-center"><input type="checkbox" id="pwm_${index}_nar"${(narrow) ? ' checked' : ''}></div></td>
            <td><div class="mui-textfield"><input id="pwm_${index}_fs" value="${failsafe}" size="6"/></div></td></tr>`
        );
    });
    htmlFields.push('</table></div><input type="submit" class="mui-btn mui-btn--primary" value="Set PWM Output">');

    let grp = document.createElement('DIV');
    grp.setAttribute('class', 'group');
    grp.innerHTML = htmlFields.join('');

    _('pwm').appendChild(grp);
    _('pwm').addEventListener('submit', callback('Set PWM Output', 'Unknown error', '/pwm', getPwmFormData));
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
            if (data.modelid)
                _('modelid').value = data.modelid;
            if (data.product_name)
                _('product_name').textContent = data.product_name;
            if (data.reg_domain)
                _('reg_domain').textContent = data.reg_domain;
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
            if (data.length > 0) {
                _('loader').style.display = 'none';
                autocomplete(_('network'), data);
                clearInterval(scanTimer);
            }
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
    ajax.setRequestHeader("X-FileSize", file.size);
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

@@include("libs.js")
