// RAIDEN PARÇA — host tarafı.
//
// Burada MAĞAZA YOK. Bu dosyanın tüm işi: WASM'ı ayağa kaldır, web bağlayıcısını
// VM'e tak, magaza.rai'yi yükle ve baslat()'ı çağır. Ürün, fiyat, sepet, filtre
// diye bir kelime geçmiyor — hepsi betikte.
//
// Sınama: bindings/js/web.js'i terminale yazan bir sürümle değiştir, magaza.rai
// tek satır değişmeden çalışsın.

(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);

  async function baslat() {
    const t0 = performance.now();

    const RS = await window.RaidenScriptHost.create();
    const baglayici = window.RaidenWeb.olustur({ kok: 'kok' });

    const vm = RS.open({ web: baglayici.ilkeller });
    baglayici.baglaVM(vm);

    const kaynak = await fetch('magaza.rai').then((r) => {
      if (!r.ok) throw new Error('magaza.rai okunamadı (' + r.status + ')');
      return r.text();
    });

    const tEval = performance.now();
    vm.eval(kaynak, 'magaza.rai');
    const tCiz = performance.now();
    vm.call('baslat', []);
    const tSon = performance.now();

    $('yukleniyor').classList.add('yukleniyor-gizli');

    // Ölçüm, iddiayı yerinde tutmak için: sayfayı kuran betiğin gerçekten ne
    // kadar sürdüğü konsolda görünsün.
    console.log(
      '%cRAIDEN PARÇA%c  yükleme ' + (tEval - t0).toFixed(0) + ' ms · ' +
      'betik derleme ' + (tCiz - tEval).toFixed(1) + ' ms · ' +
      'sayfa kurulumu ' + (tSon - tCiz).toFixed(1) + ' ms · ' +
      baglayici.dugumSayisi() + ' düğüm · ' +
      baglayici.hataSayisi() + ' host hatası',
      'font-weight:700;color:#2de2c8', 'color:#8b93a8'
    );

    // Fare takip eden parlama — saf dekor, betiği ilgilendirmiyor.
    const parlama = $('fonParlama');
    window.addEventListener('pointermove', (e) => {
      parlama.style.transform = 'translate3d(' + e.clientX + 'px,' + e.clientY + 'px,0)';
    }, { passive: true });

    // Kaçak yok: betik hatası varsa görünsün.
    window.rsTani = () => ({
      dugum: baglayici.dugumSayisi(),
      hata: baglayici.hataSayisi(),
    });
  }

  baslat().catch((e) => {
    const k = $('yukleniyor');
    k.textContent = 'Yüklenemedi: ' + e.message;
    console.error(e);
  });
})();
