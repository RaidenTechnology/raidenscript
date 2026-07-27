package com.raiden.ec;

import com.raiden.rs.RaidenScript;
import org.bukkit.Bukkit;
import org.bukkit.Particle;
import org.bukkit.entity.Player;
import org.bukkit.plugin.Plugin;

import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

/**
 * mc.* bağlayıcısı — RaidenScript'in Minecraft yüzü.
 *
 * Burada PLUGIN YOK. Tek bir kural, tek bir izin adı, tek bir mesaj metni
 * burada geçmez: bunlar {@code enderchest.rai} içinde. Buradaki her fonksiyon
 * bir ilkel — "şu oyuncuya şu metni yaz", "şu izni sorgula", "ender sandığı aç".
 *
 * Sınama, banka ve mağaza demolarındakiyle aynı: bu sınıfın gövdelerini konsola
 * yazan sürümlerle değiştir, betik değişmeden çalışsın.
 *
 * SINIR: capi.h yalnızca sayı ve dize taşıyor, nesne taşımıyor. Oyuncu kimliği
 * bu yüzden UUID dizesi olarak geçiyor. Betiğe dize ARGÜMANI da geçilemiyor
 * (rs_call sadece double alıyor), o yüzden komut bağlamını betik ÇEKİYOR:
 * {@code mc.komutOyuncu()}, {@code mc.komutArg(0)}.
 */
public final class McBaglayici {

    private final Plugin plugin;

    /** O an işlenen komutun bağlamı. Komutlar ana iş parçacığında sırayla gelir. */
    private UUID komutOyuncu;
    private String[] komutArgs = new String[0];

    public McBaglayici(Plugin plugin) { this.plugin = plugin; }

    public void baglamKur(UUID oyuncu, String[] args) {
        this.komutOyuncu = oyuncu;
        this.komutArgs = args;
    }

    private Player oyuncu(String uuid) {
        try {
            return Bukkit.getPlayer(UUID.fromString(uuid));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    public Map<String, RaidenScript.HostFn> ilkeller() {
        Map<String, RaidenScript.HostFn> m = new HashMap<>();

        // --- komut bağlamı (betik çeker) ---
        m.put("komutOyuncu", a -> komutOyuncu == null ? "" : komutOyuncu.toString());
        m.put("komutArgSayisi", a -> (double) komutArgs.length);
        m.put("komutArg", a -> {
            int i = RaidenScript.tam(a, 0);
            return (i >= 0 && i < komutArgs.length) ? komutArgs[i] : "";
        });

        // --- oyuncu ---
        m.put("ad", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            return p == null ? "" : p.getName();
        });
        m.put("oyuncuBul", a -> {
            Player p = Bukkit.getPlayerExact(RaidenScript.metin(a, 0));
            return p == null ? "" : p.getUniqueId().toString();
        });
        m.put("izinVar", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            return (p != null && p.hasPermission(RaidenScript.metin(a, 1))) ? 1.0 : 0.0;
        });
        m.put("mesaj", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p != null) p.sendMessage(RaidenScript.metin(a, 1));
            return 0.0;
        });

        // --- dünya ---
        m.put("enderAc", a -> {
            Player izleyen = oyuncu(RaidenScript.metin(a, 0));
            Player hedef = oyuncu(RaidenScript.metin(a, 1));
            if (izleyen == null || hedef == null) return 0.0;
            izleyen.openInventory(hedef.getEnderChest());
            return 1.0;
        });
        m.put("ses", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p == null) return 0.0;
            p.playSound(p.getLocation(), RaidenScript.metin(a, 1),
                        (float) RaidenScript.sayi(a, 2), (float) RaidenScript.sayi(a, 3));
            return 0.0;
        });
        m.put("parcacik", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p == null) return 0.0;
            Particle tur;
            try {
                tur = Particle.valueOf(RaidenScript.metin(a, 1).toUpperCase(java.util.Locale.ROOT));
            } catch (IllegalArgumentException e) {
                // Bilinmeyen parçacık adı sessizce yutulmaz: betikteki yazım hatası
                // ekranda "hiçbir şey olmadı" diye değil, günlükte adıyla görünür.
                plugin.getLogger().warning("mc.parcacik: bilinmeyen parçacık -> "
                                           + RaidenScript.metin(a, 1));
                return 0.0;
            }
            p.getWorld().spawnParticle(tur, p.getLocation().add(0, 1, 0),
                                       RaidenScript.tam(a, 2), 0.4, 0.6, 0.4, 0.02);
            return 0.0;
        });

        // --- ortam ---
        m.put("zaman", a -> System.currentTimeMillis() / 1000.0);
        m.put("log", a -> { plugin.getLogger().info(RaidenScript.metin(a, 0)); return 0.0; });
        m.put("cevrimici", a -> oyuncu(RaidenScript.metin(a, 0)) != null ? 1.0 : 0.0);

        return m;
    }
}
