document.addEventListener("DOMContentLoaded", load_data, false);

function _(el) {
    return document.getElementById(el);
}

function load_data() {
    var json_url = 'hardware';
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

function updateHardwareSettings(data) {
    for (let [key, value] of Object.entries(data)) {
        if (_(key)) {
            if (_(key).type == 'checkbox') {
                if (parseInt(value) > 0)
                    _(key).checked = true;
                else
                    _(key).checked = false;
            }
            else {
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
        var lines = e.target.result.split("\n");
        lines.forEach(line => {
            if (!line.startsWith(';') && line.trim().length>0) {
                var parts = line.split("=");
                key = parts[0];
                value = parts[1];
                try {
                    if (_(key).type == 'checkbox') {
                        if (parseInt(value) > 0)
                            _(key).checked = true;
                        else
                            _(key).checked = false;
                    }
                    else {
                        _(key).value = value;
                    }
                } catch(e) {
                    alert(key);
                    alert(e);
                }
            }
        });
    }
    reader.readAsText(file);
}

function Init() {
    var fileselect = _("fileselect"),
        filedrag = _("filedrag"),
        submitbutton = _("submitbutton");

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
