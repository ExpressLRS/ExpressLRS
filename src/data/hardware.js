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