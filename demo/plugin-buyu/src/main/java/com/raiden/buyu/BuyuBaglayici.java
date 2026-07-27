package com.raiden.buyu;

import com.raiden.rs.RaidenScript;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer;
import org.bukkit.Bukkit;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
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
 * mc.* bağlayıcısı — büyü masasının Minecraft yüzü.
 *
 * Burada BÜYÜ YOK. Hangi büyü var, ne yapar, kaç seviye çıkar, kaça mal olur,
 * hangisi hangisiyle çakışır, hangi nadirlik kaç yuva verir — hiçbiri burada.
 * Hepsi buyu.rai içinde. Buradaki her fonksiyon bir ilkel: "sandık arayüzü aç",
 * "şu yuvaya şu eşyayı koy", "giriş eşyasına şu statı ekle".
 *
 * ENTEGRASYON: statlar raiden-combatstats'ın kullandığı ŞEMAYA yazılıyor —
 * namespace "raidenrpg", anahtar "rpg_stat_<stat>" (DOUBLE). Böylece mevcut
 * plugin'lerin tek satırı değişmeden büyüler savaş formülüne giriyor.
 */
public final class BuyuBaglayici {

    /** raiden-combatstats/RpgKeys ile AYNI olmak zorunda. */
    private static final String RPG_NS = "raidenrpg";
    private static final String BUYU_ONEK = "rai_buyu_";

    /** raiden-combatstats/Rarity'deki görünen adlar, en düşükten en yükseğe. */
    private static final String[] NADIRLIKLER =
            { "Sıradan", "Seyrek", "Nadir", "Epik", "Efsanevi", "Mistik", "Eşsiz" };

    private final Plugin plugin;
    private final Map<UUID, Inventory> acikMasalar = new HashMap<>();

    /** Giriş eşyasının durduğu yuva. Betik de bu sayıyı biliyor (mc.girisYuvasi). */
    public static final int GIRIS_YUVASI = 13;

    private UUID komutOyuncu;
    private String[] komutArgs = new String[0];
    private int sonTiklananYuva = -1;

    public BuyuBaglayici(Plugin plugin) { this.plugin = plugin; }

