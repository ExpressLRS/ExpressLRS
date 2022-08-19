/* eslint-disable require-jsdoc */
document.addEventListener('DOMContentLoaded', init, false);

function _(el) {
  return document.getElementById(el);
}

const cwFreq = 2440000000;
const xtal = 52000000;

function init() {
  const xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      const data = JSON.parse(this.responseText);
      if (data.radios == 2) {
        _('radioOption').style.display = 'block';
      }
      _('start-cw').disabled = false;
    }
  };
  xmlhttp.open('GET', '/cw', true);
  xmlhttp.send();
}

_('start-cw').onclick = (e) => {
  e.stopPropagation();
  e.preventDefault();
  _('start-cw').disabled = true;
  const xmlhttp = new XMLHttpRequest();
  xmlhttp.open('POST', '/cw', true);
  xmlhttp.onreadystatechange = function() {};
  const formdata = new FormData;
  formdata.append('radio', _('optionsRadios1').checked ? 1 : 2);
  xmlhttp.send(formdata);
};

_('measured').onchange = (e) => {
  const calc = (e.target.value/cwFreq)*xtal;
  _('calculated').innerHTML = Math.round(calc);
  _('offset').innerHTML = Math.round(calc - xtal) / 1000;
  _('result').style.display = 'block';
};
