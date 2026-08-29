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
