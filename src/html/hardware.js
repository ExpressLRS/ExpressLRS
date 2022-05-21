document.addEventListener("DOMContentLoaded", load_data, false);

function _(el) {
    return document.getElementById(el);
}

function load_data() {
    var json_url = '/hardware.json';
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            var data = JSON.parse(this.responseText);
            updateHardwareSettings(data);
        }
    };
    xmlhttp.open("GET", json_url, true);
    xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xmlhttp.send();
}

function submitHardwareSettings() {
    var xhr = new XMLHttpRequest();
    xhr.open('POST','/hardware.json')
    xhr.setRequestHeader("Content-Type", "application/json");
    var formData = new FormData(_("upload_hardware"));
    xhr.send(JSON.stringify(Object.fromEntries(formData), function(k, v){
        if (v === '') return undefined;
        if (_(k) && _(k).type == 'checkbox') {
            return v == 'on' ? true : false;
        }
        if (k.endsWith('_values')) {
            const arr = v.split(',').map(element => {
                return Number(element);
            });
            return arr.length == 0 ? undefined : arr;
        }
        return isNaN(v) ? v : +v;
    }));
    xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            cuteAlert({
                type: 'question',
                title: "Upload Succeeded",
                message: "Reboot to take effect",
                confirmText: "Reboot",
                cancelText: "Close"
            }).then((e) => {
                if (e == "confirm") {
                    var xhr = new XMLHttpRequest();
                    xhr.open('POST', '/reboot')
                    xhr.setRequestHeader("Content-Type", "application/json");
                    xhr.onreadystatechange = function () {}
                    xhr.send();
                }
            });
        }
    };
    return false;
}

function updateHardwareSettings(data) {
    for (let [key, value] of Object.entries(data)) {
        if (_(key)) {
            if (_(key).type == 'checkbox') {
                _(key).checked = value ? "on" : "off";
            }
            else {
                if (Array.isArray(value))
                    _(key).value = value.toString();
                else
                    _(key).value = value;
            }
        }
    }
}

function FileDragHover(e) {
    e.stopPropagation();
    e.preventDefault();
    e.target.className = (e.type == "dragover" ? "hover" : "");
}

function FileSelectHandler(e) {
    FileDragHover(e);
    var files = e.target.files || e.dataTransfer.files;
    for (var i = 0, f; f = files[i]; i++) {
        ParseFile(f);
    }
}

function ParseFile(file) {
    var reader = new FileReader();
    reader.onload = function(e) {
        var data = JSON.parse(e.target.result);
        updateHardwareSettings(data);
    }
    reader.readAsText(file);
}

function Init() {
    var fileselect = _("fileselect"),
        filedrag = _("filedrag");

    fileselect.addEventListener("change", FileSelectHandler, false);

    var xhr = new XMLHttpRequest();
    if (xhr.upload) {
        filedrag.addEventListener("dragover", FileDragHover, false);
        filedrag.addEventListener("dragleave", FileDragHover, false);
        filedrag.addEventListener("drop", FileSelectHandler, false);
        filedrag.style.display = "block";
    }
}

if (window.File && window.FileList && window.FileReader) {
    Init();
}

@@include("libs.js")
