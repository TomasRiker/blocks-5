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

// F5 restarts a level and F10 restarts from the hotel. To a browser F5 is
// reload, and F10 opens the menu bar in some of them - either one throws the
// session away or takes the keyboard, and neither is what somebody pressing
// "restart" wants. Both are swallowed in the capture phase, before SDL or the
// browser sees them. F11 and F12 are left alone: they are screenshot and video
// recording, and neither of those exists in this build. Ctrl+R and the address
// bar still reload, so a page can never get stuck.
window.addEventListener('keydown', function (e) {
  if (e.key === 'F5' || e.keyCode === 116 ||
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
  function resume() {
    try {
      // AL is the library object from libopenal.js; --pre-js lands in the same
      // scope, and it is only looked at when an event fires, long after the
      // runtime has defined it.
      var ctx = AL.currentCtx && AL.currentCtx.audioCtx;
      if (ctx && ctx.state === 'suspended') ctx.resume().catch(function () {});
    } catch (e) {}
  }
  document.addEventListener('visibilitychange', function () {
    if (!document.hidden) resume();
  });
  window.addEventListener('focus', resume);
})();

// The canvas fills the page and follows the browser window. The game renders
// 640x480 into a framebuffer object and letterboxes that into whatever size the
// canvas is, so nothing here has to know about the game's own resolution - it
// only has to keep the drawing buffer the same size as the element. Engine's
// main loop reads the canvas size once a frame and picks the change up from
// there, which also catches the Fullscreen API without a second code path.
Module['b5_fitCanvas'] = function () {
  var c = Module['canvas'];
  if (!c) return;
  // In fullscreen the browser sizes the element itself; do not fight it.
  var full = document.fullscreenElement || document.webkitFullscreenElement;
  if (!full) {
    c.style.width = '100%';
    c.style.height = '100%';
  }
  var r = c.getBoundingClientRect();
  var w = Math.max(1, Math.round(r.width));
  var h = Math.max(1, Math.round(r.height));
  if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
};

Module['postRun'] = Module['postRun'] || [];
Module['postRun'].push(function () {
  // The generated shell centres the canvas in a bordered div; make that div and
  // its ancestors fill the viewport instead, so "resize the window" means
  // something. Everything else in the shell (the logo, the status line, the
  // output box) is chrome we do not want in the way.
  var c = Module['canvas'];
  if (c) {
    document.documentElement.style.height = '100%';
    document.body.style.height = '100%';
    document.body.style.margin = '0';
    document.body.style.background = '#000';
    document.body.style.overflow = 'hidden';
    var box = c.parentElement;
    if (box) {
      box.style.border = '0';
      box.style.position = 'fixed';
      box.style.left = '0';
      box.style.top = '0';
      box.style.width = '100%';
      box.style.height = '100%';
    }
    c.style.display = 'block';
    ['emscripten_logo', 'status', 'progress', 'controls', 'output'].forEach(function (id) {
      var e = document.getElementById(id);
      if (e) e.style.display = 'none';
    });
    var logo = document.querySelector('a[href*="emscripten.org"]');
    if (logo) logo.style.display = 'none';
    window.addEventListener('resize', Module['b5_fitCanvas']);
    document.addEventListener('fullscreenchange', Module['b5_fitCanvas']);
    document.addEventListener('webkitfullscreenchange', Module['b5_fitCanvas']);
    Module['b5_fitCanvas']();
  }

  setInterval(Module['b5_sync'], 5000);
  // A tab can be discarded without warning; pagehide is the last reliable hook.
  window.addEventListener('pagehide', Module['b5_sync']);
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) Module['b5_sync']();
  });
});
