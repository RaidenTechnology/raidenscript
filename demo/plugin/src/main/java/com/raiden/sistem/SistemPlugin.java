package com.raiden.sistem;

import com.raiden.rs.RaidenScript;
import org.bukkit.Material;
import org.bukkit.command.Command;
import org.bukkit.command.CommandSender;
import org.bukkit.entity.Player;
import org.bukkit.event.EventHandler;
import org.bukkit.event.Listener;
import org.bukkit.event.inventory.InventoryClickEvent;
import org.bukkit.event.inventory.InventoryCloseEvent;
import org.bukkit.event.inventory.InventoryDragEvent;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.ItemStack;
import org.bukkit.plugin.java.JavaPlugin;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/**
 * RaidenSistem — sunucu sistemlerinin RaidenScript ile yazıldığı tek paket.
 *
 * TEK dosya (sistem.rai), TEK VM, TEK yerel kütüphane. Yeni bir sistem eklemek
 * yeni bir jar değil, aynı .rai dosyasına yeni bir bölüm demek.
 *
 * Bu sınıfın işi: VM'i ayağa kaldır, mc.* ilkellerini kaydet, komutları ve
 * arayüz olaylarını betiğe ilet. Tek bir oyun kuralı burada değil.
 */
public final class SistemPlugin extends JavaPlugin implements Listener {

    private RaidenScript vm;
    private McBaglayici mc;
    private String betikYolu;

    @Override
    public void onEnable() {
        try {
            yerelKutuphaneyiYukle();
        } catch (Throwable t) {
            getLogger().severe("RaidenScript yerel kütüphanesi yüklenemedi: " + t);
            getServer().getPluginManager().disablePlugin(this);
            return;
        }
        // Betik veri klasörüne kopyalanıyor ki kullanıcı düzenleyebilsin.
        saveResource("sistem.rai", false);
        betikYolu = new File(getDataFolder(), "sistem.rai").getAbsolutePath();
        mc = new McBaglayici(this);

        if (!betigiYukle()) {
            getServer().getPluginManager().disablePlugin(this);
            return;
        }
        getServer().getPluginManager().registerEvents(this, this);
        getLogger().info("Kurallar yüklendi -> " + betikYolu);
    }

    @Override
    public void onDisable() {
        if (vm != null) { vm.close(); vm = null; }
    }

    private void yerelKutuphaneyiYukle() throws IOException {
        File hedef = new File(getDataFolder(), "raidenscript.dll");
        hedef.getParentFile().mkdirs();
        try (InputStream in = getResource("yerel/raidenscript.dll")) {
            if (in == null) throw new IOException("yerel/raidenscript.dll jar'da yok — 'make jni' çalıştırıldı mı?");
            Files.copy(in, hedef.toPath(), StandardCopyOption.REPLACE_EXISTING);
        } catch (IOException e) {
            // Zaten yüklüyse dosya kilitli olur; var olanı kullanmayı dene.
            if (!hedef.isFile()) throw e;
        }
        RaidenScript.yukle(hedef.getAbsolutePath());
    }

    /**
     * Betiği (yeniden) yükler. capi.h: bir VM = bir betik, o yüzden yeniden
     * yükleme yeni bir VM açmak demek. Eskisi ancak yenisi sorunsuz kurulduktan
     * SONRA kapatılıyor — bozuk bir düzenleme sunucuyu komutsuz bırakmasın.
     */
    private boolean betigiYukle() {
        RaidenScript yeni = null;
        try {
            String kaynak = Files.readString(new File(betikYolu).toPath(), StandardCharsets.UTF_8);
            yeni = RaidenScript.ac();
            // SIRA ÖNEMLİ: kayıtlar eval'den önce (capi.h).
            yeni.kaydetHepsi("mc", mc.ilkeller());
            yeni.eval(kaynak, "sistem.rai");
            yeni.cagir("kur");
            if (vm != null) vm.close();
            vm = yeni;
            return true;
        } catch (Throwable t) {
            if (yeni != null) yeni.close();
            // Tanı metnini olduğu gibi bas: RaidenScript satır/sütun ve ok işareti
            // veriyor, kırpmak o bilgiyi çöpe atmak olur.
            getLogger().severe("sistem.rai yüklenemedi:\n" + t.getMessage());
            return false;
        }
    }

