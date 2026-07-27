// WASM koşumu — Faz 4 / adım 2-3.
//
// Aynı C API'sinin emscripten hedefinde de çalıştığını ve JS köprüsünün
// (bindings/js/rs-host.js) doğru bağladığını doğrular.
//
// Koşum:  node tests/wasm_test.js       (önce: make wasm)

const path = require('path');

let gecen = 0;
let kalan = 0;

function bildir(tamam, ad, ek) {
  if (tamam) {
    gecen++;
    console.log('  ok    ' + ad);
  } else {
    kalan++;
    console.log('  HATA  ' + ad + (ek ? '  <- ' + ek : ''));
  }
}

const yakin = (a, b) => Math.abs(a - b) < 1e-9;

async function main() {
  const wasmYol = path.join(__dirname, '..', 'dist', 'raidenscript.js');
  globalThis.createRaidenScript = require(wasmYol);
  require(path.join(__dirname, '..', 'bindings', 'js', 'rs-host.js'));

  const RS = await globalThis.RaidenScriptHost.create();
  console.log('RaidenScript WASM kosumu');

  // 1 — argümanlı çağrı
  {
    const vm = RS.open({});
    vm.eval('fn topla(a, b):\n    return a + b\n', 'w1.rai');
    const out = vm.call('topla', [3, 4]);
    bildir(yakin(out, 7), 'wasm: argumanli cagri 3+4 -> 7', 'deger ' + out);
    vm.close();
  }

  // 2 — host köprüsü: JS fonksiyonu betikten çağrılıyor
  {
    let gorulen = null;
    const vm = RS.open({
      game: {
        zar: (n) => {
          gorulen = n;
          return 42;
        },
      },
    });
    vm.eval('include game\n\nfn at():\n    return game.zar(5)\n', 'w2.rai');
    const out = vm.call('at', []);
    bildir(yakin(out, 42) && yakin(gorulen, 5), 'wasm: include game -> game.zar(5)',
           'gorulen=' + gorulen + ' out=' + out);
    vm.close();
  }

  // 3 — sözdizimi hatası tam tanı metniyle fırlıyor
  {
    const vm = RS.open({});
    let mesaj = '';
    try {
      vm.eval('c = 5 ! 3\n', 'w3.rai');
    } catch (e) {
      mesaj = String(e.message);
    }
    const tamam = mesaj.includes('hata:') && mesaj.includes('-->') &&
                  mesaj.includes('^') && mesaj.includes('ipucu:');
    bildir(tamam, 'wasm: sozdizimi hatasi konum + ok + ipucu ile firliyor',
           tamam ? '' : mesaj.slice(0, 120));
    vm.close();
  }

  // 4 — çok sayıda çağrı: JS<->WASM sınırında sızıntı/bozulma olmamalı
  {
    const vm = RS.open({ game: { ekle: (a, b) => a + b } });
    vm.eval('include game\n\nfn topla(a, b):\n    return game.ekle(a, b)\n', 'w4.rai');
    let tamam = true;
    for (let i = 0; i < 500; i++) {
      if (!yakin(vm.call('topla', [i, i]), i * 2)) {
        tamam = false;
        break;
      }
    }
    bildir(tamam, 'wasm: 500 ardisik host cagrisi');
    vm.close();
  }

  // 5 — birden fazla VM aynı anda (bir VM = bir betik kuralının pratiği)
  {
    const a = RS.open({});
    const b = RS.open({});
    a.eval('fn v():\n    return 1\n', 'w5a.rai');
    b.eval('fn v():\n    return 2\n', 'w5b.rai');
    const tamam = yakin(a.call('v', []), 1) && yakin(b.call('v', []), 2);
    bildir(tamam, 'wasm: iki VM birbirini bozmuyor');
    a.close();
    b.close();
  }

  // 6 — dize kanalı: JS host fonksiyonu dize alıp dize döndürüyor
  {
    const form = { iban: 'TR33 0006 1005 1978 6457 8413 26', tutar: '250,00' };
    const yazilan = [];
    const vm = RS.open({
      ui: {
        girdi: (alan) => form[alan] || '',
        yaz: (metin, seviye) => {
          yazilan.push(seviye + ':' + metin);
          return yazilan.length;
        },
      },
    });
    vm.eval(
      'include ui\n' +
      'fn calis():\n' +
      '    iban = ui.girdi("iban")\n' +
      '    temiz = iban.replaceAll(" ", "")\n' +
      '    ui.yaz(f"uzunluk {len(temiz)}", 1)\n' +
      '    ui.yaz(temiz.upper(), 2)\n' +
      '    return len(temiz)\n',
      'w6.rai'
    );
    const out = vm.call('calis', []);
    const tamam = yakin(out, 26) &&
                  yazilan.length === 2 &&
                  yazilan[0] === '1:uzunluk 26' &&
                  yazilan[1] === '2:TR330006100519786457841326';
    bildir(tamam, 'wasm: dize kanali (host -> betik -> host)',
           tamam ? '' : JSON.stringify(yazilan) + ' out=' + out);
    vm.close();
  }

  // 7 — dize ve sayı aynı çağrıda karışmıyor
  {
    let gorulen = null;
    const vm = RS.open({ ui: { kart: (ad, tutar, birim) => { gorulen = [ad, tutar, birim]; return 0; } } });
    vm.eval('include ui\n\nfn ciz():\n    return ui.kart("Vadesiz", 4782560, "TRY")\n', 'w7.rai');
    vm.call('ciz', []);
    const tamam = gorulen && gorulen[0] === 'Vadesiz' &&
                  gorulen[1] === 4782560 && gorulen[2] === 'TRY';
    bildir(tamam, 'wasm: dize/sayi/dize ayni cagrida karismiyor', JSON.stringify(gorulen));
    vm.close();
  }

  console.log('\ngecen: ' + gecen + '  kalan: ' + kalan);
  process.exit(kalan === 0 ? 0 : 1);
}

main().catch((e) => {
  console.error('kosum coktu: ' + (e && e.stack ? e.stack : e));
  process.exit(1);
});
