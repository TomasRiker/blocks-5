// pre.js - browser-side setup that must happen before main() runs.

// Two diagnostic knobs on the query string, the one way to reach them on a
// phone. Both become command-line arguments and nothing else:
//
//   ?perf=1       -perf, the frame timings on the screen.
//   ?flushall=1   -flushall, every quad put up on its own: the arm to compare
//                 the batching against, in the same binary.
//
// A knob that set a Module property Emscripten reads would also have to be
// named in -sINCOMING_MODULE_JS_API, or the start aborts under ASSERTIONS.
(function () {
  var query;
  try { query = new URLSearchParams(location.search); } catch (e) { return; }

  // Present and not switched off: ?perf=0 must not read as on.
  function wants(name) {
    if (!query.has(name)) return false;
    var v = query.get(name).toLowerCase();
    return v !== '0' && v !== 'false' && v !== 'off' && v !== 'no';
  }

  if (wants('perf')) {
    Module['arguments'] = (Module['arguments'] || []).concat(['-perf']);
  }

  if (wants('flushall')) {
    Module['arguments'] = (Module['arguments'] || []).concat(['-flushall']);
  }
})();

Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  // FileSystem::getAppHomeDirectory() returns "/blocks5_home/" in this build.
  // Back it with IDBFS so saves, progress and custom levels survive a reload.
  try {
    FS.mkdir('/blocks5_home');
    FS.mount(IDBFS, {}, '/blocks5_home');
    addRunDependency('idbfs-load');
    FS.syncfs(true, function (err) {
      if (err) console.warn('[blocks5] IDBFS load failed:', err);
      removeRunDependency('idbfs-load');
    });
  } catch (e) {
    console.warn('[blocks5] could not mount IDBFS:', e);
  }
});

// Ask that the IndexedDB holding every save not be evicted when the browser
// runs short of room. It is granted silently once the page looks kept
// (installed, bookmarked, visited often) and otherwise refused, at no cost.
(function () {
  try {
    if (navigator.storage && navigator.storage.persist) {
      navigator.storage.persisted().then(function (already) {
        if (already) return;
        navigator.storage.persist().then(function (granted) {
          if (!granted) console.log('[blocks5] storage is not persistent; saves may be evicted');
        });
      }).catch(function () {});
    }
  } catch (e) {}
})();

// A single coalescing syncfs driver. FS.syncfs warns when calls overlap
// (libfs.js:615-626) and IDBFS reconciles a whole mount per run, so a request
// arriving mid-flight sets a "dirty again" bit instead of starting a second
// pass. Both the periodic flush and WebTransfer::syncHome() go through here.
Module['b5_sync'] = (function () {
  var running = false, again = false;
  function run() {
    running = true; again = false;
    try {
      FS.syncfs(false, function (err) {
        running = false;
        if (err) console.warn('[blocks5] IDBFS sync failed:', err);
        if (again) run();
      });
    } catch (e) { running = false; console.warn('[blocks5] IDBFS sync threw:', e); }
  }
  return function () { if (running) again = true; else run(); };
})();

// Every function key belongs to the game, all of them bindable (web.md);
// otherwise F1 would open the browser's help, F5 reload and lose the level,
// F10 reach for the menu bar, F11 go fullscreen and F12 open the tools. The
// default is cancelled in the capture phase on window, ahead of SDL's
// listeners and the browser's own action; SDL still receives the key.
window.addEventListener('keydown', function (e) {
  if (/^F([1-9]|1[0-9]|2[0-4])$/.test(e.key) ||
      (e.keyCode >= 112 && e.keyCode <= 135)) e.preventDefault();
}, true);

// The AudioContext follows the page's visibility. Chrome suspends it in a
// backgrounded page, and Emscripten's own unlocker (autoResumeAudioContext in
// libcore.js) listens with { once: true } and is spent by the first click;
// StreamedSound::pumpBuffers restarts a queue that ran dry, but not the
// context. So it is resumed here on every return, from a DOM event, since the
// main loop is not running yet at that moment.
(function () {
  // AL is libopenal.js's library object, in scope for --pre-js and only
  // looked at once an event fires.
  function ctx() {
    try { return AL.currentCtx && AL.currentCtx.audioCtx; } catch (e) { return null; }
  }
  function resume() {
    var c = ctx();
    if (c && c.state === 'suspended') c.resume().catch(function () {});
  }
  // A hidden page gets no requestAnimationFrame, so the engine never even
  // polls the event that would mute it, and its per-tick volume pass has
  // stopped. Suspending the context freezes every source at once; without
  // it a looping effect - a laser - keeps sounding in a hidden tab.
  function suspend() {
    var c = ctx();
    if (c && c.state === 'running') c.suspend().catch(function () {});
  }
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) suspend(); else resume();
  });
  window.addEventListener('focus', resume);
  // And the first gesture, from the listener further down.
  Module['b5_resumeAudio'] = resume;
})();

