// eslint-disable-next-line no-unused-vars
export function autocomplete(inp, arr) {
    /* the autocomplete function takes two arguments,
    the text field element and an array of possible autocompleted values: */
    let currentFocus;

    /* execute a function when someone writes in the text field: */
    function handler(e) {
        let b;
        const val = this.value;

        e.stopPropagation();
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
            if (val.length === 0 || arr[i].substr(0, val.length).toUpperCase() === val.toUpperCase()) {
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
        let x = document.getElementById(inp.id + 'autocomplete-list');
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
