@@require(isTX)

/* eslint-disable comma-dangle */
/* eslint-disable max-len */
/* eslint-disable require-jsdoc */

let isTransmitter = false;

@@if isTX:
    isTransmitter = true;
@@end

let selectedVersion = null;

let versions = {
    "current_version": "3.1.1",
    "data": [{
        "name" : "latest",
        "version_major" : "3",
        "version_minor" : "1",
        "version_patch" : "1",
        "downloads" : [{
            "chip" : "esp8285",
            "mode" : "receiver",
            "radio" : "sx1280",
            "download" : "https://pkendall64.github.io/elrs-web-flasher/firmware/3.1.1/FCC/Unified_ESP8285_2400_RX/firmware.bin"
        }, {
            "chip" : "esp32",
            "mode" : "transmitter",
            "radio" : "sx1280",
            "download" : "https://pkendall64.github.io/elrs-web-flasher/firmware/3.1.1/FCC/Unified_ESP32_2400_TX/firmware.bin"
        }]
    }, {
        "name" : "v3.0.1",
        "version_major" : "3",
        "version_minor" : "0",
        "version_patch" : "1",
        "downloads" : [{
            "chip" : "esp8285",
            "mode" : "receiver",
            "radio" : "sx1280",
            "download" : "https://pkendall64.github.io/elrs-web-flasher/firmware/3.0.1/FCC/Unified_ESP8285_2400_RX/firmware.bin"
        }, {
            "chip" : "esp32",
            "mode" : "transmitter",
            "radio" : "sx1280",
            "download" : "https://pkendall64.github.io/elrs-web-flasher/firmware/3.0.1/FCC/Unified_ESP32_2400_TX/firmware.bin"
        }]
    }]
  }

async function initAutoUpdater() {
    /*
    versions = axios.get('versions.json');
    */

    const el = document.getElementById('au_version_select');

    for(let i = 0; i < versions.data.length; ++i) {
        const option = document.createElement('option');
        const val = `${versions.data[i].version_major}.${versions.data[i].version_minor}.${versions.data[i].version_patch}`;
        option.setAttribute('value', i);
        option.innerText = `v${val}`;
        el.appendChild(option);
    }
}

function changeSelectedVersion(event) {
    selectedVersion = event.target.value;
    if (!isNaN(parseInt(selectedVersion, 10))) {
        selectedVersion = versions.data[parseInt(selectedVersion, 10)];
        document.getElementById('au_update').disabled = false;
    } else {
        selectedVersion = null;
        document.getElementById('au_update').disabled = true;
    }
}

async function startAutoUpdate() {
    document.getElementById('au_update').disabled = true;

    const bin = await axios({
        url: selectedVersion.downloads.find(d => d.mode === (isTransmitter ? 'transmitter' : 'receiver')).download,
        method: 'GET',
        responseType: 'blob',
        onDownloadProgress: progress => {
            const percentCompleted = Math.floor(progress.loaded / progress.total * 100);
            document.getElementById('auProgressBar').value = percentCompleted;
            document.getElementById('auStatus').innerHTML = percentCompleted + '% downloaded... please wait';
        }
    }).finally(() => {
        document.getElementById('auProgressBar').value = 0;
        document.getElementById('auStatus').innerHTML = 'Firmware downloaded, continue with uploading.';
    });
    const local_file = new File([bin.data], 'firmware.bin');
    uploadFile(local_file, function() {
        document.getElementById('au_update').disabled = false;
    });
}

(function() {
    initAutoUpdater();
})();