    private void betigeCagir(String fn, CommandSender bildirilecek) {
        if (vm == null) return;
        try {
            vm.cagir(fn);
        } catch (RaidenScript.RaidenHatasi e) {
            if (bildirilecek != null) bildirilecek.sendMessage("§cKural hatası — sunucu günlüğüne bak.");
            getLogger().severe("sistem.rai hatası (" + fn + "):\n" + e.getMessage());
        }
    }

    @Override
    public boolean onCommand(CommandSender gonderen, Command komut, String etiket, String[] args) {
        String ad = komut.getName().toLowerCase();

        if (ad.equals("rai")) {
            if (args.length == 1 && args[0].equalsIgnoreCase("yenile")) {
                if (!gonderen.hasPermission("raiden.rai.yonet")) {
                    gonderen.sendMessage("§cBu komut için yetkin yok.");
                    return true;
                }
                gonderen.sendMessage(betigiYukle()
                    ? "§asistem.rai yeniden yüklendi."
                    : "§cYükleme başarısız — günlüğe bak. Eski kurallar duruyor.");
                return true;
            }
            gonderen.sendMessage("§7Kullanım: §f/rai yenile");
            return true;
        }

        if (!(gonderen instanceof Player oyuncu)) {
            gonderen.sendMessage("Bu komut oyuncu içindir.");
            return true;
        }
        if (vm == null) {
            oyuncu.sendMessage("§cKurallar yüklü değil.");
            return true;
        }
        // Dize argümanı betiğe geçirilemiyor (rs_call sadece double alıyor),
        // o yüzden bağlam kurulup betiğin ÇEKMESİ sağlanıyor — capi.h deseni.
        mc.baglamKur(oyuncu.getUniqueId(), ad, args);
        betigeCagir("komut", oyuncu);
        return true;
    }

    // ---------------- arayüz olayları ----------------

    private boolean bizimMasa(Inventory inv, Player p) {
        Inventory bizim = mc.masasi(p.getUniqueId());
        return bizim != null && inv != null && inv.equals(bizim);
    }

    @EventHandler
    public void tiklama(InventoryClickEvent e) {
        if (!(e.getWhoClicked() instanceof Player p)) return;
        Inventory bizim = mc.masasi(p.getUniqueId());
        if (bizim == null) return;

        if (!bizimMasa(e.getClickedInventory(), p)) {
            // Alt çantadan shift-tıkla masaya eşya fırlatmayı engelle.
            if (e.isShiftClick() && e.getView().getTopInventory().equals(bizim)) e.setCancelled(true);
            return;
        }
        int yuva = e.getSlot();
        if (yuva == McBaglayici.GIRIS_YUVASI) {
            // Giriş yuvası serbest. Eşya bir tik sonra yerine oturuyor,
            // o yüzden tazeleme gecikmeli.
            getServer().getScheduler().runTask(this, () -> {
                mc.baglamKur(p.getUniqueId(), "buyu", new String[0]);
                betigeCagir("girisDegisti", null);
            });
            return;
        }
        e.setCancelled(true);
        mc.tiklamaKur(p.getUniqueId(), yuva);
        betigeCagir("tiklandi", p);
    }

    @EventHandler
    public void surukleme(InventoryDragEvent e) {
        if (!(e.getWhoClicked() instanceof Player p)) return;
        Inventory bizim = mc.masasi(p.getUniqueId());
        if (bizim == null || !e.getView().getTopInventory().equals(bizim)) return;
        boolean sadeceGiris = e.getRawSlots().stream()
                .allMatch(s -> s == McBaglayici.GIRIS_YUVASI || s >= bizim.getSize());
        if (!sadeceGiris) { e.setCancelled(true); return; }
        getServer().getScheduler().runTask(this, () -> {
            mc.baglamKur(p.getUniqueId(), "buyu", new String[0]);
            betigeCagir("girisDegisti", null);
        });
    }

    @EventHandler
    public void kapanma(InventoryCloseEvent e) {
        if (!(e.getPlayer() instanceof Player p)) return;
        Inventory bizim = mc.masasi(p.getUniqueId());
        if (bizim == null || !e.getInventory().equals(bizim)) return;
        // Giriş eşyası masada kalmasın: oyuncuya geri ver, yer yoksa yere at.
        ItemStack giris = bizim.getItem(McBaglayici.GIRIS_YUVASI);
        if (giris != null && giris.getType() != Material.AIR) {
            p.getInventory().addItem(giris).values()
             .forEach(kalan -> p.getWorld().dropItemNaturally(p.getLocation(), kalan));
        }
        mc.masayiUnut(p.getUniqueId());
    }
}
