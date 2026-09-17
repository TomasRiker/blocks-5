// pre.js - browser-side setup that must happen before main() runs.

// Two knobs on the query string, both diagnostics. They live here because a
// phone has no console, no command line and no test harness: typing a URL is
// the only way to reach them, and it is the same URL on a desktop.
//
//   ?perf=1       put the frame timings on the screen. It becomes the -perf
//                 the desktop build takes on its command line, so both
//                 platforms run one piece of code. Turning a query string into
//                 argv is Emscripten's own idiom - see its emrun_prejs.js.
//
//   ?flushall=1   becomes -flushall, which makes the renderer put every quad
//                 up on its own instead of batching it. That is the arm to
//                 compare the batching against, and it is a query string
//                 rather than a build flag so that both arms are one binary.
//
// Both become arguments and nothing else. A knob that had to set a Module.*
// property Emscripten reads would also have to name it in build.sh's
// -sINCOMING_MODULE_JS_API, or the start aborts on it with ASSERTIONS on.
(function () {
  var query;
  try { query = new URLSearchParams(location.search); } catch (e) { return; }

  // Present and not switched off. A bare query.get() would read "0" as on,
  // since every non-empty string is truthy - and ?perf=0 asking for the
  // overlay is the opposite of what anybody types it for.
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

// Saves, progress and imported levels live in IndexedDB, and a browser is
// allowed to throw that away when it is short of room - which on a phone is a
// question of when, not whether. Asking makes the origin's storage persistent
// where the browser is willing; it grants it silently once the page looks like
// something the user meant to keep (installed to the home screen, bookmarked,
// visited often) and otherwise says no, which costs nothing.
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

// Every function key belongs to the game here, not to the browser. They are
// bindable like any other key and the desktop build answers to all of them, so
// a player who knows the game must not find half of them missing; taking a
// named few and leaving the rest would be the worst of both. Swallowed in the
// capture phase, before SDL or the browser sees them - F1 would otherwise open
// the browser's help, F5 reload the page and lose the level, F10 reach for the
// menu bar, F11 go fullscreen and F12 open the developer tools.
//
// Nothing is lost by it: fullscreen is Alt+Enter, the same idiom as on the
// desktop and the one the loading screen names, while Ctrl+R and the address
// bar still reload and Ctrl+Shift+I still opens the tools. Whether a browser
// hands a page F11 and F12 at all is its own decision - asking costs nothing
// where the answer is no.
window.addEventListener('keydown', function (e) {
  if (/^F([1-9]|1[0-9]|2[0-4])$/.test(e.key) ||
      (e.keyCode >= 112 && e.keyCode <= 135)) e.preventDefault();
}, true);

// The other half of "the music stopped when I switched tabs". A hidden page
// gets no requestAnimationFrame, so the game cannot refill the OpenAL queue and
// the source runs dry; StreamedSound::pumpBuffers restarts it when the page
// comes back. What it cannot restart is the AudioContext: Chrome suspends the
// one belonging to a backgrounded page, and Emscripten's own unlocker
// (autoResumeAudioContext in libcore.js) registers its listeners with
// { once: true } and spent them on the very first click of the session. Nobody
// would ever resume it again, so do it here - on every return to the page, and
// from a real DOM event rather than from inside the main loop, which is exactly
// what is not running yet at that moment.
(function () {
  // AL is the library object from libopenal.js; --pre-js lands in the same
  // scope, and it is only looked at when an event fires, long after the
  // runtime has defined it.
  function ctx() {
    try { return AL.currentCtx && AL.currentCtx.audioCtx; } catch (e) { return null; }
  }
  function resume() {
    var c = ctx();
    if (c && c.state === 'suspended') c.resume().catch(function () {});
  }
  // A hidden page is a stopped game: requestAnimationFrame does not fire, so
  // no logic tick runs and nothing in the engine can react. Muting is the
  // engine's own answer to losing focus, but it cannot work here twice over -
  // Emscripten's SDL reports a hidden page as SDL_WINDOWEVENT and the game
  // listens for SDL 1.2's SDL_ACTIVEEVENT, and the volume change would be
  // applied by the very per-tick pass that has stopped. So the page does it,
  // from the DOM event, one layer below the engine: suspending the context
  // freezes every source at once. Without it the music dies on its own when
  // its queue runs dry, while a looping effect - a laser - keeps buzzing in a
  // tab nobody is looking at.
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

// Fullscreen goes on the root element, never on the canvas. Only the fullscreen
// element and its descendants are painted, so with the canvas itself promoted
// the on-screen controls - a sibling of it - simply vanish, while still
// reporting a full-size bounding rect, which is why this survived a test that
// only measured. From <html> both are inside, and the canvas is 100%/100% of
// the page anyway, so it fills the screen without anyone resizing it.
//
// It has to be called under a transient user activation: from the first
// gesture below, from the pad's button or from the Alt+Return callback at the
// DOM. Returns whether a request was made at all.
Module['b5_setFullscreen'] = function (on) {
  try {
    if (on) {
      // Without transient activation the request is refused, and a phone does
      // not necessarily grant it as early as touchstart. Say nothing then: a
      // rejected promise per touch would only fill the console. This is the
      // same test Emscripten's own doRequestFullscreen makes before deferring.
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
// its own: a swipe or a long Escape out of it is meant, and Alt+Return or the
// pad's button bring it back. No guess about the device is involved. The one
// that stood here, a coarse pointer and no fine one, called a Galaxy with no
// pen and no mouse a notebook and left it under the address bar.
//
// The events are the ones that carry the activation the API demands: a mouse
// button going down, a finger or a pen lifting, a key other than Escape. The
// loading screen already stops for a gesture, so nobody pays an extra one. The
// listeners stay armed until a request was actually made, in case the first
// event arrives without activation after all.
//
// The AudioContext is resumed from the same event, and not only in
// GS_Loading: going fullscreen turns a phone to landscape, the rotation makes
// the browser cancel the touch in flight, and SDL then never sees the press
// that was the gesture.
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

// Escape stays with the game while fullscreen, where the browser allows it:
// the menu, the note and the dialogs all hang off that key, and the browser's
// own exit would otherwise take the first press. Chromium alone has the API;
// it asks for a long Escape to leave instead and says so in its own bubble.
// Firefox and Safari keep their exit, and the game gets its Escape on the
// next press. Every path swallows the failure, as with the orientation.
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

// Landscape, and only while the game holds the screen. The lock is refused
// unless the document is fullscreen, which is why this hangs off the change
// event rather than off the request: on Android the promise rejects if the two
// are the wrong way round. It rejects on a desktop in any case - there is no
// orientation to lock - so every path here swallows the failure, and it is
// simply attempted on every entry: a tablet held upright gets the picture the
// right way round like a phone does.
//
// The manifest asks for landscape as well, but that only counts once the game
// has been installed to the home screen. This is the same answer for the page.
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

// The canvas fills the page and follows the browser window. The game renders
// 640x480 into a framebuffer object and letterboxes that into whatever size the
// canvas is, so nothing here has to know about the game's own resolution - it
// only has to keep the drawing buffer the same size as the element. Engine's
// main loop reads the canvas size once a frame and picks the change up from
// there, which also catches the Fullscreen API without a second code path.
Module['b5_fitCanvas'] = function () {
  var c = Module['canvas'];
  if (!c) return;
  // 100% of the page in both states, because it is the page that goes
  // fullscreen and not the canvas - see b5_setFullscreen.
  c.style.width = '100%';
  c.style.height = '100%';
  var r = c.getBoundingClientRect();
  var w = Math.max(1, Math.round(r.width));
  var h = Math.max(1, Math.round(r.height));
  if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
};

Module['postRun'] = Module['postRun'] || [];
Module['postRun'].push(function () {
  // The page itself is shell.html, which already gives the canvas the whole
  // viewport in CSS and suppresses the browser's own touch gestures. What is
  // left here is keeping the drawing buffer in step with the element.
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
