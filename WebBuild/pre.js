// pre.js - browser-side setup that must happen before main() runs.
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

// Three function keys the game binds mean something else to a browser, and the
// browser wins unless the event is swallowed first: F1 mutes and unmutes but
// opens the browser's help, F5 restarts the level but reloads the page, F10
// restarts from the hotel but opens the menu bar in some of them. Losing the
// session or the keyboard is not what somebody pressing one of these wants.
// All three are taken in the capture phase, before SDL or the browser sees
// them. F11 and F12 are left alone: they are screenshot and video recording,
// and neither of those exists in this build. Ctrl+R and the address bar still
// reload, so a page can never get stuck.
window.addEventListener('keydown', function (e) {
  if (e.key === 'F1'  || e.keyCode === 112 ||
      e.key === 'F5'  || e.keyCode === 116 ||
      e.key === 'F10' || e.keyCode === 121) e.preventDefault();
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
})();

// A phone, meaning a device with no mouse. (any-pointer: fine) is what a mouse
// or a stylus reports, so its absence together with a coarse pointer is the
// closest the platform gets to the question actually being asked. It is one
// function rather than two copies because Engine::enforceTouchFullScreen asks
// it too, through Module.
Module['b5_isPhone'] = function () {
  if (!window.matchMedia) return navigator.maxTouchPoints > 0;
  return window.matchMedia('(any-pointer: coarse)').matches &&
         !window.matchMedia('(any-pointer: fine)').matches;
};

// Fullscreen goes on the root element, never on the canvas. Only the fullscreen
// element and its descendants are painted, so with the canvas itself promoted
// the on-screen controls - a sibling of it - simply vanish, while still
// reporting a full-size bounding rect, which is why this survived a test that
// only measured. From <html> both are inside, and the canvas is 100%/100% of
// the page anyway, so it fills the screen without anyone resizing it.
//
// It has to be called from a trusted event handler; C++ does that through
// Engine::enforceTouchFullScreen and the Alt+Return callback.
Module['b5_setFullscreen'] = function (on) {
  try {
    if (on) {
      var el = document.documentElement;
      var req = el.requestFullscreen || el.webkitRequestFullscreen;
      if (!req) return;
      var p = req.call(el);
      if (p && p['catch']) p['catch'](function () {});
    } else {
      var exit = document.exitFullscreen || document.webkitExitFullscreen;
      if (exit) exit.call(document);
    }
  } catch (e) {}
};

// Landscape, and only while the game holds the screen. The lock is refused
// unless the document is fullscreen, which is why this hangs off the change
// event rather than off the request: on Android the promise rejects if the two
// are the wrong way round. It rejects on a desktop in any case - there is no
// orientation to lock - so every path here swallows the failure.
//
// The manifest asks for landscape as well, but that only counts once the game
// has been installed to the home screen. This is the same answer for the page.
Module['b5_lockOrientation'] = function () {
  var o = window.screen && screen.orientation;
  if (!o || !Module['b5_isPhone']()) return;
  var full = document.fullscreenElement || document.webkitFullscreenElement;
  if (full) {
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
    Module['b5_fitCanvas']();
  }

  setInterval(Module['b5_sync'], 5000);
  // A tab can be discarded without warning; pagehide is the last reliable hook.
  window.addEventListener('pagehide', Module['b5_sync']);
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) Module['b5_sync']();
  });
});
