package com.raiden.sistem;

import com.raiden.rs.RaidenScript;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.TextDecoration;
import net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer;
import org.bukkit.Bukkit;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Particle;
import org.bukkit.entity.Player;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.meta.ItemMeta;
import org.bukkit.persistence.PersistentDataContainer;
import org.bukkit.persistence.PersistentDataType;
import org.bukkit.plugin.Plugin;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;

/**
 * mc.* bağlayıcısı — RaidenScript'in Minecraft yüzü.
 *
 * Burada SİSTEM YOK. Hangi büyü var, bekleme kaç saniye, hangi izin ne yapar,
 * hangi mesaj ne yazar — hiçbiri burada. Hepsi sistem.rai içinde. Buradaki her
 * fonksiyon bir ilkel: "sandık arayüzü aç", "şu yuvaya şu eşyayı koy", "ender
 * sandığını aç", "giriş eşyasına şu statı ekle", "XP düş".
 *
 * Sınama, banka ve mağaza demolarındakiyle aynı: bu sınıfın gövdelerini konsola
 * yazan sürümlerle değiştir, betik değişmeden çalışsın.
 *
 * ENTEGRASYON: statlar raiden-combatstats'ın kullandığı ŞEMAYA yazılıyor --
 * namespace "raidenrpg", anahtar "rpg_stat_<stat>" (DOUBLE), DELTA olarak.
 * Böylece mevcut plugin'lerin tek satırı değişmeden büyüler savaş formülüne
 * giriyor ve eşyanın kendi temel statı ezilmiyor.
 *
 * SINIR: capi.h yalnızca sayı ve dize taşıyor, nesne taşımıyor. Oyuncu kimliği
 * bu yüzden UUID dizesi. Betiğe dize ARGÜMANI da geçilemiyor (rs_call sadece
 * double alıyor), o yüzden komut bağlamını betik ÇEKİYOR: mc.komutAdi(),
 * mc.komutOyuncu(), mc.komutArg(0).
 */
public final class McBaglayici {

    /** raiden-combatstats/RpgKeys ile AYNI olmak zorunda. */
    private static final String RPG_NS = "raidenrpg";
    private static final String BUYU_ONEK = "rai_buyu_";

    /** raiden-combatstats/Rarity'deki görünen adlar. */
    private static final String[] NADIRLIKLER =
            { "Sıradan", "Seyrek", "Nadir", "Epik", "Efsanevi", "Mistik", "Eşsiz" };

    /** Büyü masasındaki giriş yuvası. Betik de bu sayıyı mc.girisYuvasi ile alıyor. */
    public static final int GIRIS_YUVASI = 13;

    private final Plugin plugin;
    private final Map<UUID, Inventory> acikMasalar = new HashMap<>();

    private UUID komutOyuncu;
    private String komutAdi = "";
    private String[] komutArgs = new String[0];
    private int sonTiklananYuva = -1;

    public McBaglayici(Plugin plugin) { this.plugin = plugin; }

    public void baglamKur(UUID oyuncu, String komut, String[] args) {
        this.komutOyuncu = oyuncu;
        this.komutAdi = komut;
        this.komutArgs = args;
    }

    public void tiklamaKur(UUID oyuncu, int yuva) {
        this.komutOyuncu = oyuncu;
        this.sonTiklananYuva = yuva;
    }

    public Inventory masasi(UUID u) { return acikMasalar.get(u); }
    public void masayiUnut(UUID u) { acikMasalar.remove(u); }

    private Player oyuncu(String uuid) {
        try { return Bukkit.getPlayer(UUID.fromString(uuid)); }
        catch (IllegalArgumentException e) { return null; }
    }

    private Inventory masa(String uuid) {
        try { return acikMasalar.get(UUID.fromString(uuid)); }
        catch (IllegalArgumentException e) { return null; }
    }

    private ItemStack giris(String uuid) {
        Inventory inv = masa(uuid);
        if (inv == null) return null;
        ItemStack it = inv.getItem(GIRIS_YUVASI);
        return (it == null || it.getType() == Material.AIR) ? null : it;
    }

