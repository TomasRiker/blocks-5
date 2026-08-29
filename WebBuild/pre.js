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

Module['postRun'] = Module['postRun'] || [];
Module['postRun'].push(function () {
  setInterval(Module['b5_sync'], 5000);
  // A tab can be discarded without warning; pagehide is the last reliable hook.
  window.addEventListener('pagehide', Module['b5_sync']);
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) Module['b5_sync']();
  });
});
