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

_('reset').addEventListener('click', postWithFeedback('Reset LR1121 Firmware', 'An error occurred resetting the custom firmware flag', '/reset?lr1121', null))

function loadData() {
  let xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (this.readyState === 4 && this.status === 200) {
      const data = JSON.parse(this.responseText);
      if (data['manual']) _('manual_upload').style.display = 'block';
      _('radio_type1').textContent = dec2hex(data['radio1']['type'], 2)
      _('radio_hardware1').textContent = dec2hex(data['radio1']['hardware'], 2)
      _('radio_firmware1').textContent = dec2hex(data['radio1']['firmware'], 4)
      if (data['radio2']) {
        _('radios').style.display='block'
        _('radio_type2').textContent = dec2hex(data['radio2']['type'], 2)
        _('radio_hardware2').textContent = dec2hex(data['radio2']['hardware'], 2)
        _('radio_firmware2').textContent = dec2hex(data['radio2']['firmware'], 4)
      }
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
  mui.overlay('on', {
    'keyboard': false,
    'static': true
  })
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
    ajax.setRequestHeader('X-Radio', document.querySelector("input[name=optionsRadio]:checked").value);
    ajax.send(formdata);
  }
  catch (e) {
    _('upload_btn').disabled = false
    mui.overlay('off')
  }
}

function progressHandler(event) {
  // _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total;
  const percent = Math.round((event.loaded / event.total) * 100);
  _('progressBar').value = percent;
  _('status').innerHTML = percent + '% uploaded... please wait';
}

async function completeHandler(event) {
  _('status').innerHTML = '';
  _('progressBar').value = 0;
  const data = JSON.parse(event.target.responseText);
  if (data.status === 'ok') {
    // This is basically a delayed display of the success dialog with a fake progress
    let percent = 0;
    const interval = setInterval(async ()=>{
      percent = percent + 2;
      _('progressBar').value = percent;
      _('status').innerHTML = percent + '% flashed... please wait';
      if (percent === 100) {
        clearInterval(interval);
        _('status').innerHTML = '';
        _('progressBar').value = 0;
        _('upload_btn').disabled = false
        mui.overlay('off')
        await cuteAlert({
          type: 'success',
          title: 'Update Succeeded',
          message: data.msg
        });
      }
    }, 100);
  } else {
    _('upload_btn').disabled = false
    mui.overlay('off')
    await cuteAlert({
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
  mui.overlay('off')
  return cuteAlert({
    type: 'error',
    title: 'Update Failed',
    message: event.target.responseText
  });
}

function abortHandler(event) {
  _('status').innerHTML = '';
  _('progressBar').value = 0;
  _('upload_btn').disabled = false
  mui.overlay('off')
  return cuteAlert({
    type: 'info',
    title: 'Update Aborted',
    message: event.target.responseText
  });
}

@@include("libs.js")
