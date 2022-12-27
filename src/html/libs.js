/* eslint-disable max-len */
/* eslint-disable comma-dangle */
/* eslint-disable require-jsdoc */

// =========================================================

// Alert box design by Igor Ferrão de Souza: https://www.linkedin.com/in/igor-ferr%C3%A3o-de-souza-4122407b/

// eslint-disable-next-line no-unused-vars
function cuteAlert({
  type,
  title,
  message,
  buttonText = 'OK',
  confirmText = 'OK',
  cancelText = 'Cancel',
  closeStyle,
}) {
  return new Promise((resolve) => {
    setInterval(() => {}, 5000);
    const body = document.querySelector('body');

    let closeStyleTemplate = 'alert-close';
    if (closeStyle === 'circle') {
      closeStyleTemplate = 'alert-close-circle';
    }

    let btnTemplate = `<button class="alert-button ${type}-bg ${type}-btn mui-btn mui-btn--primary">${buttonText}</button>`;
    if (type === 'question') {
      btnTemplate = `
<div class="question-buttons">
  <button class="confirm-button error-bg error-btn mui-btn mui-btn--danger">${confirmText}</button>
  <button class="cancel-button question-bg question-btn mui-btn">${cancelText}</button>
</div>
`;
    }

    let svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 52 52" xmlns:v="https://vecta.io/nano">
<path d="M26 0C11.664 0 0 11.663 0 26s11.664 26 26 26 26-11.663 26-26S40.336 0 26 0zm0 50C12.767 50 2 39.233 2 26S12.767 2 26 2s24 10.767 24 24-10.767 24-24
24zm9.707-33.707a1 1 0 0 0-1.414 0L26 24.586l-8.293-8.293a1 1 0 0 0-1.414 1.414L24.586 26l-8.293 8.293a1 1 0 0 0 0 1.414c.195.195.451.293.707.293s.512-.098.707
-.293L26 27.414l8.293 8.293c.195.195.451.293.707.293s.512-.098.707-.293a1 1 0 0 0 0-1.414L27.414 26l8.293-8.293a1 1 0 0 0 0-1.414z"/>
</svg>
`;
    if (type === 'success') {
      svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 52 52" xmlns:v="https://vecta.io/nano">
<path d="M26 0C11.664 0 0 11.663 0 26s11.664 26 26 26 26-11.663 26-26S40.336 0 26 0zm0 50C12.767 50 2 39.233 2 26S12.767 2 26 2s24 10.767 24 24-10.767 24-24
24zm12.252-34.664l-15.369 17.29-9.259-7.407a1 1 0 0 0-1.249 1.562l10 8a1 1 0 0 0 1.373-.117l16-18a1 1 0 1 0-1.496-1.328z"/>
</svg>
`;
    }
    if (type === 'info') {
      svgTemplate = `
<svg class="alert-img" xmlns="http://www.w3.org/2000/svg" fill="#fff" viewBox="0 0 64 64" xmlns:v="https://vecta.io/nano">
<path d="M38.535 47.606h-4.08V28.447a1 1 0 0 0-1-1h-4.52a1 1 0 1 0 0 2h3.52v18.159h-5.122a1 1 0 1 0 0 2h11.202a1 1 0 1 0 0-2z"/>
<circle cx="32" cy="18" r="3"/><path d="M32 0C14.327 0 0 14.327 0 32s14.327 32 32 32 32-14.327 32-32S49.673 0 32 0zm0 62C15.458 62 2 48.542 2 32S15.458 2 32 2s30 13.458 30 30-13.458 30-30 30z"/>
</svg>
`;
    }

    const template = `
<div class="alert-wrapper">
  <div class="alert-frame">
    <div class="alert-header ${type}-bg">
      <span class="${closeStyleTemplate}">X</span>
      ${svgTemplate}
    </div>
    <div class="alert-body">
      <span class="alert-title">${title}</span>
      <span class="alert-message">${message}</span>
      ${btnTemplate}
    </div>
  </div>
</div>
`;

    body.insertAdjacentHTML('afterend', template);

    const alertWrapper = document.querySelector('.alert-wrapper');
    const alertFrame = document.querySelector('.alert-frame');
    const alertClose = document.querySelector(`.${closeStyleTemplate}`);

    function resolveIt() {
      alertWrapper.remove();
      resolve();
    }
    function confirmIt() {
      alertWrapper.remove();
      resolve('confirm');
    }
    function stopProp(e) {
      e.stopPropagation();
    }

    if (type === 'question') {
      const confirmButton = document.querySelector('.confirm-button');
      const cancelButton = document.querySelector('.cancel-button');

      confirmButton.addEventListener('click', confirmIt);
      cancelButton.addEventListener('click', resolveIt);
    } else {
      const alertButton = document.querySelector('.alert-button');

      alertButton.addEventListener('click', resolveIt);
    }

    alertClose.addEventListener('click', resolveIt);
    alertWrapper.addEventListener('click', resolveIt);
    alertFrame.addEventListener('click', stopProp);
  });
}


