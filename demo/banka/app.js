// RAIDEN BANK — host tarafı.
//
// Burada BANKACILIK YOK. Bu dosya bir çizim aygıtı: banka.rai ne derse onu
// DOM'a koyar, form alanlarını sorulunca okur. Bakiye toplamıyor, IBAN
// doğrulamıyor, para biçimlendirmiyor, limit bilmiyor — hepsi betikte.
//
// Sınama: aşağıdaki ui.* gövdelerini terminale yazan sürümlerle değiştir,
// banka.rai'ye dokunmadan aynı uygulama terminalde çalışır.

(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);

  // Betiğin doldurabileceği metin alanları. Betik ui.metin("toplamVarlik", ...)
  // dediğinde buraya bakılır; listede olmayan bir ad sessizce yutulmaz.
  const ALANLAR = [
    'bankaAd', 'subeKodu', 'musteriNo', 'musteriAd', 'bugun',
    'toplamVarlik', 'toplamBirim',
    'gonderenAd', 'gonderenIban', 'gonderenBakiye', 'kalanLimit',
    'ekstreBaslik', 'ekstreOzet',
    'projeksiyonHesap', 'projeksiyonAnapara', 'projeksiyonOran',
  ];

  const BOLGELER = {
    hesaplar: 'hesapListesi',
    ekstre: 'ekstreGovde',
    projeksiyon: 'projeksiyonGovde',
  };

  let vm = null;

  function el(etiket, sinif, metin) {
    const d = document.createElement(etiket);
    if (sinif) d.className = sinif;
    if (metin !== undefined) d.textContent = metin;
    return d;
  }

  // --- betiğin çağırdığı ilkeller ---

  const ui = {
    // Bir bölgeyi boşalt.
    temizle: (bolge) => {
      const hedef = $(BOLGELER[bolge]);
      if (!hedef) {
        console.error('ui.temizle: unknown region -> ' + bolge);
        return 0;
      }
      hedef.replaceChildren();
      return 0;
    },

    // Tek bir metin alanını doldur.
    metin: (alan, deger) => {
      if (ALANLAR.indexOf(alan) < 0) {
        console.error('ui.metin: unknown field -> ' + alan);
        return 0;
      }
      const d = $(alan);
      if (d) d.textContent = deger;
      return 0;
    },

    // Hesap kartı. secili: 1/0 — betik karar verir, biz sadece sınıf ekleriz.
    hesapKarti: (no, ad, bakiye, birim, iban, secili) => {
      const kart = el('button', 'kart' + (secili ? ' kart-secili' : ''));
      kart.type = 'button';
      kart.appendChild(el('span', 'kart-ad', ad));
      const tutar = el('span', 'kart-tutar');
      tutar.appendChild(el('span', 'kart-para', bakiye));
      tutar.appendChild(el('span', 'kart-birim', birim));
      kart.appendChild(tutar);
      kart.appendChild(el('span', 'kart-iban', iban));
      kart.addEventListener('click', () => cagir('hesapSec', [no]));
      $('hesapListesi').appendChild(kart);
      return 0;
    },

    ekstreSatiri: (tarih, tur, tutar, bakiye, aciklama, artiMi) => {
      const tr = el('tr');
      tr.appendChild(el('td', 'kucuk', tarih));
      const rozet = el('td');
      rozet.appendChild(el('span', 'rozet rozet-' + (artiMi ? 'arti' : 'eksi'), tur));
      tr.appendChild(rozet);
      tr.appendChild(el('td', 'aciklama', aciklama));
      tr.appendChild(el('td', 'sayi ' + (artiMi ? 'arti' : 'eksi'), tutar));
      tr.appendChild(el('td', 'sayi soluk', bakiye));
      $('ekstreGovde').appendChild(tr);
      return 0;
    },

    projeksiyonSatiri: (yil, bakiye, kazanc) => {
      const tr = el('tr');
      tr.appendChild(el('td', 'kucuk', yil));
      tr.appendChild(el('td', 'sayi', bakiye));
      tr.appendChild(el('td', 'sayi arti', kazanc));
      $('projeksiyonGovde').appendChild(tr);
      return 0;
    },

    // tur: "ok" | "hata" | "bilgi"
    durum: (tur, baslik, satir) => {
      const kutu = $('durum');
      kutu.className = 'durum durum-' + tur + ' gorunur';
      kutu.replaceChildren();
      kutu.appendChild(el('strong', null, baslik));
      kutu.appendChild(el('span', null, satir));
      clearTimeout(ui._zaman);
      ui._zaman = setTimeout(() => kutu.classList.remove('gorunur'), 6000);
      return 0;
    },

    // Görünür ekranı değiştir: "giris" | "uygulama"
    ekran: (ad) => {
      $('girisEkrani').hidden = ad !== 'giris';
      $('uygulamaEkrani').hidden = ad !== 'uygulama';
      return 0;
    },

    // Betik form değerini ÇEKER. Host itmiyor — capi.h'daki desen bu.
    girdi: (alan) => {
      const d = $('alan_' + alan);
      return d ? d.value : '';
    },
  };

  // --- betiğe çağrı ---

  function cagir(fn, args) {
    try {
      return vm.call(fn, args || []);
    } catch (e) {
      // Betikten kaçan hata = kural boşluğu. Yutma, göster.
      ui.durum('hata', 'Script error', String(e.message).split('\n')[0]);
      console.error(e);
      return -1;
    }
  }

  // --- IBAN alanı canlı denetimi ---

  function ibanDurumu() {
    const kod = cagir('ibanDenetle', []);
    const ipucu = $('ibanDurum');
    const alan = $('alan_iban');
    alan.classList.remove('gecerli', 'gecersiz');
    if (kod === 1) {
      alan.classList.add('gecerli');
      ipucu.textContent = 'IBAN is valid';
      ipucu.className = 'ipucu arti';
    } else if (kod === 2) {
      alan.classList.add('gecersiz');
      ipucu.textContent = 'Check digits do not match';
      ipucu.className = 'ipucu eksi';
    } else if (kod === 3) {
      ipucu.textContent = 'a TR IBAN is 26 characters';
      ipucu.className = 'ipucu soluk';
    } else {
      ipucu.textContent = '';
      ipucu.className = 'ipucu';
    }
  }

  // --- kurulum ---

  async function baslat() {
    const RS = await window.RaidenScriptHost.create();
    vm = RS.open({ ui: ui });

    const kaynak = await fetch('banka.rai').then((r) => {
      if (!r.ok) throw new Error('could not read banka.rai (' + r.status + ')');
      return r.text();
    });

    vm.eval(kaynak, 'banka.rai');
    $('yukleniyor').hidden = true;
    cagir('baslat', []);

    $('girisFormu').addEventListener('submit', (e) => {
      e.preventDefault();
      cagir('girisYap', []);
    });
    $('transferFormu').addEventListener('submit', (e) => {
      e.preventDefault();
      cagir('transferYap', []);
    });
    $('alan_iban').addEventListener('input', ibanDurumu);

    document.querySelectorAll('.sekme').forEach((d) => {
      d.addEventListener('click', () => {
        document.querySelectorAll('.sekme').forEach((x) => x.classList.remove('etkin'));
        document.querySelectorAll('.panel').forEach((x) => (x.hidden = true));
        d.classList.add('etkin');
        $('panel_' + d.dataset.panel).hidden = false;
      });
    });

    console.log('RaidenScript bank demo ready — all of the logic lives in banka.rai');
  }

  baslat().catch((e) => {
    $('yukleniyor').textContent = 'Failed to load: ' + e.message;
    console.error(e);
  });
})();
