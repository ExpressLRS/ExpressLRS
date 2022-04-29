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

    let grp = document.createElement('DIV');
    grp.setAttribute('class', 'group');
    grp.innerHTML = htmlFields.join('');

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