    /*
     * NamespacedKey yalnızca [a-z0-9/._-] kabul ediyor. Betikteki büyü id'leri
     * camelCase ("celikDeri", "raidenMuhru") ve bunlar doğrudan anahtar yapılınca
     * IllegalArgumentException atıyor -- gerçek sunucuda böyle yakalandı:
     *
     *   Invalid key. Must be [a-z0-9/._-]: rai_buyu_celikDeri
     *
     * Sonuç sessiz değildi ama görünmezdi: köprünün try/catch'i istisnayı yutup
     * 0 döndürüyordu, yani o büyüler "hiç takılı değil" gibi davranıyordu.
     * Çözüm burada, betikte değil -- id'leri Java'nın kısıtına uydurmak için
     * betiği çirkinleştirmek yanlış olurdu.
     */
    private static String pdcAd(String ham) {
        return ham.toLowerCase(Locale.ROOT).replaceAll("[^a-z0-9/._-]", "_");
    }

    private static NamespacedKey anahtar(String ad) {
        @SuppressWarnings("deprecation")
        NamespacedKey k = new NamespacedKey(RPG_NS, pdcAd(ad));
        return k;
    }

    private static Component bilesen(String legacy) {
        return LegacyComponentSerializer.legacySection().deserialize(legacy)
                .decoration(TextDecoration.ITALIC, false);
    }

    private static String duzMetin(Component c) {
        return LegacyComponentSerializer.legacySection().serialize(c);
    }

    private static String renksiz(String s) {
        return s.replaceAll("§[0-9a-fk-orA-FK-OR]", "");
    }

