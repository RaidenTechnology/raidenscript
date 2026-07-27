// RaidenScript — web bağlayıcısı (DOM + animasyon).
//
// Burada UYGULAMA YOK. Bu dosya bir çizim aygıtı: betik "şu düğümü kur, şu
// zaman çizelgesini oynat" der, burası DOM'a çevirir. Ürün, fiyat, sepet,
// filtre diye bir kavram bu dosyada geçmez — hepsi .rai tarafında.
//
// Sınama, banka demosundakiyle aynı: aşağıdaki ilkellerin gövdelerini terminale
// yazan sürümlerle değiştir, betik değişmeden çalışsın.
//
// Kullanım:
//     const b  = RaidenWeb.olustur({ kok: 'kok' });
//     const vm = RS.open({ web: b.ilkeller });
//     b.baglaVM(vm);                       // olaylar betiğe buradan döner
//     vm.eval(kaynak, 'magaza.rai');
//     vm.call('baslat', []);
//
// TEMEL KARAR — animasyon BİLDİRİMSEL:
// Betik kare başına stil yazmaz. Zaman çizelgesini bir kez bildirir, oynatmayı
// tarayıcıya devreder (Web Animations API / ScrollTimeline). Böylece animasyon
// compositor'da koşar; betik uzun bir iş yapsa bile 60 fps düşmez. Ölçüm kare
// başına sürmenin de mümkün olduğunu söylüyor (200 öğe x 2 stil = 1.45 ms) ama
// mümkün olması doğru olması demek değil.