// Fullscreen goes on the root element, never on the canvas: only the
// fullscreen element and its descendants are painted, and the on-screen pad
// is the canvas's sibling (window.md). The canvas is 100% of the page anyway.
// Call it under a transient user activation - the first gesture, the pad's
// button, the Alt+Return handler. Returns whether a request was made at all.
Module['b5_setFullscreen'] = function (on) {
  try {
    if (on) {
      // Without activation the request is refused - a phone may not grant it
      // as early as touchstart - so none is made, rather than a rejected
      // promise per touch. Emscripten's doRequestFullscreen tests the same.
      if (navigator.userActivation && !navigator.userActivation.isActive) return false;
      var el = document.documentElement;
      var req = el.requestFullscreen || el.webkitRequestFullscreen;
      if (!req) return false;
      var p = req.call(el);
      if (p && p['catch']) p['catch'](function () {});
      return true;
    }
    var exit = document.exitFullscreen || document.webkitExitFullscreen;
    if (!exit) return false;
    exit.call(document);
    return true;
  } catch (e) { return false; }
};

Module['b5_isFullscreen'] = function () {
  return !!(document.fullscreenElement || document.webkitFullscreenElement);
};

// The pad's button: whichever way the page is, the other.
Module['b5_toggleFullscreen'] = function () {
  Module['b5_setFullscreen'](!Module['b5_isFullscreen']());
};

// The first gesture takes the fullscreen, on every device, and never again on
// its own: leaving by a swipe or a long Escape is meant (window.md). These are
// the events that carry the activation the API demands - a mouse button going
// down, a finger or pen lifting, a key other than Escape - and the listeners
// stay armed until a request was actually made. The AudioContext is resumed
// here too: turning to landscape cancels the touch in flight, and SDL never
// sees the press GS_Loading waits for.
(function () {
  var types = ['mousedown', 'pointerup', 'touchend', 'keydown'];
  function first(e) {
    if (!e.isTrusted) return;
    if (e.type === 'keydown' && e.key === 'Escape') return;
    if (e.type === 'pointerup' && e.pointerType === 'mouse') return;
    Module['b5_resumeAudio']();
    if (!Module['b5_setFullscreen'](true)) return;
    types.forEach(function (t) { window.removeEventListener(t, first, true); });
  }
  types.forEach(function (t) { window.addEventListener(t, first, true); });
})();

// A touch the browser cancels - turning to landscape cancels the one in
// flight, and so does a gesture the system takes over - ends for Emscripten's
// SDL only with a touchend, which then never comes: it keeps the finger down
// and the game's button with it. A phone gives the next touch the same
// identifier, which SDL takes for that finger still down, and passes on no
// press for it: the next tap would be lost, and a list being dragged would
// follow the new finger. A cancel is therefore handed on as the touchend it
// stands for, of which SDL reads nothing but the type and the touches.
// Untrusted, it is no gesture the fullscreen request above could take, and
// the pad, which goes by pointer events, never sees it.
window.addEventListener('touchcancel', function (e) {
  var end = new Event('touchend', { bubbles: true, cancelable: true });
  Object.defineProperty(end, 'changedTouches', { value: e.changedTouches });
  Object.defineProperty(end, 'touches', { value: e.touches });
  Object.defineProperty(end, 'targetTouches', { value: e.targetTouches });
  e.target.dispatchEvent(end);
}, true);

