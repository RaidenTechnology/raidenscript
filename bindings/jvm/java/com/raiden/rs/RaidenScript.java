package com.raiden.rs;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

/**
 * RaidenScript'in JVM köprüsü — capi.h'daki C yüzeyinin Java yüzü.
 *
 * Tasarım bindings/js/rs-host.js ile bilerek aynı: sayılar args dizisinden,
 * dizeler capi.h'daki ayrı kanaldan geçer. Java tarafında bu görünmez; host
 * fonksiyonu sıradan {@code Object[]} alır, {@link Double} ya da
 * {@link String} döndürür.
 *
 * <pre>
 *   RaidenScript.yukle(dllYolu);
 *   try (RaidenScript vm = RaidenScript.ac()) {
 *       vm.kaydet("mc", "mesaj", a -&gt; { ...; return 0.0; });
 *       vm.eval(kaynak, "enderchest.rai");
 *       vm.cagir("komut", 0);
 *   }
 * </pre>
 *
 * SIRA ÖNEMLİ: {@code kaydet} çağrıları {@code eval}'den ÖNCE yapılmalı —
 * 'include mc' satırı eval sırasında çözülüyor (capi.h).
 */
public final class RaidenScript implements AutoCloseable {

    /** Betikten host'a düşen çağrı. Sayı ya da dize döndürür (null = 0). */
    public interface HostFn extends Function<Object[], Object> { }

    private static boolean yuklendi = false;

    /** vm işaretçisi -> o VM'in modülleri. Statik, çünkü C geri çağrısı statik. */
    private static final Map<Long, Map<String, HostFn>> KAYIT = new HashMap<>();

    private final long ptr;
    private boolean kapali = false;

    private RaidenScript(long ptr) { this.ptr = ptr; }

    /** Yerel kütüphaneyi yükler. Yol mutlak olmalı (jar içinden çıkarılmış dosya). */
    public static synchronized void yukle(String dllYolu) {
        if (yuklendi) return;
        System.load(dllYolu);
        yuklendi = true;
    }

    public static RaidenScript ac() {
        if (!yuklendi) throw new IllegalStateException("önce RaidenScript.yukle(dll) çağrılmalı");
        long p = nOpen();
        if (p == 0) throw new IllegalStateException("rs_new başarısız — bellek yok");
        synchronized (KAYIT) { KAYIT.put(p, new HashMap<>()); }
        return new RaidenScript(p);
    }

    /** Host fonksiyonu kaydeder. eval'den ÖNCE çağrılmalı. */
    public void kaydet(String modul, String fn, HostFn islev) {
        kontrol();
        synchronized (KAYIT) { KAYIT.get(ptr).put(modul + "." + fn, islev); }
        if (nRegister(ptr, modul, fn) != 0) {
            throw new RaidenHatasi(sonHata());
        }
    }

    /** Bir modülün tüm fonksiyonlarını tek seferde kaydeder. */
    public void kaydetHepsi(String modul, Map<String, HostFn> islevler) {
        for (Map.Entry<String, HostFn> e : islevler.entrySet()) {
            kaydet(modul, e.getKey(), e.getValue());
        }
    }

    public void eval(String kaynak, String ad) {
        kontrol();
        if (nEval(ptr, kaynak, ad) != 0) throw new RaidenHatasi(sonHata());
    }

    /** Global bir betik fonksiyonunu çağırır. */
    public double cagir(String fn, double... args) {
        kontrol();
        double[] cikti = new double[1];
        int rc = nCall(ptr, fn, args, cikti);
        if (rc != 0) throw new RaidenHatasi(sonHata());
        return cikti[0];
    }

    public String sonHata() { return kapali ? "" : nLastError(ptr); }

    @Override public void close() {
        if (kapali) return;
        kapali = true;
        synchronized (KAYIT) { KAYIT.remove(ptr); }
        nClose(ptr);
    }

    private void kontrol() {
        if (kapali) throw new IllegalStateException("bu VM kapatıldı");
    }

    /**
     * C tarafının çağırdığı tek giriş noktası. rs_jni.cpp buraya düşüyor.
     *
     * Java istisnasının C++ karelerinin arasından geçmesine İZİN VERİLMEZ:
     * web köprüsünde ölçüldü, kaçan istisna yığın işaretçisini geri almıyor ve
     * kaybedilen alan bir daha dönmüyor. Burada yakalanıyor ve gürültü
     * çıkarılıyor — sessiz sıfır kötü, ama sunucunun çökmesi daha kötü.
     */
    @SuppressWarnings("unused")   // JNI çağırıyor
    private static Object hostCall(long vm, String modul, String fn, Object[] args) {
        HostFn islev;
        synchronized (KAYIT) {
            Map<String, HostFn> m = KAYIT.get(vm);
            islev = (m == null) ? null : m.get(modul + "." + fn);
        }
        if (islev == null) {
            System.err.println("RaidenScript: host fonksiyonu yok -> " + modul + "." + fn);
            return null;
        }
        try {
            return islev.apply(args);
        } catch (Throwable t) {
            System.err.println("RaidenScript: host hatası yutuldu -> " + modul + "." + fn
                               + " : " + t);
            t.printStackTrace();
            return null;
        }
    }

    // --- argüman okuma yardımcıları: betikten gelen değerler Double ya da String ---

    public static double sayi(Object[] a, int i) {
        if (i >= a.length) return 0;
        if (a[i] instanceof Double d) return d;
        try { return Double.parseDouble(String.valueOf(a[i])); } catch (Exception e) { return 0; }
    }

    public static int tam(Object[] a, int i) { return (int) sayi(a, i); }

    public static boolean mantik(Object[] a, int i) { return sayi(a, i) != 0; }

    public static String metin(Object[] a, int i) {
        if (i >= a.length) return "";
        return (a[i] instanceof String s) ? s : String.valueOf(a[i]);
    }

    /** Betiğe kaydedilmiş modül adlarını verir — tanı için. */
    public List<String> kayitliIsimler() {
        synchronized (KAYIT) {
            Map<String, HostFn> m = KAYIT.get(ptr);
            return (m == null) ? List.of() : new ArrayList<>(m.keySet());
        }
    }

    public static final class RaidenHatasi extends RuntimeException {
        public RaidenHatasi(String mesaj) { super(mesaj); }
    }

    // --- yerel yüzey (rs_jni.cpp) ---
    private static native long   nOpen();
    private static native int    nRegister(long p, String modul, String fn);
    private static native int    nEval(long p, String kaynak, String ad);
    private static native int    nCall(long p, String fn, double[] args, double[] cikti);
    private static native String nLastError(long p);
    private static native void   nClose(long p);
}
