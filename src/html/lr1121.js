/* eslint-disable no-unused-vars */
/* eslint-disable comma-dangle */
/* eslint-disable require-jsdoc */

document.addEventListener('DOMContentLoaded', onReady, false);

function _(el) {
  return document.getElementById(el);
}

function onReady() {
  if (window.File && window.FileList && window.FileReader) {
    initFiledrag();
  }
  loadData();
}

function dec2hex(i, len) {
  return "0x" + (i+0x10000).toString(16).substr(-len).toUpperCase();
}

function loadData() {
  xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (this.readyState === 4 && this.status === 200) {
      const data = JSON.parse(this.responseText);
      _('radio_type').textContent = dec2hex(data['type'], 2)
      _('radio_hardware').textContent = dec2hex(data['hardware'], 2)
      _('radio_firmware').textContent = dec2hex(data['firmware'], 4)
    }
  };
  xmlhttp.open('GET', '/lr1121.json', true);
  xmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  xmlhttp.send();
}

function initFiledrag() {
  const fileselect = _('firmware_file');
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

function fileDragHover(e) {
  e.stopPropagation();
  e.preventDefault();
  if (e.target === _('filedrag')) e.target.className = (e.type === 'dragover' ? 'hover' : '');
}

function fileSelectHandler(e) {
  fileDragHover(e);
  const files = e.target.files || e.dataTransfer.files;
  uploadFile(files[0]);
}

function uploadFile(file) {
  _('upload_btn').disabled = true
  try {
    const formdata = new FormData();
    formdata.append('upload', file, file.name);
    const ajax = new XMLHttpRequest();
    ajax.upload.addEventListener('progress', progressHandler, false);
    ajax.addEventListener('load', completeHandler, false);
    ajax.addEventListener('error', errorHandler, false);
    ajax.addEventListener('abort', abortHandler, false);
    ajax.open('POST', '/lr1121');
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
      if (percent === 100) {
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
        if (this.readyState === 4) {
          _('status').innerHTML = '';
          _('progressBar').value = 0;
          if (this.status === 200) {
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

@@include("libs.js")