(function (global) {
  'use strict';

  function olustur(secenekler) {
    secenekler = secenekler || {};
    const kokId = secenekler.kok || 'kok';

    // id -> Element. Betik DOM'u görmez, isimle konuşur.
    const dugumler = new Map();
    // cizelgeAdi -> Map(yuzde -> { ozellik: deger })
    const cizelgeler = new Map();
    // Dinleyicileri sökebilmek için: id -> [{ hedef, olay, islev }]
    const dinleyiciler = new Map();
    const gozlemciler = [];

    let vm = null;
    let hataSayaci = 0;

    function kok() {
      const k = document.getElementById(kokId);
      if (!k) throw new Error('kök düğüm yok: #' + kokId);
      return k;
    }

    function bul(id) {
      if (id === '' || id === kokId) return kok();
      const d = dugumler.get(id);
      if (d) return d;
      // Sayfada elle yazılmış bir kabuk düğümü olabilir (hero, header...).
      const s = document.getElementById(id);
      if (s) {
        dugumler.set(id, s);
        return s;
      }
      return null;
    }

    // Bilinmeyen id sessizce yutulmaz: betikteki bir yazım hatası, ekranda
    // "hiçbir şey olmadı" diye değil, konsolda adıyla görünür.
    function gerek(ilkel, id) {
      const d = bul(id);
      if (!d) {
        hataSayaci++;
        console.error('web.' + ilkel + ': bilinmeyen düğüm -> "' + id + '"');
      }
      return d;
    }

    // Betiğe geri çağrı. Olaylar buradan .rai fonksiyonlarına düşer.
    function cagir(fnAdi, veri) {
      if (!vm) {
        console.error('web: VM bağlanmadan olay geldi -> ' + fnAdi);
        return 0;
      }
      try {
        return vm.call(fnAdi, [veri === undefined ? 0 : veri]);
      } catch (e) {
        // Betikten kaçan hata = kural boşluğu. Yutma, göster.
        console.error('RaidenScript hatası (' + fnAdi + '):\n' + e.message);
        return -1;
      }
    }

    // Zaman çizelgesini WAAPI'nin beklediği keyframe dizisine çevirir.
    function kareler(ad) {
      const c = cizelgeler.get(ad);
      if (!c) return null;
      return Array.from(c.keys())
        .sort((a, b) => a - b)
        .map((yuzde) => Object.assign({ offset: yuzde / 100 }, c.get(yuzde)));
    }

    // CSS özellik adı mı, custom property mi? "--" ile başlıyorsa setProperty.
    function stilYaz(el, ozellik, deger) {
      if (ozellik.slice(0, 2) === '--') el.style.setProperty(ozellik, deger);
      else el.style[ozellik] = deger;
    }

    const ilkeller = {

      // ---------------- yapı ----------------

      // Yeni düğüm oluşturur ve ustId'nin altına ekler. ustId "" ise köke.
      dugum: (ustId, id, etiket, sinif) => {
        const ust = ustId === '' ? kok() : gerek('dugum', ustId);
        if (!ust) return -1;
        if (dugumler.has(id)) {
          hataSayaci++;
          console.error('web.dugum: bu id zaten var -> "' + id + '"');
          return -1;
        }
        const el = document.createElement(etiket || 'div');
        if (sinif) el.className = sinif;
        // Gerçek DOM id'si de yazılıyor: betiğin kurduğu sayfa tarayıcı
        // denetleyicisinde okunabilsin, CSS '#id' ile seçebilsin, çapa (#kurucu)
        // bağlantıları çalışsın. dataset ayrı tutuluyor çünkü silme sırasında
        // kaydı düşürmek için kullanılıyor ve id'yi kullanıcı değiştirebilir.
        el.id = id;
        el.dataset.rsId = id;
        ust.appendChild(el);
        dugumler.set(id, el);
        return 0;
      },

      metin: (id, deger) => {
        const el = gerek('metin', id);
        if (!el) return -1;
        el.textContent = deger;
        return 0;
      },

      oznitelik: (id, ad, deger) => {
        const el = gerek('oznitelik', id);
        if (!el) return -1;
        if (deger === '') el.removeAttribute(ad);
        else el.setAttribute(ad, deger);
        return 0;
      },

      // acik: 1 ekle, 0 çıkar. Betik karar verir, biz sadece uygularız.
      sinif: (id, ad, acik) => {
        const el = gerek('sinif', id);
        if (!el) return -1;
        el.classList.toggle(ad, acik === 1);
        return 0;
      },

      stil: (id, ozellik, deger) => {
        const el = gerek('stil', id);
        if (!el) return -1;
        stilYaz(el, ozellik, String(deger));
        return 0;
      },

      // Düğümü ve altındaki her şeyi kaydından düşürerek siler.
      sil: (id) => {
        const el = bul(id);
        if (!el) return -1;
        unut(el);
        el.remove();
        return 0;
      },

      // Çocuklarını siler, kendisi kalır.
      temizle: (id) => {
        const el = gerek('temizle', id);
        if (!el) return -1;
        Array.from(el.children).forEach(unut);
        el.replaceChildren();
        return 0;
      },

      // ---------------- okuma ----------------

      girdi: (id) => {
        const el = bul(id);
        return el && 'value' in el ? String(el.value) : '';
      },

      // Düğümü görünür alana kaydırır. CSS'teki scroll-behavior yumuşatıyor.
      kaydir: (id) => {
        const el = gerek('kaydir', id);
        if (!el) return -1;
        el.scrollIntoView({ behavior: 'smooth', block: 'start' });
        return 0;
      },

      // "genislik" | "yukseklik" | "kaydirma" | "ustMesafe"
      olcu: (id, ne) => {
        const el = bul(id);
        if (!el) return 0;
        if (ne === 'kaydirma') return window.scrollY;
        const k = el.getBoundingClientRect();
        if (ne === 'genislik') return k.width;
        if (ne === 'yukseklik') return k.height;
        if (ne === 'ustMesafe') return k.top;
        return 0;
      },

      // ---------------- animasyon (bildirimsel) ----------------

      zaman: (ad) => {
        cizelgeler.set(ad, new Map());
        return 0;
      },

      anahtarKare: (ad, yuzde, ozellik, deger) => {
        const c = cizelgeler.get(ad);
        if (!c) {
          hataSayaci++;
          console.error('web.anahtarKare: tanımsız çizelge -> "' + ad + '"');
          return -1;
        }
        if (!c.has(yuzde)) c.set(yuzde, {});
        // WAAPI camelCase bekler; custom property'ler olduğu gibi geçer.
        c.get(yuzde)[ozellik] = String(deger);
        return 0;
      },

      // sure/gecikme ms. tekrar 0 = sonsuz. Buradan sonrası tarayıcının işi:
      // betik bir daha bu animasyona dokunmaz, sınır bir kez geçilir.
      oynat: (id, ad, sure, gecis, gecikme, tekrar) => {
        const el = gerek('oynat', id);
        if (!el) return -1;
        const kf = kareler(ad);
        if (!kf) {
          hataSayaci++;
          console.error('web.oynat: tanımsız çizelge -> "' + ad + '"');
          return -1;
        }
        el.animate(kf, {
          duration: sure,
          easing: gecis || 'linear',
          delay: gecikme || 0,
          iterations: tekrar === 0 ? Infinity : (tekrar || 1),
          fill: 'both',
        });
        return 0;
      },

      gecis: (id, ozellikler, sure, gecis) => {
        const el = gerek('gecis', id);
        if (!el) return -1;
        el.style.transition = ozellikler
          .split(',')
          .map((o) => o.trim() + ' ' + sure + 'ms ' + (gecis || 'ease'))
          .join(', ');
        return 0;
      },

      // Kaydırmaya bağlı animasyon. Destekleyen tarayıcıda compositor'da koşar;
      // desteklemeyende IntersectionObserver'lı kaba bir yedeğe düşer.
      kaydirmaBagla: (id, ad, baslangic, bitis) => {
        const el = gerek('kaydirmaBagla', id);
        if (!el) return -1;
        const kf = kareler(ad);
        if (!kf) return -1;
        if (typeof global.ViewTimeline === 'function') {
          el.animate(kf, {
            timeline: new global.ViewTimeline({ subject: el }),
            rangeStart: { rangeName: 'entry', offset: CSS.percent(baslangic) },
            rangeEnd: { rangeName: 'exit', offset: CSS.percent(bitis) },
            fill: 'both',
          });
          return 0;
        }
        const g = new IntersectionObserver(
          (girisler) => girisler.forEach((e) => {
            if (e.isIntersecting) el.animate(kf, { duration: 700, easing: 'ease-out', fill: 'both' });
          }),
          { threshold: 0.15 }
        );
        g.observe(el);
        gozlemciler.push(g);
        return 0;
      },

      // Sayfanın kaydırma ilerlemesine bağlar (üstteki ilerleme çubuğu gibi).
      // kaydirmaBagla düğümün kendi görünürlüğüne bakar; bu ise belgenin tamamına.
      sayfaBagla: (id, ad) => {
        const el = gerek('sayfaBagla', id);
        if (!el) return -1;
        const kf = kareler(ad);
        if (!kf) return -1;
        if (typeof global.ScrollTimeline === 'function') {
          el.animate(kf, { timeline: new global.ScrollTimeline({ source: document.documentElement }), fill: 'both' });
          return 0;
        }
        // Yedek: aynı işi olayla yap. Compositor'da koşmuyor ama görsel aynı.
        const guncelle = () => {
          const d = document.documentElement;
          const o = d.scrollHeight - d.clientHeight;
          el.style.transform = 'scaleX(' + (o > 0 ? d.scrollTop / o : 0) + ')';
        };
        window.addEventListener('scroll', guncelle, { passive: true });
        guncelle();
        return 0;
      },

      // ---------------- olaylar ----------------

      // Olay olunca betikteki fnAdi(veri) çağrılır. veri sayıdır — betik hangi
      // ürün/hangi satır olduğunu ondan çözer.
      dinle: (id, olay, fnAdi, veri) => {
        const el = gerek('dinle', id);
        if (!el) return -1;
        const islev = (e) => {
          if (olay === 'gonder' || olay === 'submit') e.preventDefault();
          cagir(fnAdi, veri);
        };
        const gercek = { gonder: 'submit', tikla: 'click', yaz: 'input',
                         degis: 'change', uzerine: 'pointerenter',
                         ayril: 'pointerleave' }[olay] || olay;
        el.addEventListener(gercek, islev);
        if (!dinleyiciler.has(id)) dinleyiciler.set(id, []);
        dinleyiciler.get(id).push({ hedef: el, olay: gercek, islev });
        return 0;
      },

      // Düğüm görünür olunca bir kez fnAdi(veri) çağrılır — scroll reveal.
      gorununce: (id, fnAdi, esik, veri) => {
        const el = gerek('gorununce', id);
        if (!el) return -1;
        const g = new IntersectionObserver((girisler) => {
          girisler.forEach((e) => {
            if (!e.isIntersecting) return;
            g.unobserve(e.target);
            cagir(fnAdi, veri);
          });
        }, { threshold: Math.max(0, Math.min(1, esik / 100)) });
        g.observe(el);
        gozlemciler.push(g);
        return 0;
      },
    };

    // Silinen ağaçtaki id'leri ve dinleyicileri kayıttan düşür — yoksa bul()
    // DOM'dan kopmuş düğümler döndürmeye devam eder ve hata sessizleşir.
    function unut(el) {
      if (el.nodeType !== 1) return;
      const id = el.dataset && el.dataset.rsId;
      if (id) {
        dugumler.delete(id);
        const ds = dinleyiciler.get(id);
        if (ds) {
          ds.forEach((d) => d.hedef.removeEventListener(d.olay, d.islev));
          dinleyiciler.delete(id);
        }
      }
      Array.from(el.children).forEach(unut);
    }

    return {
      ilkeller: ilkeller,
      baglaVM: (v) => { vm = v; },
      cagir: cagir,
      hataSayisi: () => hataSayaci,
      dugumSayisi: () => dugumler.size,
    };
  }

  global.RaidenWeb = { olustur: olustur };
})(typeof window !== 'undefined' ? window : globalThis);
