// Web build: songs, techniques and settings live in /john (dataDir() in src/song.cpp), a folder backed by
// IndexedDB. Load it before main() runs; persist() in src/song.cpp writes it back after every save.
// coi-serviceworker.js (web/shell.html) reloads the page once, on the first visit, to make it cross-origin isolated
// (#75). Don't open IndexedDB before that reload: a database opened (and created) just before it stayed locked for the
// reloaded page in Chrome, which then never started. So on a first visit the load waits 4 s, by which time the reload
// has normally happened; if it didn't (the browser refused the service worker), John starts anyway.
const coiReloadComing = self.crossOriginIsolated === false && window.isSecureContext && navigator.serviceWorker && !navigator.serviceWorker.controller;
Module.preRun = Module.preRun || [];
Module.preRun.push(() => {
  FS.mkdir('/john');
  FS.mount(IDBFS, {}, '/john');
  addRunDependency('idbfs');
  setTimeout(() => FS.syncfs(true, (err) => {
    if (err) console.error('John: loading saved data from IndexedDB failed', err);
    removeRunDependency('idbfs');
  }), coiReloadComing ? 4000 : 0);
});
