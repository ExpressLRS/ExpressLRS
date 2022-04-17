document.addEventListener("DOMContentLoaded", init, false);

function _(el) {
    return document.getElementById(el);
}

function init() {
    initBindingPhraseGen();
    var json_url = 'options';
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var data = JSON.parse(this.responseText);
            updateOptions(data);
        }
    };
    xmlhttp.open("GET", json_url, true);
    xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xmlhttp.send();
}

function updateOptions(data) {
    for (let [key, value] of Object.entries(data)) {
        if (_(key)) {
            if (_(key).type == 'checkbox') {
                _(key).checked = value;
            }
            else {
                if (Array.isArray(value))
                    _(key).value = value.toString();
                else
                    _(key).value = value;
            }
        }
    }
}

md5 = function(){

  var k = [], i = 0;

  for(; i < 64; ){
    k[i] = 0|(Math.abs(Math.sin(++i)) * 4294967296);
  }

  function calcMD5(str){
    var b, c, d, j,
        x = [],
        str2 = unescape(encodeURI(str)),
        a = str2.length,
        h = [b = 1732584193, c = -271733879, ~b, ~c],
        i = 0;

    for(; i <= a; ) x[i >> 2] |= (str2.charCodeAt(i)||128) << 8 * (i++ % 4);

    x[str = (a + 8 >> 6) * 16 + 14] = a * 8;
    i = 0;

    for(; i < str; i += 16){
      a = h; j = 0;
      for(; j < 64; ){
        a = [
          d = a[3],
          ((b = a[1]|0) +
            ((d = (
              (a[0] +
                [
                  b & (c = a[2]) | ~b&d,
                  d & b | ~d & c,
                  b ^ c ^ d,
                  c ^ (b | ~d)
                ][a = j >> 4]
              ) +
              (k[j] +
                (x[[
                  j,
                  5 * j + 1,
                  3 * j + 5,
                  7 * j
                ][a] % 16 + i]|0)
              )
            )) << (a = [
              7, 12, 17, 22,
              5,  9, 14, 20,
              4, 11, 16, 23,
              6, 10, 15, 21
            ][4 * a + j++ % 4]) | d >>> 32 - a)
          ),
          b,
          c
        ];
      }
      for(j = 4; j; ) h[--j] = h[j] + a[j];
    }

    str = [];
    for(; j < 32; ) str.push(((h[j >> 3] >> ((1 ^ j++ & 7) * 4)) & 15) * 16 + ((h[j >> 3] >> ((1 ^ j++ & 7) * 4)) & 15));

    return new Uint8Array(str);
  }
  return calcMD5;
}();

function uidBytesFromText(text) {
  const bindingPhraseFull = `-DMY_BINDING_PHRASE="${text}"`;
  const bindingPhraseHashed = md5(bindingPhraseFull);
  const uidBytes = bindingPhraseHashed.subarray(0,6);

  return uidBytes;
}

function initBindingPhraseGen() {
  const uid = _("uid");

  function setOutput(text) {
    const uidBytes = uidBytesFromText(text);
    uid.value = uidBytes;
  }

  function updateValue(e) {
    setOutput(e.target.value);
  }

  phrase.addEventListener("input", updateValue);
  setOutput("");
}