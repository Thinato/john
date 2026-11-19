// Web build: songs, techniques and settings live in /john (dataDir() in src/song.cpp), a folder backed by
// IndexedDB. Load it before main() runs; persist() in src/song.cpp writes it back after every save.
Module.preRun = Module.preRun || [];
Module.preRun.push(() => {
  FS.mkdir('/john');
  FS.mount(IDBFS, {}, '/john');
  addRunDependency('idbfs');
  FS.syncfs(true, (err) => {
    if (err) console.error('John: loading saved data from IndexedDB failed', err);
    removeRunDependency('idbfs');
  });
});