    public void baglamKur(UUID oyuncu, String[] args) {
        this.komutOyuncu = oyuncu;
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

    private static NamespacedKey anahtar(String ad) {
        @SuppressWarnings("deprecation")
        NamespacedKey k = new NamespacedKey(RPG_NS, ad);
        return k;
    }

    private static String duzMetin(Component c) {
        return LegacyComponentSerializer.legacySection().serialize(c);
    }

    /** § kodlarını temizler — nadirlik satırını eşleştirmek için. */
    private static String renksiz(String s) {
        return s.replaceAll("§[0-9a-fk-orA-FK-OR]", "");
    }

    public Map<String, RaidenScript.HostFn> ilkeller() {
        Map<String, RaidenScript.HostFn> m = new HashMap<>();

        // ---------------- komut bağlamı (betik çeker) ----------------
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
        m.put("mesaj", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p != null) p.sendMessage(LegacyComponentSerializer.legacySection()
                    .deserialize(RaidenScript.metin(a, 1)));
            return 0.0;
        });
        m.put("izinVar", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            return (p != null && p.hasPermission(RaidenScript.metin(a, 1))) ? 1.0 : 0.0;
        });
        m.put("ses", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p == null) return 0.0;
            p.playSound(p.getLocation(), RaidenScript.metin(a, 1),
                        (float) RaidenScript.sayi(a, 2), (float) RaidenScript.sayi(a, 3));
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
        m.put("log", a -> { plugin.getLogger().info(RaidenScript.metin(a, 0)); return 0.0; });

        // ---------------- arayüz ----------------
        m.put("guiAc", a -> {
            Player p = oyuncu(RaidenScript.metin(a, 0));
            if (p == null) return 0.0;
            int satir = Math.max(1, Math.min(6, RaidenScript.tam(a, 2)));
            Inventory inv = Bukkit.createInventory(new MasaSahibi(), satir * 9,
                    LegacyComponentSerializer.legacySection().deserialize(RaidenScript.metin(a, 1)));
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
                // Betikteki yazım hatası sessizce boş yuvaya dönüşmesin.
                plugin.getLogger().warning("mc.guiEsya: bilinmeyen materyal -> " + RaidenScript.metin(a, 2));
                return 0.0;
            }
            ItemStack it = new ItemStack(mat, Math.max(1, Math.min(64, RaidenScript.tam(a, 4))));
            ItemMeta meta = it.getItemMeta();
            String ad = RaidenScript.metin(a, 3);
            if (!ad.isEmpty()) {
                meta.displayName(LegacyComponentSerializer.legacySection().deserialize(ad)
                        .decoration(net.kyori.adventure.text.format.TextDecoration.ITALIC, false));
            }
            it.setItemMeta(meta);
            inv.setItem(yuva, it);
            return 1.0;
        });
        m.put("guiLore", a -> {
            Inventory inv = masa(RaidenScript.metin(a, 0));
            if (inv == null) return 0.0;
            int yuva = RaidenScript.tam(a, 1);
            ItemStack it = inv.getItem(yuva);
            if (it == null) return 0.0;
            ItemMeta meta = it.getItemMeta();
            List<Component> lore = meta.lore() == null ? new ArrayList<>() : new ArrayList<>(meta.lore());
            lore.add(LegacyComponentSerializer.legacySection().deserialize(RaidenScript.metin(a, 2))
                    .decoration(net.kyori.adventure.text.format.TextDecoration.ITALIC, false));
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
            return (meta != null && meta.displayName() != null)
                    ? duzMetin(meta.displayName())
                    : it.getType().name();
        });
        m.put("girisMateryal", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            return it == null ? "" : it.getType().name();
        });
        // "silah" | "zirh" | "diger" -- tür kararı burada değil, sadece ham sınıflama.
        m.put("girisTur", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return "";
            String t = it.getType().name();
            if (t.endsWith("_SWORD") || t.endsWith("_AXE") || t.equals("TRIDENT")
                || t.equals("BOW") || t.equals("CROSSBOW") || t.endsWith("_HOE")) return "silah";
            if (t.endsWith("_HELMET") || t.endsWith("_CHESTPLATE")
                || t.endsWith("_LEGGINGS") || t.endsWith("_BOOTS")) return "zirh";
            return "diger";
        });
        /* Nadirlik lore'un son satırında duruyor (raiden-combatstats/Rarity: "display
         * name is appended to the bottom of every item's lore"). Renk kodları
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
                    if (satir.contains(n.toUpperCase(new Locale("tr"))) || satir.contains(n)) return n;
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
            long n = pdc.getKeys().stream()
                    .filter(k -> k.getNamespace().equals(RPG_NS) && k.getKey().startsWith(BUYU_ONEK))
                    .count();
            return (double) n;
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
        /* Stat DELTA'sı ekleniyor, mutlak değer değil: silahın kendi temel statını
         * ezmemek için. raiden-combatstats aynı anahtarı okuyor. */
        m.put("girisStatEkle", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return 0.0;
            String stat = RaidenScript.metin(a, 1).toLowerCase(Locale.ROOT);
            double delta = RaidenScript.sayi(a, 2);
            ItemMeta meta = it.getItemMeta();
            NamespacedKey k = anahtar("rpg_stat_" + stat);
            Double eski = meta.getPersistentDataContainer().get(k, PersistentDataType.DOUBLE);
            meta.getPersistentDataContainer().set(k, PersistentDataType.DOUBLE,
                    (eski == null ? 0.0 : eski) + delta);
            it.setItemMeta(meta);
            return 1.0;
        });
        m.put("girisLoreEkle", a -> {
            ItemStack it = giris(RaidenScript.metin(a, 0));
            if (it == null) return 0.0;
            ItemMeta meta = it.getItemMeta();
            List<Component> lore = meta.lore() == null ? new ArrayList<>() : new ArrayList<>(meta.lore());
            lore.add(LegacyComponentSerializer.legacySection().deserialize(RaidenScript.metin(a, 1))
                    .decoration(net.kyori.adventure.text.format.TextDecoration.ITALIC, false));
            meta.lore(lore);
            it.setItemMeta(meta);
            return 1.0;
        });
        /* Belirli bir metni içeren lore satırını siler — büyü satırını
         * yükseltirken eskisini temizlemek için. */
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
        void setInventory(Inventory i) { this.inv = i; }
    }
}