// =========================================================
// Autocomplete handler

// eslint-disable-next-line no-unused-vars
function autocomplete(inp, arr) {
  /* the autocomplete function takes two arguments,
  the text field element and an array of possible autocompleted values: */
  let currentFocus;

  /* execute a function when someone writes in the text field: */
  function handler(e) {
    let b;
    const val = this.value;

    /* close any already open lists of autocompleted values */
    closeAllLists();
    currentFocus = -1;
    /* create a DIV element that will contain the items (values): */
    const a = document.createElement('DIV');
    a.setAttribute('id', this.id + 'autocomplete-list');
    a.setAttribute('class', 'autocomplete-items');
    /* append the DIV element as a child of the autocomplete container: */
    this.parentNode.appendChild(a);
    /* for each item in the array... */
    for (let i = 0; i < arr.length; i++) {
      /* check if the item starts with the same letters as the text field value :*/
      if (arr[i].substr(0, val.length).toUpperCase() == val.toUpperCase()) {
        /* create a DIV element for each matching element: */
        b = document.createElement('DIV');
        /* make the matching letters bold :*/
        b.innerHTML = '<strong>' + arr[i].substr(0, val.length) + '</strong>';
        b.innerHTML += arr[i].substr(val.length);
        /* insert a input field that will hold the current array item's value: */
        b.innerHTML += '<input type="hidden" value="' + arr[i] + '">';
        /* execute a function when someone clicks on the item value (DIV element): */
        b.addEventListener('click', ((arg) => (e) => {
          /* insert the value for the autocomplete text field: */
          inp.value = arg.getElementsByTagName('input')[0].value;
          /* close the list of autocompleted values,
          (or any other open lists of autocompleted values: */
          closeAllLists();
        })(b));
        a.appendChild(b);
      }
    }
  }
  inp.addEventListener('input', handler);
  inp.addEventListener('click', handler);

  /* execute a function presses a key on the keyboard: */
  inp.addEventListener('keydown', (e) => {
    let x = _(this.id + 'autocomplete-list');
    if (x) x = x.getElementsByTagName('div');
    if (e.keyCode == 40) {
      /* If the arrow DOWN key is pressed,
      increase the currentFocus variable: */
      currentFocus++;
      /* and and make the current item more visible: */
      addActive(x);
    } else if (e.keyCode == 38) { // up
      /* If the arrow UP key is pressed,
      decrease the currentFocus variable: */
      currentFocus--;
      /* and and make the current item more visible: */
      addActive(x);
    } else if (e.keyCode == 13) {
      /* If the ENTER key is pressed, prevent the form from being submitted, */
      e.preventDefault();
      if (currentFocus > -1) {
        /* and simulate a click on the "active" item: */
        if (x) x[currentFocus].click();
      }
    }
  });
  function addActive(x) {
    /* a function to classify an item as "active": */
    if (!x) return false;
    /* start by removing the "active" class on all items: */
    removeActive(x);
    if (currentFocus >= x.length) currentFocus = 0;
    if (currentFocus < 0) currentFocus = (x.length - 1);
    /* add class "autocomplete-active": */
    x[currentFocus].classList.add('autocomplete-active');
  }
  function removeActive(x) {
    /* a function to remove the "active" class from all autocomplete items: */
    for (let i = 0; i < x.length; i++) {
      x[i].classList.remove('autocomplete-active');
    }
  }
  function closeAllLists(elmnt) {
    /* close all autocomplete lists in the document,
    except the one passed as an argument: */
    const x = document.getElementsByClassName('autocomplete-items');
    for (let i = 0; i < x.length; i++) {
      if (elmnt != x[i] && elmnt != inp) {
        x[i].parentNode.removeChild(x[i]);
      }
    }
  }
  /* execute a function when someone clicks in the document: */
  document.addEventListener('click', (e) => {
    closeAllLists(e.target);
  });
}
