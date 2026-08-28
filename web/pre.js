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
// Flush the user directory back to IndexedDB periodically.
Module['postRun'] = Module['postRun'] || [];
Module['postRun'].push(function () {
  setInterval(function () { try { FS.syncfs(false, function () {}); } catch (e) {} }, 5000);
});
