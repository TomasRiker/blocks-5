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
