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

// Emscripten's SDL makes one mouse of every finger on the canvas: a press for
// each new one, a move to wherever the first in the page's list of touches
// is, a release for every lift. Two fingers would be one mouse jumping between
// them - the game would drop the first finger's gesture at the second's press,
// follow whichever the list holds first and take the second's lift for the
// first's - and a finger held on the pad, first in that list, would be where
// a finger on the canvas pressed. So SDL is handed one finger, the first to
// land on the canvas while none is down there, from its touch to its lift, in
// copies of the page's events that hold that finger alone; the page's own stop
// on their way. Their default is cancelled here as SDL cancelled it - no
// scrolling, and no mouse events made of a tap - which a listener on the
// window may do only where it says it is not passive. SDL reads nothing of an
// event but its type and its touches.
//
// A touch the browser cancels - turning to landscape cancels the one in
// flight, and so does a gesture the system takes over - is handed on as the
// lift it stands for: SDL keeps a finger down until it sees one, and would
// take the next touch, which a phone gives the same identifier, for that
// finger still down and make no press of it. The lift carries a touch id of
// its own, engine.cpp's CANCELLED_TOUCH, and the game lets go of what the
// finger held without the click, the selection or the glide a lift brings.
// Untrusted, no copy is a gesture the fullscreen request above could take,
// and the pad, which goes by pointer events, never sees one. A copy carries
// the page's event's timeStamp as b5time, the time the engine takes for the
// touch (inputtime.cpp): its own is when it was made.
(function () {
  var CANCELLED_TOUCH = 0x43414e43;
  var finger = null;   // the identifier of the finger SDL is handed, null for none
  var last = null;     // where that finger was last, for a lift SDL has to be told of

  function find(list) {
    for (var i = 0; i < list.length; i++) if (list[i].identifier === finger) return list[i];
    return null;
  }
  function copyOf(t, id) {
    return { identifier: t.identifier, clientX: t.clientX, clientY: t.clientY,
             pageX: t.pageX, pageY: t.pageY, screenX: t.screenX, screenY: t.screenY, deviceID: id };
  }
  function handOn(type, t, stamp) {
    var copy = new Event(type, { bubbles: true, cancelable: true });
    var held = type === 'touchend' ? [] : [t];
    Object.defineProperty(copy, 'touches', { value: held });
    Object.defineProperty(copy, 'targetTouches', { value: held });
    Object.defineProperty(copy, 'changedTouches', { value: [t] });
    copy.b5time = stamp;
    Module['canvas'].dispatchEvent(copy);
  }
  function cancelled(stamp) {
    var t = copyOf(last, CANCELLED_TOUCH);
    finger = last = null;
    handOn('touchend', t, stamp);
  }
  // The page's own touches on the canvas, stopped; the copies pass.
  function ours(e) {
    if (!e.isTrusted || e.target !== Module['canvas']) return false;
    e.stopPropagation();
    if (e.cancelable) e.preventDefault();
    return true;
  }
  var options = { capture: true, passive: false };

  window.addEventListener('touchstart', function (e) {
    if (!ours(e)) return;
    // The finger handed on is gone from the glass without an end or a
    // cancel: SDL is told it went, as of a cancel, or it would take no other.
    if (finger !== null && !find(e.touches)) cancelled(e.timeStamp);
    if (finger !== null) return;
    var t = e.changedTouches[0];
    finger = t.identifier;
    last = copyOf(t);
    handOn('touchstart', t, e.timeStamp);
  }, options);
  window.addEventListener('touchmove', function (e) {
    var t = ours(e) && finger !== null && find(e.changedTouches);
    if (!t) return;
    last = copyOf(t);
    handOn('touchmove', t, e.timeStamp);
  }, options);
  window.addEventListener('touchend', function (e) {
    var t = ours(e) && finger !== null && find(e.changedTouches);
    if (!t) return;
    finger = last = null;
    handOn('touchend', t, e.timeStamp);
  }, options);
  window.addEventListener('touchcancel', function (e) {
    var t = ours(e) && finger !== null && find(e.changedTouches);
    if (!t) return;
    last = copyOf(t);
    cancelled(e.timeStamp);
  }, options);
})();

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
  // Whether a field of several lines takes the room left (fit): not before
  // the keyboard has had its moment to come up. Counted, so that the moment
  // of a sheet already closed cannot grow the next one.
  var grown = false, opened = 0;
  var watcher = null;  // Android's Back, while the sheet is open
  // Keys the sheet took the press of, by code, until a press of their own:
  // the repeats of one held down as it closed the sheet are not new presses.
  var taken = {};

  function el(id) { return document.getElementById(id); }

  // A touch's point in the canvas's pixels, computed as Emscripten's SDL
  // computes the press it makes of the same touch: the client coordinates,
  // which it hands to calculateMouseCoords in place of the page's, scroll
  // and all.
  function canvasPoint(t) {
    var c = Module['canvas'], r = c.getBoundingClientRect();
    return { x: (t.clientX - (window.scrollX + r.left)) * (c.width / r.width),
             y: (t.clientY - (window.scrollY + r.top)) * (c.height / r.height) };
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

  // The page's own touches, not the copies of them the game is handed
  // (above).
  window.addEventListener('touchstart', function (e) {
    if (!e.isTrusted) return;
    // A second finger makes it no tap.
    if (e.touches.length !== 1 || e.target !== Module['canvas']) { down = null; return; }
    var t = e.changedTouches[0], p = canvasPoint(t);
    down = { id: t.identifier, x0: p.x, y0: p.y, x1: p.x, y1: p.y };
  }, true);

  window.addEventListener('touchmove', function (e) {
    var t = e.isTrusted && down && mine(e.changedTouches);
    if (t) follow(t);
  }, true);

  // A cancelled touch is no tap.
  window.addEventListener('touchcancel', function (e) {
    if (e.isTrusted && down && mine(e.changedTouches)) down = null;
  }, true);

  // The default of every touch on the canvas is cancelled (above), which is
  // also what keeps the mouse events a browser makes of a tap from taking
  // the focus off the field just opened.
  window.addEventListener('touchend', function (e) {
    var t = e.isTrusted && down && mine(e.changedTouches);
    if (!t) return;
    follow(t);
    var d = down;
    down = null;
    if (open || !runtimeInitialized) return;
    try {
      Module['_blocks5_textFieldTapped'](d.x0 | 0, d.y0 | 0, d.x1 | 0, d.y1 | 0);
    } catch (err) { console.warn('[blocks5] text sheet:', err); }
  }, true);

  // Letters with nothing to decompose into, as the letters they are read as.
  var PLAIN = { '\u0110': 'D', '\u0111': 'd', '\u0126': 'H', '\u0127': 'h', '\u0131': 'i',
                '\u0141': 'L', '\u0142': 'l', '\u014a': 'N', '\u014b': 'n', '\u0152': 'OE',
                '\u0153': 'oe', '\u0166': 'T', '\u0167': 't', '\u1e9e': 'SS', '\u20ac': 'EUR' };

  // What the game's fields can hold: Latin-1 from the space up, and a line
  // break in a multi-line one - the characters typedCharacter() lets
  // through. A phone's keyboard makes typographic quotes, dashes and an
  // ellipsis of its own accord, and they become the plain ones it replaced;
  // a letter beyond Latin-1 keeps what it is built on (an a of an a with a
  // macron, the l of a Polish l), and what has none, an emoji, is left out.
  function forTheGame(text) {
    // Composed first, so that an a followed by a combining umlaut is the
    // Latin-1 letter it looks like.
    if (text.normalize) text = text.normalize('NFC');
    text = text.replace(/[\u2018\u2019\u201a\u201b\u2032]/g, "'")
               .replace(/[\u201c\u201d\u201e\u201f\u2033]/g, '"')
               .replace(/[\u2010-\u2015\u2212]/g, '-')
               .replace(/\u2026/g, '...')
               .replace(/[\u2007\u202f]/g, ' ')
               // A language marker is lower case, as the game looks for it,
               // whatever a keyboard capitalizing the start of a line made of it.
               .replace(/\u00a7([A-Za-z]{2}):/g, function (m, code) { return '\u00a7' + code.toLowerCase() + ':'; });
    var out = '';
    for (var c of text) {
      var parts = c.charCodeAt(0) <= 255 ? c : PLAIN[c] || (c.normalize ? c.normalize('NFD') : c);
      for (var p of parts) {
        var code = p.charCodeAt(0);
        if ((code >= 32 && code <= 126) || (code >= 160 && code <= 255) || (code === 10 && multiline)) out += p;
      }
    }
    return out;
  }

  function field() { return el(multiline ? 'b5_sheet_lines' : 'b5_sheet_line'); }

  // The sheet covers what the keyboard leaves of the screen and follows it
  // there: a phone reports the keyboard as the visual viewport shrinking,
  // and pans that viewport to show the caret, which would otherwise carry
  // OK and Cancel off the top. A multi-line field takes the height left
  // once it may grow.
  function fit() {
    if (!open) return;
    var s = el('b5_sheet'), vv = window.visualViewport;
    if (vv) {
      s.style.top = vv.offsetTop + 'px';
      s.style.left = vv.offsetLeft + 'px';
      s.style.width = vv.width + 'px';
      s.style.height = vv.height + 'px';
    }
    if (!multiline || !grown) return;
    var f = el('b5_sheet_lines');
    var bottom = vv ? vv.offsetTop + vv.height : window.innerHeight;
    var room = bottom - f.getBoundingClientRect().top - 12;
    f.style.height = Math.max(lines(f, 2), Math.min(lines(f, 12), room)) + 'px';
  }

  // The keyboard has come up, or gone, or had its moment: the room left is
  // the field's.
  function grow() { grown = true; fit(); }

  // The height of so many lines of the field's text, its padding and border
  // included.
  function lines(f, n) { return n * (parseFloat(getComputedStyle(f).lineHeight) || 21) + 16; }

  // web_textsheet.cpp calls this inside the touchend, which is what lets
  // focus() bring the keyboard up on iOS. A name typed exactly - a file, a
  // skin - gets no capital letter and no correction. Whether it opened: an
  // exception must not unwind through the wasm that called it.
  Module['b5_openTextSheet'] = function (text, caption, isMultiline, verbatim, okText, cancelText) {
    try {
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
      // A field of several lines opens two lines high, so that the caret at
      // its end is not where the keyboard is about to come up and the phone
      // pans nothing to show it; it grows to the room the keyboard leaves as
      // the keyboard comes, or to the screen where none comes.
      grown = false;
      if (multiline) f.style.height = lines(f, 2) + 'px';
      f.focus({ preventScroll: true });
      try { f.setSelectionRange(f.value.length, f.value.length); } catch (e) {}
      open = true;
      fit();
      if (multiline) f.scrollTop = f.scrollHeight;
      var mine = ++opened;
      setTimeout(function () { if (open && mine === opened) grow(); }, 400);
      if (window.CloseWatcher) {
        try {
          watcher = new CloseWatcher();
          watcher.onclose = function () { watcher = null; close(false); };
        } catch (e) { watcher = null; }
      }
      return true;
    } catch (err) {
      console.warn('[blocks5] text sheet:', err);
      open = false;
      var sheet = el('b5_sheet');
      if (sheet) sheet.style.display = 'none';
      return false;
    }
  };

  function close(ok) {
    if (!open) return;
    var f = field();
    var changed = ok && f.value !== before;
    Module['b5_sheetText'] = changed ? forTheGame(f.value) : '';
    open = false;
    if (watcher) {
      var w = watcher;
      watcher = null;
      try { w.destroy(); } catch (e) {}
    }
    var s = el('b5_sheet');
    // The focus leaves the sheet, wherever in it it was: a browser that
    // keeps it on an element hidden would send it the keys.
    if (s.contains(document.activeElement)) document.activeElement.blur();
    s.style.display = 'none';
    s.style.top = s.style.left = s.style.width = s.style.height = '';
    // What a keyboard scrolled is put back: SDL takes a touch's client
    // coordinates for the page's, so every later touch would land off by it.
    window.scrollTo(0, 0);
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
    // A press anywhere in the sheet but on a field leaves the focus where it
    // is, so a tap on the dimmed page does not take it from the field and
    // the keyboard with it. The buttons are clicked all the same.
    sheet.addEventListener('mousedown', function (e) {
      if (e.target !== el('b5_sheet_line') && e.target !== el('b5_sheet_lines')) e.preventDefault();
    });
    // What is typed here is the field's and nobody else's: SDL listens on
    // the document and cancels the default of every keypress, and of
    // Backspace, which would leave the field with no character and nothing
    // to delete with. The release goes on: it types nothing, and a key held
    // as the sheet opened would otherwise stay down in the game. Enter is OK
    // in the one-line field only - not on a focused button, not as Alt+Enter,
    // which is the fullscreen, and not as the Enter Safari sends (229) to
    // confirm what an input method composed.
    ['keydown', 'keypress'].forEach(function (type) {
      sheet.addEventListener(type, function (e) {
        if (!open) return;
        e.stopPropagation();
        if (type !== 'keydown') return;
        if (e.code) taken[e.code] = true;
        if (e.isComposing || e.keyCode === 229) return;
        if (e.key === 'Escape') { e.preventDefault(); close(false); }
        else if (e.key === 'Enter' && !e.altKey && !multiline && e.target === field()) { e.preventDefault(); close(true); }
      });
    });
    // A button's Space clicks it at the release, the default SDL cancels.
    sheet.addEventListener('keyup', function (e) {
      if (open && e.key === ' ' && e.target.tagName === 'BUTTON') e.target.click();
    });
    // Nor does the game get a key while the sheet is open and its field has
    // lost the focus: typed into the game's field behind the sheet, or Escape
    // quitting the editor there, either would happen out of sight. Only kept
    // from SDL, so the browser's own keys still work, Tab back into the sheet
    // among them; Escape is Cancel here too. And once it has closed, the
    // repeats of a key it took the press of - an Escape or an Enter held as
    // it closed - stay away from the game until that key is pressed anew.
    ['keydown', 'keypress'].forEach(function (type) {
      window.addEventListener(type, function (e) {
        if (!open) {
          if (!e.code || !taken[e.code]) return;
          if (type === 'keydown' && !e.repeat) { delete taken[e.code]; return; }
          e.stopPropagation();
          return;
        }
        if (sheet.contains(e.target)) return;
        e.stopPropagation();
        if (type !== 'keydown') return;
        if (e.code) taken[e.code] = true;
        if (e.key === 'Escape') close(false);
      }, true);
    });
    if (window.visualViewport) {
      window.visualViewport.addEventListener('resize', grow);
      window.visualViewport.addEventListener('scroll', fit);
    }
    window.addEventListener('resize', grow);
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