// The text sheet (shell.html, web_textsheet.cpp). A phone shows its keyboard
// only for a field of the page, never for the canvas, and Android's keyboards
// type nothing a game could read as keys, so a finger's tap on one of the
// game's text fields opens a sheet of the page's own: the field's caption, a
// real field holding its text, OK and Cancel. The game says whether a place is
// a text field (GUI::textFieldTapped); whether the finger tapped is told here,
// from the path it took, because the sheet has to open inside the touchend:
// iOS shows its keyboard only for a field focused in the handler of a touch.
(function () {
  var down = null;   // the one finger on the canvas: where it pressed, the farthest it went
  var open = false, multiline = false, before = '';

  function el(id) { return document.getElementById(id); }

  // A touch's point in the canvas's pixels, computed as Emscripten's SDL
  // computes the press it makes of the same touch (calculateMouseCoords).
  function canvasPoint(t) {
    var c = Module['canvas'], r = c.getBoundingClientRect();
    return { x: (t.pageX - (window.scrollX + r.left)) * (c.width / r.width),
             y: (t.pageY - (window.scrollY + r.top)) * (c.height / r.height) };
  }

  function mine(list) {
    for (var i = 0; i < list.length; i++) if (list[i].identifier === down.id) return list[i];
    return null;
  }

  function follow(t) {
    var p = canvasPoint(t);
    var dx = p.x - down.x0, dy = p.y - down.y0, fx = down.x1 - down.x0, fy = down.y1 - down.y0;
    if (dx * dx + dy * dy > fx * fx + fy * fy) { down.x1 = p.x; down.y1 = p.y; }
  }

  window.addEventListener('touchstart', function (e) {
    // A second finger makes it no tap.
    if (e.touches.length !== 1 || e.target !== Module['canvas']) { down = null; return; }
    var t = e.changedTouches[0], p = canvasPoint(t);
    down = { id: t.identifier, x0: p.x, y0: p.y, x1: p.x, y1: p.y };
  }, true);

  window.addEventListener('touchmove', function (e) {
    var t = down && mine(e.changedTouches);
    if (t) follow(t);
  }, true);

  window.addEventListener('touchend', function (e) {
    var t = down && mine(e.changedTouches);
    if (!t) return;
    follow(t);
    var d = down;
    down = null;
    // Untrusted is a cancelled touch handed on as a lift (above): no tap.
    if (!e.isTrusted || open || !runtimeInitialized) return;
    try {
      // And once it is open, no mouse event made of the touch may take the
      // focus off its field again: the canvas takes the focus too.
      if (Module['_blocks5_textFieldTapped'](d.x0 | 0, d.y0 | 0, d.x1 | 0, d.y1 | 0)) e.preventDefault();
    } catch (err) { console.warn('[blocks5] text sheet:', err); }
  }, true);

  // What the game's fields can hold: Latin-1 from the space up, and a line
  // break in a multi-line one - the characters typedCharacter() lets
  // through. A phone's keyboard makes typographic quotes, dashes and an
  // ellipsis of its own accord, and they become the plain ones it replaced;
  // a letter beyond Latin-1 keeps what it is built on (an a of an a with a
  // macron), and what has none, an emoji, is left out.
  function forTheGame(text) {
    // Composed first, so that an a followed by a combining umlaut is the
    // Latin-1 letter it looks like.
    if (text.normalize) text = text.normalize('NFC');
    text = text.replace(/[\u2018\u2019\u201a\u201b\u2032]/g, "'")
               .replace(/[\u201c\u201d\u201e\u201f\u2033]/g, '"')
               .replace(/[\u2010-\u2015\u2212]/g, '-')
               .replace(/\u2026/g, '...')
               .replace(/[\u2007\u202f]/g, ' ');
    var out = '';
    for (var c of text) {
      var parts = (c.charCodeAt(0) > 255 && c.normalize) ? c.normalize('NFD') : c;
      for (var p of parts) {
        var code = p.charCodeAt(0);
        if ((code >= 32 && code <= 126) || (code >= 160 && code <= 255) || (code === 10 && multiline)) out += p;
      }
    }
    return out;
  }

  function field() { return el(multiline ? 'b5_sheet_lines' : 'b5_sheet_line'); }

  // A multi-line field takes what the keyboard leaves of the screen, so OK
  // and Cancel, above it, stay in sight; a phone reports the keyboard as the
  // visual viewport shrinking.
  function fit() {
    if (!open || !multiline) return;
    var f = el('b5_sheet_lines'), vv = window.visualViewport;
    var bottom = vv ? vv.offsetTop + vv.height : window.innerHeight;
    var room = bottom - f.getBoundingClientRect().top - 12;
    var line = parseFloat(getComputedStyle(f).lineHeight) || 21;
    f.style.height = Math.max(3 * line + 16, Math.min(12 * line + 16, room)) + 'px';
  }

  // web_textsheet.cpp calls this inside the touchend, which is what lets
  // focus() bring the keyboard up on iOS. A name typed exactly - a file, a
  // skin - gets no capital letter and no correction.
  Module['b5_openTextSheet'] = function (text, caption, isMultiline, verbatim, okText, cancelText) {
    open = true;
    multiline = isMultiline;
    el('b5_sheet_line').style.display = multiline ? 'none' : 'block';
    el('b5_sheet_lines').style.display = multiline ? 'block' : 'none';
    el('b5_sheet_caption').textContent = caption;
    el('b5_sheet_ok').textContent = okText;
    el('b5_sheet_cancel').textContent = cancelText;
    var f = field();
    f.setAttribute('autocapitalize', verbatim ? 'off' : 'sentences');
    f.setAttribute('autocorrect', verbatim ? 'off' : 'on');
    f.spellcheck = !verbatim;
    f.value = text;
    // As the field holds it, line breaks normalized: OK on a text nobody
    // touched changes nothing, whatever the field made of it.
    before = f.value;
    el('b5_sheet').style.display = 'flex';
    f.focus();
    try { f.setSelectionRange(f.value.length, f.value.length); } catch (e) {}
    fit();
  };

  function close(ok) {
    if (!open) return;
    var f = field();
    var changed = ok && f.value !== before;
    Module['b5_sheetText'] = changed ? forTheGame(f.value) : '';
    open = false;
    f.blur();
    el('b5_sheet').style.display = 'none';
    try { Module['_blocks5_textSheetClosed'](changed ? 1 : 0); } catch (err) { console.warn('[blocks5] text sheet:', err); }
  }

  function wire() {
    var sheet = el('b5_sheet');
    if (!sheet) return;
    el('b5_sheet_ok').addEventListener('click', function () { close(true); });
    el('b5_sheet_cancel').addEventListener('click', function () { close(false); });
    // A keyboard's Done or Go in a one-line field submits the form, however
    // the keyboard reports the key - Android's report 229 for it as well.
    el('b5_sheet_form').addEventListener('submit', function (e) { e.preventDefault(); close(true); });
    // What is typed here is the field's and nobody else's: SDL listens on
    // the document, and the default it cancels for every key would leave the
    // field with no character and no Backspace.
    ['keydown', 'keyup', 'keypress'].forEach(function (type) {
      sheet.addEventListener(type, function (e) {
        e.stopPropagation();
        if (type !== 'keydown' || e.isComposing) return;
        if (e.key === 'Escape') { e.preventDefault(); close(false); }
        else if (e.key === 'Enter' && !multiline) { e.preventDefault(); close(true); }
      });
    });
    if (window.visualViewport) window.visualViewport.addEventListener('resize', fit);
    window.addEventListener('resize', fit);
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', wire);
  else wire();
})();

// Escape stays with the game while fullscreen where the browser allows it,
// since the menu, the note and the dialogs hang off that key. Only Chromium
// has the Keyboard Lock, and it then wants a long Escape to leave; elsewhere
// the browser's exit takes the first press. Every failure is swallowed.
Module['b5_lockEscape'] = function () {
  var k = navigator.keyboard;
  if (!k || !k.lock) return;
  try {
    if (Module['b5_isFullscreen']()) {
      var p = k.lock(['Escape']);
      if (p && p['catch']) p['catch'](function () {});
    } else if (k.unlock) {
      k.unlock();
    }
  } catch (e) {}
};

// Landscape, only while fullscreen: the lock is refused unless the document
// already is, hence the change event rather than the request. It rejects on a
// desktop, so every failure is swallowed, and it is tried on every entry, so
// a tablet held upright turns like a phone. The manifest's landscape counts
// only for an installed app.
Module['b5_lockOrientation'] = function () {
  var o = window.screen && screen.orientation;
  if (!o) return;
  if (Module['b5_isFullscreen']()) {
    if (!o.lock) return;
    try {
      var p = o.lock('landscape');
      if (p && p['catch']) p['catch'](function () {});
    } catch (e) {}
  } else if (o.unlock) {
    try { o.unlock(); } catch (e) {}
  }
};

// Keeps the drawing buffer the size of the element. The game letterboxes its
// 640x480 frame into whatever size the canvas is, and the main loop reads the
// canvas size once a frame, which also catches the Fullscreen API.
Module['b5_fitCanvas'] = function () {
  var c = Module['canvas'];
  if (!c) return;
  // 100% in both states: the page goes fullscreen, not the canvas.
  c.style.width = '100%';
  c.style.height = '100%';
  var r = c.getBoundingClientRect();
  var w = Math.max(1, Math.round(r.width));
  var h = Math.max(1, Math.round(r.height));
  if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
};

Module['postRun'] = Module['postRun'] || [];
Module['postRun'].push(function () {
  // shell.html sizes the canvas in CSS; this keeps its drawing buffer in step.
  var c = Module['canvas'];
  if (c) {
    window.addEventListener('resize', Module['b5_fitCanvas']);
    // A phone changes the viewport without a resize event when the address bar
    // slides away or the device is turned; both arrive here.
    window.addEventListener('orientationchange', Module['b5_fitCanvas']);
    if (window.visualViewport) window.visualViewport.addEventListener('resize', Module['b5_fitCanvas']);
    document.addEventListener('fullscreenchange', Module['b5_fitCanvas']);
    document.addEventListener('webkitfullscreenchange', Module['b5_fitCanvas']);
    document.addEventListener('fullscreenchange', Module['b5_lockOrientation']);
    document.addEventListener('webkitfullscreenchange', Module['b5_lockOrientation']);
    document.addEventListener('fullscreenchange', Module['b5_lockEscape']);
    document.addEventListener('webkitfullscreenchange', Module['b5_lockEscape']);
    Module['b5_fitCanvas']();
  }

  setInterval(Module['b5_sync'], 5000);
  // A tab can be discarded without warning; pagehide is the last reliable hook.
  window.addEventListener('pagehide', Module['b5_sync']);
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) Module['b5_sync']();
  });
});
