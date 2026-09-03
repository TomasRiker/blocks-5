// touch_controls.js - an on-screen pad for playing without a keyboard.
//
// It dispatches ordinary keydown/keyup events on the document, which is the
// one route that reaches the game's named actions: Engine::updateVKs reads
// SDL_GetKeyState, and Emscripten's SDL updates that array from DOM key
// events. Engine::setKeyData would only reach the raw key layer - the GUI and
// the title demo - and movement would stay dead. See ROADMAP item 19.
//
// Nothing about the game changes. The pad sends a key, the action layer maps
// it, so a rebinding in the options dialog is followed for free.
//
// Stepping versus running needs no logic here either. The movement actions
// keep registerAction's defaults, delay 240 and interval 80, so a tap is one
// press - one step - and a held finger starts repeating after 240 ms. Holding
// the key for exactly as long as the finger is down gives both.
//
// Where it sits: the game is 4:3 inside a phone's much wider screen, so there
// are two black bars, 162 px on an iPhone 14 and 183 px on a Pixel 7. That is
// more than enough for a pad, and it covers nothing. Only where the bars are
// too narrow does it lie over the picture, at reduced opacity.
(function () {
  'use strict';

  // key/code/keyCode all set: Emscripten's SDL reads more than one of them
  // depending on the path, and a synthetic event costs nothing extra.
  var KEYS = {
    left:   { key: 'ArrowLeft',  code: 'ArrowLeft',   keyCode: 37 },
    right:  { key: 'ArrowRight', code: 'ArrowRight',  keyCode: 39 },
    up:     { key: 'ArrowUp',    code: 'ArrowUp',     keyCode: 38 },
    down:   { key: 'ArrowDown',  code: 'ArrowDown',   keyCode: 40 },
    bomb:   { key: 'Shift',      code: 'ShiftLeft',   keyCode: 16 },
    put:    { key: 'Control',    code: 'ControlLeft', keyCode: 17 },
    swap:   { key: 'Tab',        code: 'Tab',         keyCode: 9  },
    retry:  { key: 'F5',         code: 'F5',          keyCode: 116 },
    hotel:  { key: 'F10',        code: 'F10',         keyCode: 121 },
    menu:   { key: 'Escape',     code: 'Escape',      keyCode: 27 }
  };

  // A finger can lift faster than the game looks. Engine::update runs every
  // 20 ms and samples the keyboard once per run, so a press shorter than that
  // can fall between two samples and be lost. Holding every press for at least
  // this long makes a quick tap always worth one step.
  var MIN_HOLD_MS = 70;

  var held = {};        // name -> timestamp of the keydown
  var pending = {};     // name -> timeout id for a release still owed

  function send(name, type) {
    var k = KEYS[name];
    if (!k) return;
    document.dispatchEvent(new KeyboardEvent(type, {
      key: k.key, code: k.code, keyCode: k.keyCode, which: k.keyCode,
      bubbles: true, cancelable: true
    }));
  }

  function press(name) {
    if (pending[name]) { clearTimeout(pending[name]); delete pending[name]; }
    if (held[name]) return;
    held[name] = Date.now();
    send(name, 'keydown');
  }

  function release(name) {
    if (!held[name]) return;
    var left = MIN_HOLD_MS - (Date.now() - held[name]);
    if (left > 0) {
      pending[name] = setTimeout(function () { delete pending[name]; release(name); }, left);
      return;
    }
    delete held[name];
    send(name, 'keyup');
  }

  function releaseAll() {
    for (var name in held) if (held.hasOwnProperty(name)) release(name);
  }

  // --- the elements --------------------------------------------------------

  var root = document.createElement('div');
  root.id = 'b5pad';
  // The container lets everything through; only the controls themselves take a
  // touch, so a tap between them still reaches the canvas and the game's GUI.
  root.setAttribute('style', 'position:fixed;left:0;top:0;right:0;bottom:0;' +
                             'pointer-events:none;z-index:10;display:none');

  var CSS =
    'div#b5pad .b5btn{position:absolute;pointer-events:auto;touch-action:none;' +
    '-webkit-user-select:none;user-select:none;-webkit-tap-highlight-color:transparent;' +
    'display:flex;align-items:center;justify-content:center;' +
    'border:2px solid rgba(255,255,255,.30);border-radius:14px;' +
    'background:rgba(255,255,255,.10);color:rgba(255,255,255,.78);' +
    'font:600 13px/1 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;' +
    'letter-spacing:.02em;transition:background .06s,border-color .06s}' +
    'div#b5pad .b5btn.b5on{background:rgba(255,190,60,.42);border-color:rgba(255,200,90,.85);' +
    'color:#fff}' +
    'div#b5pad .b5big{border-radius:50%;font-size:14px}' +
    'div#b5pad .b5dpad{position:absolute;pointer-events:auto;touch-action:none;' +
    '-webkit-user-select:none;user-select:none;-webkit-tap-highlight-color:transparent;' +
    'border-radius:24px;background:rgba(255,255,255,.07);' +
    'border:2px solid rgba(255,255,255,.22)}' +
    // The four arrows are drawn, not written, so this file stays plain ASCII.
    'div#b5pad .b5arrow{position:absolute;width:0;height:0;pointer-events:none;' +
    'opacity:.55;transition:opacity .06s}' +
    'div#b5pad .b5arrow.b5on{opacity:1}';
  var style = document.createElement('style');
  style.textContent = CSS;

  function button(name, label, big) {
    var b = document.createElement('div');
    b.className = 'b5btn' + (big ? ' b5big' : '');
    b.textContent = label;
    var id = null;
    b.addEventListener('pointerdown', function (e) {
      e.preventDefault();
      id = e.pointerId;
      try { b.setPointerCapture(id); } catch (x) {}
      b.classList.add('b5on');
      press(name);
    });
    function up(e) {
      if (id === null || (e && e.pointerId !== id)) return;
      id = null;
      b.classList.remove('b5on');
      release(name);
    }
    b.addEventListener('pointerup', up);
    b.addEventListener('pointercancel', up);
    root.appendChild(b);
    return b;
  }

  // --- the d-pad -----------------------------------------------------------
  // One element with four zones rather than four buttons: a thumb rolls from
  // one direction into the next, and separate buttons leave a gap in between
  // where nothing at all is pressed.

  var dpad = document.createElement('div');
  dpad.className = 'b5dpad';
  var arrows = {};
  ['up', 'down', 'left', 'right'].forEach(function (dir) {
    var a = document.createElement('i');
    a.className = 'b5arrow';
    arrows[dir] = a;
    dpad.appendChild(a);
  });
  root.appendChild(dpad);

  var dpadId = null;
  var current = null;      // the direction being held, or null

  function setDirection(dir) {
    if (dir === current) return;
    if (current) { release(current); arrows[current].classList.remove('b5on'); }
    current = dir;
    if (current) { press(current); arrows[current].classList.add('b5on'); }
  }

  function pickDirection(e) {
    var r = dpad.getBoundingClientRect();
    var dx = e.clientX - (r.left + r.width / 2);
    var dy = e.clientY - (r.top + r.height / 2);
    var dead = r.width * 0.16;
    if (Math.abs(dx) < dead && Math.abs(dy) < dead) return null;

    // Hysteresis: the other axis has to win clearly before the direction
    // changes, or a thumb resting on a diagonal flickers between the two.
    var horizontal = Math.abs(dx) > Math.abs(dy);
    if (current === 'left' || current === 'right') horizontal = Math.abs(dx) * 1.3 > Math.abs(dy);
    if (current === 'up' || current === 'down') horizontal = Math.abs(dx) > Math.abs(dy) * 1.3;
    if (horizontal) return dx < 0 ? 'left' : 'right';
    return dy < 0 ? 'up' : 'down';
  }

  dpad.addEventListener('pointerdown', function (e) {
    e.preventDefault();
    dpadId = e.pointerId;
    try { dpad.setPointerCapture(dpadId); } catch (x) {}
    setDirection(pickDirection(e));
  });
  dpad.addEventListener('pointermove', function (e) {
    if (e.pointerId !== dpadId) return;
    setDirection(pickDirection(e));
  });
  function dpadUp(e) {
    if (dpadId === null || e.pointerId !== dpadId) return;
    dpadId = null;
    setDirection(null);
  }
  dpad.addEventListener('pointerup', dpadUp);
  dpad.addEventListener('pointercancel', dpadUp);

  var bBomb  = button('bomb',  'Bomb', true);
  var bPut   = button('put',   'Put',  true);
  var bSwap  = button('swap',  'Swap');
  var bRetry = button('retry', 'Retry');
  var bHotel = button('hotel', 'Hotel');
  var bMenu  = button('menu',  'Menu');

  // --- layout --------------------------------------------------------------
  // The same arithmetic the game uses in computePresentRect: the picture is
  // 640x480 scaled to fit, centred, and what is left over is the bar.

  function layout() {
    var w = window.innerWidth, h = window.innerHeight;
    var scale = Math.min(w / 640, h / 480);
    var bar = Math.round((w - 640 * scale) / 2);
    var inBars = bar >= 118;
    root.style.opacity = inBars ? '1' : '0.72';

    var pad = Math.max(126, Math.min(inBars ? bar - 16 : 168, Math.round(h * 0.52)));
    var big = Math.max(60, Math.min(Math.round(pad * 0.46), 84));
    var small = Math.max(46, Math.min(Math.round(pad * 0.33), 60));
    var m = 10;

    // Left: the d-pad, low enough for a thumb rather than centred.
    var lx = inBars ? Math.round((bar - pad) / 2) : m;
    var ly = Math.round(h - pad - Math.max(m, h * 0.06));
    dpad.style.left = lx + 'px'; dpad.style.top = ly + 'px';
    dpad.style.width = pad + 'px'; dpad.style.height = pad + 'px';

    var arm = Math.round(pad * 0.11);
    var off = Math.round(pad * 0.13);
    var edge = 'transparent';
    var col = 'rgba(255,255,255,.9)';
    function arrow(el, dir) {
      var s = el.style, half = Math.round(pad / 2);
      s.borderLeft = s.borderRight = s.borderTop = s.borderBottom = '0';
      if (dir === 'up' || dir === 'down') {
        s.borderLeft = arm + 'px solid ' + edge;
        s.borderRight = arm + 'px solid ' + edge;
        s.left = (half - arm) + 'px';
        if (dir === 'up') { s.borderBottom = arm + 'px solid ' + col; s.top = off + 'px'; }
        else { s.borderTop = arm + 'px solid ' + col; s.top = (pad - off - arm) + 'px'; }
      } else {
        s.borderTop = arm + 'px solid ' + edge;
        s.borderBottom = arm + 'px solid ' + edge;
        s.top = (half - arm) + 'px';
        if (dir === 'left') { s.borderRight = arm + 'px solid ' + col; s.left = off + 'px'; }
        else { s.borderLeft = arm + 'px solid ' + col; s.left = (pad - off - arm) + 'px'; }
      }
    }
    ['up', 'down', 'left', 'right'].forEach(function (d) { arrow(arrows[d], d); });

    // Right, low: the two the thumb rests on. Bomb is the nearer one.
    var rRight = inBars ? Math.round((bar - big) / 2) : m;
    var by = Math.round(h - big - Math.max(m, h * 0.06));
    bBomb.style.right = rRight + 'px'; bBomb.style.top = by + 'px';
    bBomb.style.width = big + 'px'; bBomb.style.height = big + 'px';
    bPut.style.right = (rRight + Math.round(big * 0.20)) + 'px';
    bPut.style.top = (by - big - m) + 'px';
    bPut.style.width = big + 'px'; bPut.style.height = big + 'px';

    // Right, high and out of the way: the ones a mistake would hurt.
    var gx = inBars ? Math.round((bar - (2 * small + m)) / 2) : m;
    var gy = Math.max(m, Math.round(h * 0.05));
    [[bMenu, 0, 0], [bSwap, 1, 0], [bRetry, 0, 1], [bHotel, 1, 1]].forEach(function (t) {
      var el = t[0];
      el.style.right = (gx + t[1] * (small + m)) + 'px';
      el.style.top = (gy + t[2] * (small + m)) + 'px';
      el.style.width = small + 'px'; el.style.height = small + 'px';
    });
  }

  // --- when to show it -----------------------------------------------------
  // There is no way to ask whether a physical keyboard exists. maxTouchPoints,
  // (pointer: coarse) and the Keyboard API all describe pointers or layouts,
  // never presence, and a laptop can have a touchscreen too. So: guess from the
  // pointer, then correct from behaviour - a real keypress hides it, a touch
  // brings it back. ?pad=on / ?pad=off overrides and is remembered.

  var visible = false;

  function show(on) {
    if (on === visible) return;
    visible = on;
    root.style.display = on ? 'block' : 'none';
    if (!on) { setDirection(null); releaseAll(); }
    if (on) layout();
  }

  function remember(v) {
    try { localStorage.setItem('b5pad', v); } catch (e) {}
  }

  function initialChoice() {
    var forced = (location.search.match(/[?&]pad=(on|off|auto)/) || [])[1];
    if (forced) { remember(forced); if (forced !== 'auto') return forced === 'on'; }
    var stored = null;
    try { stored = localStorage.getItem('b5pad'); } catch (e) {}
    if (stored === 'on') return true;
    if (stored === 'off') return false;
    return (window.matchMedia && window.matchMedia('(any-pointer: coarse)').matches) ||
           navigator.maxTouchPoints > 0;
  }

  document.addEventListener('DOMContentLoaded', function () {
    document.head.appendChild(style);
    document.body.appendChild(root);
    show(initialChoice());
  });
  if (document.readyState !== 'loading') {
    document.head.appendChild(style);
    document.body.appendChild(root);
    show(initialChoice());
  }

  window.addEventListener('resize', function () { if (visible) layout(); });
  window.addEventListener('orientationchange', function () { if (visible) layout(); });
  if (window.visualViewport) {
    window.visualViewport.addEventListener('resize', function () { if (visible) layout(); });
  }

  // A real key hides the pad; the pad's own events are untrusted, so they
  // cannot hide it by accident. That one flag is the whole filter.
  window.addEventListener('keydown', function (e) {
    if (e.isTrusted && visible) { show(false); remember('off'); }
  }, true);
  window.addEventListener('touchstart', function () {
    if (!visible) { show(true); remember('on'); }
  }, true);

  // Nothing may stay held while the page is away, or the game keeps walking.
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) { setDirection(null); releaseAll(); }
  });
  window.addEventListener('blur', function () { setDirection(null); releaseAll(); });

  // For the tests, and for a look from the console.
  window.b5pad = { show: show, isVisible: function () { return visible; },
                   held: function () { return Object.keys(held); }, layout: layout };
})();
