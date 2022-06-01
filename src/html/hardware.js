/* eslint-disable no-unused-vars */
/* eslint-disable comma-dangle */
/* eslint-disable require-jsdoc */

document.addEventListener('DOMContentLoaded', loadData, false);

function _(el) {
  return document.getElementById(el);
}

function loadData() {
  xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const data = JSON.parse(this.responseText);
      updateHardwareSettings(data);
    }
  };
  xmlhttp.open('GET', '/hardware.json', true);
  xmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  xmlhttp.send();
}

function submitHardwareSettings() {
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/hardware.json');
  xhr.setRequestHeader('Content-Type', 'application/json');
  const formData = new FormData(_('upload_hardware'));
  xhr.send(JSON.stringify(Object.fromEntries(formData), function(k, v) {
    if (v === '') return undefined;
    if (_(k) && _(k).type == 'checkbox') {
      return v == 'on' ? true : false;
    }
    if (_(k) && _(k).classList.contains('array')) {
      const arr = v.split(',').map((element) => {
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
  return false;
}

function updateHardwareSettings(data) {
  for (const [key, value] of Object.entries(data)) {
    if (_(key)) {
      if (_(key).type == 'checkbox') {
        _(key).checked = value ? 'on' : 'off';
      } else {
        if (Array.isArray(value)) _(key).value = value.toString();
        else _(key).value = value;
      }
    }
  }
  if (data.customised) _('custom_config').style.display = 'block';
}

function fileDragHover(e) {
  e.stopPropagation();
  e.preventDefault();
  e.target.className = (e.type == 'dragover' ? 'hover' : '');
}

function fileSelectHandler(e) {
  fileDragHover(e);
  const files = e.target.files || e.dataTransfer.files;
  for (let i = 0, f; f = files[i]; i++) {
    parseFile(f);
  }
}

function parseFile(file) {
  const reader = new FileReader();
  reader.onload = function(e) {
    const data = JSON.parse(e.target.result);
    updateHardwareSettings(data);
  };
  reader.readAsText(file);
}

function init() {
  const fileselect = _('fileselect');
  const filedrag = _('filedrag');

  fileselect.addEventListener('change', fileSelectHandler, false);

  const xhr = new XMLHttpRequest();
  if (xhr.upload) {
    filedrag.addEventListener('dragover', fileDragHover, false);
    filedrag.addEventListener('dragleave', fileDragHover, false);
    filedrag.addEventListener('drop', fileSelectHandler, false);
    filedrag.style.display = 'block';
  }
}

if (window.File && window.FileList && window.FileReader) {
  init();
}

@@include("libs.js")
