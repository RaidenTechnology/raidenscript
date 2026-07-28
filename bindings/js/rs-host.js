// RaidenScript — tarayıcı/Node host köprüsü.
//
// capi.h'daki C yüzeyini kullanılabilir bir JS nesnesine sarar. Bundler yok,
// düz <script> ile çalışır (STAR BREAKER'ın kurulumu böyle).
//
// Kullanım:
//     const RS = await RaidenScriptHost.create();
//     const vm = RS.open({
//         game: {
//             spawnBullet: (x, y, aci) => sahne.mermiUret(x, y, aci),
//             playSfx:     (id)       => sfx.cal(id),
//         },
//     });
//     vm.eval(kaynakMetni, "plazma.rai");
//     vm.call("atesEt", [oyuncu.x, oyuncu.y]);
//     vm.close();
//
// SINIR: sayılar args[] dizisinden, dizeler capi.h'daki dize kanalından geçer
// (rs_arg_str / rs_return_str). JS tarafında bu görünmez — host fonksiyonun
// sıradan JS değerleri alır ve döndürür:
//
//     ui: {
//         girdi:  (alan)        => formdaki[alan],      // dize döner
//         yaz:    (metin, satir) => { ... },            // dize + sayı alır
//     }
//
// Uydurma serileştirme hâlâ YOK: liste/harita sınırdan geçmez. Geçmesi
// gerekiyorsa betik onu satır satır host'a yazar.

