/* eslint-disable max-len, require-jsdoc */
document.addEventListener('DOMContentLoaded', init, false);

function _(el) {
  return document.getElementById(el);
}

const cwFreq = 2440000000;
const xtalNominal = 52000000;

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
  _('optionsRadios1').disabled = true;
  _('optionsRadios2').disabled = true;
  const xmlhttp = new XMLHttpRequest();
  xmlhttp.open('POST', '/cw', true);
  xmlhttp.onreadystatechange = function() {};
  const formdata = new FormData;
  formdata.append('radio', _('optionsRadios1').checked ? 1 : 2);
  xmlhttp.send(formdata);
};

_('measured').onchange = (e) => {
  const calc = (e.target.value/cwFreq)*xtalNominal;
  _('calculated').innerHTML = Math.round(calc);
  _('offset').innerHTML = Math.round(calc - xtalNominal) / 1000;
  _('ppm').innerHTML = Math.abs(Math.round(calc - xtalNominal)) / 52;
  const rawShift = Math.round(e.target.value - cwFreq);
  _('raw').innerHTML = rawShift / 1000;
  let icon;
  if (Math.abs(rawShift) < 90000) {
    icon = `<svg version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" x="0px" y="0px" width="64" height="64" viewBox="0 0 96 96" enable-background="new 0 0 96 96" xml:space="preserve"><g><path fill-rule="evenodd" clip-rule="evenodd" fill="#6BBE66" d="M48,0c26.51,0,48,21.49,48,48S74.51,96,48,96S0,74.51,0,48 S21.49,0,48,0L48,0z M26.764,49.277c0.644-3.734,4.906-5.813,8.269-3.79c0.305,0.182,0.596,0.398,0.867,0.646l0.026,0.025 c1.509,1.446,3.2,2.951,4.876,4.443l1.438,1.291l17.063-17.898c1.019-1.067,1.764-1.757,3.293-2.101 c5.235-1.155,8.916,5.244,5.206,9.155L46.536,63.366c-2.003,2.137-5.583,2.332-7.736,0.291c-1.234-1.146-2.576-2.312-3.933-3.489 c-2.35-2.042-4.747-4.125-6.701-6.187C26.993,52.809,26.487,50.89,26.764,49.277L26.764,49.277z"/></g></svg>`;
  } else if (Math.abs(rawShift) < 180000) {
    icon = `
    <svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 20 20">
      <style type="text/css">.cls-1{ fill: #fc3 }</style>
      <path class="cls-1" d="M19.64 16.36L11.53 2.3A1.85 1.85 0 0 0 10 1.21 1.85 1.85 0 0 0 8.48 2.3L.36 16.36C-.48 17.81.21 19 1.88 19h16.24c1.67 0 2.36-1.19 1.52-2.64zM11 16H9v-2h2zm0-4H9V6h2z"/>
    </svg>`;
  } else {
    icon = `
      <svg xmlns="http://www.w3.org/2000/svg"  width="64" height="64" viewBox="0 0 122.88 122.88">
      <defs><style>.cls-1{fill:#eb0100;}.cls-1,.cls-2{fill-rule:evenodd;}.cls-2{fill:#fff;}</style></defs>
      <path class="cls-1" d="M61.44,0A61.44,61.44,0,1,1,0,61.44,61.44,61.44,0,0,1,61.44,0Z"/><path class="cls-2" d="M35.38,49.72c-2.16-2.13-3.9-3.47-1.19-6.1l8.74-8.53c2.77-2.8,4.39-2.66,7,0L61.68,46.86,73.39,35.15c2.14-2.17,3.47-3.91,6.1-1.2L88,42.69c2.8,2.77,2.66,4.4,0,7L76.27,61.44,88,73.21c2.65,2.58,2.79,4.21,0,7l-8.54,8.74c-2.63,2.71-4,1-6.1-1.19L61.68,76,49.9,87.81c-2.58,2.64-4.2,2.78-7,0l-8.74-8.53c-2.71-2.63-1-4,1.19-6.1L47.1,61.44,35.38,49.72Z"/>
    </svg>`;
  }
  _('tldr').innerHTML = icon;
  _('result').style.display = 'block';
};