    public Map<String, RaidenScript.HostFn> ilkeller() {
        Map<String, RaidenScript.HostFn> m = new HashMap<>();

        // ---------------- komut bağlamı (betik çeker) ----------------
        m.put("komutAdi", a -> komutAdi);
        m.put("komutOyuncu", a -> komutOyuncu == null ? "" : komutOyuncu.toString());
        m.put("komutArgSayisi", a -> (double) komutArgs.length);
        m.put("komutArg", a -> {
            int i = RaidenScript.tam(a, 0);
            return (i >= 0 && i < komutArgs.length) ? komutArgs[i] : "";
        });
        m.put("tiklananYuva", a -> (double) sonTiklananYuva);
        m.put("girisYuvasi", a -> (double) GIRIS_YUVASI);

        // ---------------- oyuncu ----------------
        m.put("ad", a -> { Player p = oyuncu(RaidenScript.metin(a, 0)); return p == null ? "" : p.getName(); });
        m.put("oyuncuBul", a -> {
            Player p = Bukkit.getPlayerExact(RaidenScript.metin(a, 0));
            return p == null ? "" : p.getUniqueId().toString();
        });
        m.put("cevrimici", a -> oyuncu(RaidenScript.metin(a, 0)) != null ? 1.0 : 0.0);
        m.put("izinVar", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            return (p != null && p.hasPermission(RaidenScript.metin(a, 1))) ? 1.0 : 0.0;
        });
        m.put("mesaj", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p != null) p.sendMessage(bilesen(RaidenScript.metin(a, 1)));
            return 0.0;
        });
        m.put("xpSeviye", a -> { Player p = oyuncu(RaidenScript.metin(a, 0)); return p == null ? 0.0 : p.getLevel(); });
        m.put("xpDus", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            int n = RaidenScript.tam(a, 1);
            if (p == null || p.getLevel() < n) return 0.0;
            p.setLevel(p.getLevel() - n);
            return 1.0;
        });

        // ---------------- dünya ----------------
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
                tur = Particle.valueOf(RaidenScript.metin(a, 1).toUpperCase(Locale.ROOT));
            } catch (IllegalArgumentException e) {
                plugin.getLogger().warning("mc.parcacik: bilinmeyen parçacık -> " + RaidenScript.metin(a, 1));
                return 0.0;
            }
            p.getWorld().spawnParticle(tur, p.getLocation().add(0, 1, 0),
                                       RaidenScript.tam(a, 2), 0.4, 0.6, 0.4, 0.02);
            return 0.0;
        });
        m.put("zaman", a -> System.currentTimeMillis() / 1000.0);
        m.put("log", a -> { plugin.getLogger().info(RaidenScript.metin(a, 0)); return 0.0; });

        // ---------------- arayüz ----------------
        m.put("guiAc", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p == null) return 0.0;
            int satir = Math.max(1, Math.min(6, RaidenScript.tam(a, 2)));
            Inventory inv = Bukkit.createInventory(new MasaSahibi(), satir * 9,
                    bilesen(RaidenScript.metin(a, 1)));
            acikMasalar.put(p.getUniqueId(), inv);
            p.openInventory(inv);
            return 1.0;
        });
        m.put("guiEsya", a -> {
            Inventory inv = masa(RaidenScript.metin(a, 0));
            if (inv == null) return 0.0;
            int yuva = RaidenScript.tam(a, 1);
            if (yuva < 0 || yuva >= inv.getSize()) return 0.0;
            Material mat = Material.matchMaterial(RaidenScript.metin(a, 2).toUpperCase(Locale.ROOT));
            if (mat == null) {
                plugin.getLogger().warning("mc.guiEsya: bilinmeyen materyal -> " + RaidenScript.metin(a, 2));
                return 0.0;
            }
            ItemStack it = new ItemStack(mat, Math.max(1, Math.min(64, RaidenScript.tam(a, 4))));
            ItemMeta meta = it.getItemMeta();
            String ad = RaidenScript.metin(a, 3);
            if (!ad.isEmpty()) meta.displayName(bilesen(ad));
            it.setItemMeta(meta);
            inv.setItem(yuva, it);
            return 1.0;
        });
        m.put("guiLore", a -> {
            Inventory inv = masa(RaidenScript.metin(a, 0));
            if (inv == null) return 0.0;
            ItemStack it = inv.getItem(RaidenScript.tam(a, 1));
            if (it == null) return 0.0;
            ItemMeta meta = it.getItemMeta();
            List<Component> lore = meta.lore() == null ? new ArrayList<>() : new ArrayList<>(meta.lore());
            lore.add(bilesen(RaidenScript.metin(a, 2)));
            meta.lore(lore);
            it.setItemMeta(meta);
            return 1.0;
        });
        m.put("guiBosalt", a -> {
            Inventory inv = masa(RaidenScript.metin(a, 0));
            if (inv == null) return 0.0;
            for (int i = 0; i < inv.getSize(); i++) {
                if (i != GIRIS_YUVASI) inv.setItem(i, null);
            }
            return 1.0;
        });

        // ---------------- giriş eşyası ----------------
        m.put("girisVar", a -> giris(RaidenScript.metin(a, 0)) != null ? 1.0 : 0.0);
        m.put("girisAd", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return "";
            ItemMeta meta = it.getItemMeta();
            return (meta != null && meta.displayName() != null) ? duzMetin(meta.displayName()) : it.getType().name();
        });
        m.put("girisMateryal", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            return it == null ? "" : it.getType().name();
        });
        m.put("girisTur", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return "";
            String t = it.getType().name();
            if (t.endsWith("_SWORD") || t.endsWith("_AXE") || t.equals("TRIDENT")
                || t.equals("BOW") || t.equals("CROSSBOW")) return "silah";
            if (t.endsWith("_HELMET") || t.endsWith("_CHESTPLATE")
                || t.endsWith("_LEGGINGS") || t.endsWith("_BOOTS")) return "zirh";
            return "diger";
        });
        /* Nadirlik lore'un son satırında (raiden-combatstats/Rarity). Renk kodları
         * temizlenip bilinen adlarla eşleştiriliyor; eşleşme yoksa "" dönüyor ve
         * kararı betik veriyor. */
        m.put("girisNadirlik", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return "";
            List<Component> lore = it.getItemMeta().lore();
            if (lore == null || lore.isEmpty()) return "";
            for (int i = lore.size() - 1; i >= 0; i--) {
                String satir = renksiz(duzMetin(lore.get(i))).trim();
                for (String n : NADIRLIKLER) {
                    if (satir.contains(n) || satir.contains(n.toUpperCase(new Locale("tr")))) return n;
                }
            }
            return "";
        });
        m.put("girisBuyuSeviye", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return 0.0;
            Integer v = it.getItemMeta().getPersistentDataContainer()
                    .get(anahtar(BUYU_ONEK + RaidenScript.metin(a, 1)), PersistentDataType.INTEGER);
            return v == null ? 0.0 : v;
        });
        m.put("girisBuyuSayisi", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return 0.0;
            PersistentDataContainer pdc = it.getItemMeta().getPersistentDataContainer();
            return (double) pdc.getKeys().stream()
                    .filter(k -> k.getNamespace().equals(RPG_NS) && k.getKey().startsWith(BUYU_ONEK))
                    .count();
        });
        m.put("girisBuyuYaz", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return 0.0;
            ItemMeta meta = it.getItemMeta();
            meta.getPersistentDataContainer().set(anahtar(BUYU_ONEK + RaidenScript.metin(a, 1)),
                    PersistentDataType.INTEGER, RaidenScript.tam(a, 2));
            it.setItemMeta(meta);
            return 1.0;
        });
        m.put("girisStatEkle", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return 0.0;
            String stat = RaidenScript.metin(a, 1).toLowerCase(Locale.ROOT);
            ItemMeta meta = it.getItemMeta();
            NamespacedKey k = anahtar("rpg_stat_" + stat);
            Double eski = meta.getPersistentDataContainer().get(k, PersistentDataType.DOUBLE);
            meta.getPersistentDataContainer().set(k, PersistentDataType.DOUBLE,
                    (eski == null ? 0.0 : eski) + RaidenScript.sayi(a, 2));
            it.setItemMeta(meta);
            return 1.0;
        });
        m.put("girisLoreEkle", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return 0.0;
            ItemMeta meta = it.getItemMeta();
            List<Component> lore = meta.lore() == null ? new ArrayList<>() : new ArrayList<>(meta.lore());
            lore.add(bilesen(RaidenScript.metin(a, 1)));
            meta.lore(lore);
            it.setItemMeta(meta);
            return 1.0;
        });
        /* --- lore'u ORTASINDAN düzenleme ---
         * Büyü bloğu sona eklenmiyor, statlarla yeteneğin ARASINA giriyor. Bunun
         * için betiğin satırları okuyup araya yazabilmesi gerekiyor. Nereye
         * gireceğine ve neyin silineceğine betik karar veriyor; burası sadece
         * listeyi taşıyor. */
        m.put("girisLoreSayisi", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return 0.0;
            List<Component> lore = it.getItemMeta().lore();
            return lore == null ? 0.0 : lore.size();
        });
        m.put("girisLoreSatir", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return "";
            List<Component> lore = it.getItemMeta().lore();
            int i = RaidenScript.tam(a, 1);
            if (lore == null || i < 0 || i >= lore.size()) return "";
            return duzMetin(lore.get(i));
        });
        m.put("girisLoreYaz", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return 0.0;
            ItemMeta meta = it.getItemMeta();
            List<Component> lore = meta.lore() == null ? new ArrayList<>() : new ArrayList<>(meta.lore());
            int i = Math.max(0, Math.min(lore.size(), RaidenScript.tam(a, 1)));
            lore.add(i, bilesen(RaidenScript.metin(a, 2)));
            meta.lore(lore);
            it.setItemMeta(meta);
            return 1.0;
        });
        m.put("girisLoreSilAralik", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return 0.0;
            ItemMeta meta = it.getItemMeta();
            List<Component> lore = meta.lore();
            if (lore == null) return 0.0;
            int bas = RaidenScript.tam(a, 1);
            int adet = RaidenScript.tam(a, 2);
            if (bas < 0 || adet <= 0 || bas >= lore.size()) return 0.0;
            int son = Math.min(lore.size(), bas + adet);
            List<Component> yeni = new ArrayList<>(lore.subList(0, bas));
            yeni.addAll(lore.subList(son, lore.size()));
            meta.lore(yeni);
            it.setItemMeta(meta);
            return son - bas;
        });

        /* Betiğin kendi defteri: eşya üzerinde tam sayı saklar. Büyü bloğunun
         * lore'da nerede başladığını ve kaç satır tuttuğunu böyle hatırlıyor --
         * görünür bir işaretçi satırı koymaya gerek kalmıyor. */
        m.put("girisVeriOku", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return 0.0;
            Integer v = it.getItemMeta().getPersistentDataContainer()
                    .get(anahtar("rai_" + RaidenScript.metin(a, 1)), PersistentDataType.INTEGER);
            return v == null ? 0.0 : v;
        });
        m.put("girisVeriYaz", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return 0.0;
            ItemMeta meta = it.getItemMeta();
            meta.getPersistentDataContainer().set(anahtar("rai_" + RaidenScript.metin(a, 1)),
                    PersistentDataType.INTEGER, RaidenScript.tam(a, 2));
            it.setItemMeta(meta);
            return 1.0;
        });

        m.put("girisLoreSil", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null || !it.hasItemMeta()) return 0.0;
            ItemMeta meta = it.getItemMeta();
            List<Component> lore = meta.lore();
            if (lore == null) return 0.0;
            String ara = renksiz(RaidenScript.metin(a, 1));
            List<Component> yeni = new ArrayList<>();
            int silinen = 0;
            for (Component c : lore) {
                if (renksiz(duzMetin(c)).contains(ara)) { silinen++; continue; }
                yeni.add(c);
            }
            meta.lore(yeni);
            it.setItemMeta(meta);
            return silinen;
        });

        return m;
    }

    /** Arayüzü tanımak için işaretçi — oyuncunun kendi çantasıyla karışmasın. */
    public static final class MasaSahibi implements org.bukkit.inventory.InventoryHolder {
        private Inventory inv;
        @Override public Inventory getInventory() { return inv; }
    }
}