(function (global) {
  'use strict';

  // capi.h: double (*)(const char*, const char*, const double*, int, void*)
  const HOST_FN_SIG = 'diiiii';

  function create(opts) {
    opts = opts || {};
    const factory = opts.factory || global.createRaidenScript;
    if (typeof factory !== 'function') {
      throw new Error(
        'createRaidenScript bulunamadı — raidenscript.js bu dosyadan ÖNCE yüklenmeli'
      );
    }

    return factory(opts.moduleConfig || {}).then(function (Module) {
      return new RaidenScriptHost(Module);
    });
  }

  function RaidenScriptHost(Module) {
    this.M = Module;
  }

  // C tarafına geçici bir UTF-8 dizesi yazar. Dönen işaretçi serbest bırakılmalı.
  // NOT: HEAP görünümleri ALLOW_MEMORY_GROWTH ile yenilenebiliyor, o yüzden
  // hiçbir yerde önbelleğe alınmıyor — her erişimde Module'den okunuyor.
  function strYaz(M, s) {
    const n = M.lengthBytesUTF8(s) + 1;
    const p = M._malloc(n);
    M.stringToUTF8(s, p, n);
    return p;
  }

  // modules: { modulAdi: { fnAdi: (…sayilar) => sayi } }
  RaidenScriptHost.prototype.open = function (modules) {
    return new RaidenVM(this.M, modules || {});
  };

  function RaidenVM(Module, modules) {
    const M = Module;
    this.M = M;
    this.modules = modules;
    this.ptr = M._rs_new();
    if (!this.ptr) {
      throw new Error('rs_new başarısız — bellek yok');
    }
    this._kapali = false;

    const self = this;
    const dizeKanali = typeof M._rs_arg_str === 'function' &&
                       typeof M._rs_return_str === 'function';
    // Henüz C tarafında yok; eklendiği gün bu köprü kendiliğinden kullanmaya başlar.
    const hostFail = typeof M._rs_host_fail === 'function';

    // Betikten gelen çağrı buraya düşer. Sayılar HEAPF64'ten, dizeler
    // rs_arg_str'den okunur — hangisi olduğunu C tarafı biliyor, biz sormuyoruz.
    this._cbPtr = M.addFunction(function (modulPtr, fnPtr, argsPtr, argc, _user) {
      const modul = M.UTF8ToString(modulPtr);
      const fn = M.UTF8ToString(fnPtr);
      const args = new Array(argc);
      for (let i = 0; i < argc; i++) {
        // Dize kanalı olmayan ESKİ bir .wasm ile eşleşirsek sayı yoluna düş.
        // Bu köprü dosyası tek başına kopyalanabiliyor (STAR BREAKER öyle
        // yapıyor); eşleşmeyen çift sessiz çökme yerine eski davranışı versin.
        const sp = dizeKanali ? M._rs_arg_str(self.ptr, i) : 0;
        args[i] = sp ? M.UTF8ToString(sp) : M.HEAPF64[(argsPtr >> 3) + i];
      }
      const mod = self.modules[modul];
      const f = mod && mod[fn];
      if (typeof f !== 'function') {
        // Kayıtlı ama uygulanmamış: sessiz sıfır yerine gürültü çıkar, çünkü
        // sessiz sıfır oyunda "silah ateş etmiyor ama hata da yok"a dönüşür.
        console.error('RaidenScript: host fonksiyonu yok -> ' + modul + '.' + fn);
        return 0;
      }
      // İstisnanın WASM karelerinden geçmesine İZİN VERİLMEZ. Ölçüldü: kaçan bir JS
      // istisnası yığın işaretçisini geri almıyor, kaybedilen alan bir daha dönmüyor
      // (5.000 istisnada özyineleme derinliği 1000 -> 937, 50.000'de modül ölüyor).
      // Web bağlayıcısında host hatası kaçınılmaz: eksik düğüm, geçersiz seçici.
      let sonuc;
      try {
        sonuc = f.apply(null, args);
      } catch (err) {
        const mesaj = String((err && err.message) || err);
        if (hostFail) {
          // C tarafı destekliyorsa hata betiğe yakalanabilir bir Error olarak gider.
          const ep = strYaz(M, modul + '.' + fn + ': ' + mesaj);
          M._rs_host_fail(self.ptr, ep);
          M._free(ep);
          return 0;
        }
        // Desteklemiyorsa yapılabilecek en iyi şey gürültü çıkarmak: sessiz sıfır
        // kötü, ama modülün ölmesi daha kötü. rs_host_fail eklenince bu dal düşer.
        console.error('RaidenScript: host hatası yutuldu -> ' + modul + '.' + fn, err);
        return 0;
      }
      if (typeof sonuc === 'string') {
        if (!dizeKanali) {
          console.error('RaidenScript: bu .wasm dize kanalı olmadan derlenmiş — ' +
                        modul + '.' + fn + ' dize döndüremez (make wasm ile yenile)');
          return 0;
        }
        const p = strYaz(M, sonuc);
        M._rs_return_str(self.ptr, p);
        M._free(p);
        return 0;  // rs_return_str çağrıldıysa bu değer yok sayılır
      }
      return typeof sonuc === 'number' && isFinite(sonuc) ? sonuc : 0;
    }, HOST_FN_SIG);

    M._rs_set_host(this.ptr, this._cbPtr, 0);

    // SIRA ÖNEMLİ: kayıtlar eval'den önce yapılmalı — 'include game' eval
    // sırasında çözülüyor (capi.h).
    for (const modulAdi of Object.keys(this.modules)) {
      for (const fnAdi of Object.keys(this.modules[modulAdi])) {
        const mp = strYaz(M, modulAdi);
        const fp = strYaz(M, fnAdi);
        const rc = M._rs_register(this.ptr, mp, fp);
        M._free(mp);
        M._free(fp);
        if (rc !== 0) {
          throw new Error('rs_register başarısız: ' + this.lastError());
        }
      }
    }
  }

  RaidenVM.prototype.lastError = function () {
    return this.M.UTF8ToString(this.M._rs_last_error(this.ptr));
  };

  // Özyineleme sınırı (varsayılan 800). wasm yığını -sSTACK_SIZE=8MB ile
  // ~1000 kare taşıyor; sınır o duvarın altında kalsın diye var. Eski .wasm
  // dosyalarında bu dışa aktarım yok, o yüzden sessizce atlanıyor.
  RaidenVM.prototype.setMaxDepth = function (n) {
    this._kontrol();
    if (typeof this.M._rs_set_max_depth === 'function') {
      this.M._rs_set_max_depth(this.ptr, n | 0);
    }
  };

  RaidenVM.prototype._kontrol = function () {
    if (this._kapali) {
      throw new Error('bu VM kapatıldı');
    }
  };

  // Betiği yükler. Bir VM = bir betik (capi.h); ikincisi hata verir.
  RaidenVM.prototype.eval = function (kaynak, ad) {
    this._kontrol();
    const M = this.M;
    const kp = strYaz(M, kaynak);
    const ap = strYaz(M, ad || '<gomulu>');
    const rc = M._rs_eval(this.ptr, kp, ap);
    M._free(kp);
    M._free(ap);
    if (rc !== 0) {
      throw new Error(this.lastError());
    }
  };

  // Global fonksiyonu sayılarla çağırır, sayı döndürür.
  RaidenVM.prototype.call = function (fn, args) {
    this._kontrol();
    const M = this.M;
    args = args || [];
    const n = args.length;
    const fp = strYaz(M, fn);
    const ap = n > 0 ? M._malloc(n * 8) : 0;
    for (let i = 0; i < n; i++) {
      M.HEAPF64[(ap >> 3) + i] = args[i];
    }
    const op = M._malloc(8);
    const rc = M._rs_call(this.ptr, fp, ap, n, op);
    const sonuc = rc === 0 ? M.HEAPF64[op >> 3] : 0;
    M._free(fp);
    if (ap) {
      M._free(ap);
    }
    M._free(op);
    if (rc !== 0) {
      throw new Error(this.lastError());
    }
    return sonuc;
  };

  RaidenVM.prototype.close = function () {
    if (this._kapali) {
      return;
    }
    this.M._rs_free(this.ptr);
    if (this._cbPtr && this.M.removeFunction) {
      this.M.removeFunction(this._cbPtr);
    }
    this.ptr = 0;
    this._cbPtr = 0;
    this._kapali = true;
  };

  global.RaidenScriptHost = { create: create };
})(typeof window !== 'undefined' ? window : globalThis);
