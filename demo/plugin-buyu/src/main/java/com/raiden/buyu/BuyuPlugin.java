package com.raiden.buyu;

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
 * RaidenBüyü — büyü masası. Kuralların tamamı buyu.rai içinde.
 *
 * Bu sınıfın işi: VM'i ayağa kaldır, mc.* ilkellerini kaydet, arayüz olaylarını
 * betiğe ilet. Hangi büyü var, ne kadar pahalı, neyle çakışır — hiçbiri burada.
 */
public final class BuyuPlugin extends JavaPlugin implements Listener {

    private RaidenScript vm;
    private BuyuBaglayici mc;
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
        saveResource("buyu.rai", false);
        betikYolu = new File(getDataFolder(), "buyu.rai").getAbsolutePath();
        mc = new BuyuBaglayici(this);

        if (!betigiYukle()) {
            getServer().getPluginManager().disablePlugin(this);
            return;
        }
        getServer().getPluginManager().registerEvents(this, this);
        getLogger().info("Büyü kuralları yüklendi -> " + betikYolu);
    }

    @Override
    public void onDisable() {
        if (vm != null) { vm.close(); vm = null; }
    }

    /*
     * DİKKAT — dosya adı BİLEREK farklı: bir yerel kütüphane JVM içinde yalnızca
     * TEK bir sınıf yükleyicisi tarafından yüklenebilir. RaidenEnderChest
     * "raidenscript.dll" adıyla yüklüyor; aynı yolu bu plugin de yüklemeye
     * kalkarsa UnsatisfiedLinkError ("already loaded in another classloader")
     * alır. Ayrı kopya = ayrı yol = ayrı örnek.
     */
    private void yerelKutuphaneyiYukle() throws IOException {
        File hedef = new File(getDataFolder(), "raidenscript-buyu.dll");
        hedef.getParentFile().mkdirs();
        try (InputStream in = getResource("yerel/raidenscript.dll")) {
            if (in == null) throw new IOException("yerel/raidenscript.dll jar'da yok — 'make jni' çalıştırıldı mı?");
            Files.copy(in, hedef.toPath(), StandardCopyOption.REPLACE_EXISTING);
        } catch (IOException e) {
            if (!hedef.isFile()) throw e;
        }
        RaidenScript.yukle(hedef.getAbsolutePath());
    }

    /** Betiği (yeniden) yükler. Yenisi kurulmadan eskisi kapatılmıyor. */
    private boolean betigiYukle() {
        RaidenScript yeni = null;
        try {
            String kaynak = Files.readString(new File(betikYolu).toPath(), StandardCharsets.UTF_8);
            yeni = RaidenScript.ac();
            yeni.kaydetHepsi("mc", mc.ilkeller());
            yeni.eval(kaynak, "buyu.rai");
            yeni.cagir("kur");
            if (vm != null) vm.close();
            vm = yeni;
            return true;
        } catch (Throwable t) {
            if (yeni != null) yeni.close();
            getLogger().severe("buyu.rai yüklenemedi:\n" + t.getMessage());
            return false;
        }
    }

    private void betigeCagir(String fn) {
        if (vm == null) return;
        try {
            vm.cagir(fn);
        } catch (RaidenScript.RaidenHatasi e) {
            getLogger().severe("buyu.rai hatası (" + fn + "):\n" + e.getMessage());
        }
    }

    @Override
    public boolean onCommand(CommandSender gonderen, Command komut, String etiket, String[] args) {
        if (args.length == 1 && args[0].equalsIgnoreCase("yenile")) {
            if (!gonderen.hasPermission("raiden.buyu.yonet")) {
                gonderen.sendMessage("§cBu komut için yetkin yok.");
                return true;
            }
            gonderen.sendMessage(betigiYukle()
                ? "§abuyu.rai yeniden yüklendi."
                : "§cYükleme başarısız — günlüğe bak. Eski kurallar duruyor.");
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
        mc.baglamKur(oyuncu.getUniqueId(), args);
        betigeCagir("komut");
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
        if (!bizimMasa(e.getClickedInventory(), p)) {
            // Alt çantadan shift-tıkla masaya eşya fırlatmayı engelle.
            if (e.isShiftClick() && mc.masasi(p.getUniqueId()) != null
                && e.getView().getTopInventory().equals(mc.masasi(p.getUniqueId()))) {
                e.setCancelled(true);
            }
            return;
        }
        int yuva = e.getSlot();
        if (yuva == BuyuBaglayici.GIRIS_YUVASI) {
            // Giriş yuvası serbest: oyuncu eşyayı koyup alabilsin. Eşya bir tik
            // sonra yerine oturuyor, o yüzden tazeleme gecikmeli.
            getServer().getScheduler().runTask(this, () -> {
                mc.baglamKur(p.getUniqueId(), new String[0]);
                betigeCagir("girisDegisti");
            });
            return;
        }
        e.setCancelled(true);
        mc.tiklamaKur(p.getUniqueId(), yuva);
        betigeCagir("tiklandi");
    }

    @EventHandler
    public void surukleme(InventoryDragEvent e) {
        if (!(e.getWhoClicked() instanceof Player p)) return;
        Inventory bizim = mc.masasi(p.getUniqueId());
        if (bizim == null || !e.getView().getTopInventory().equals(bizim)) return;
        boolean sadeceGiris = e.getRawSlots().stream()
                .allMatch(s -> s == BuyuBaglayici.GIRIS_YUVASI || s >= bizim.getSize());
        if (!sadeceGiris) { e.setCancelled(true); return; }
        getServer().getScheduler().runTask(this, () -> {
            mc.baglamKur(p.getUniqueId(), new String[0]);
            betigeCagir("girisDegisti");
        });
    }

    @EventHandler
    public void kapanma(InventoryCloseEvent e) {
        if (!(e.getPlayer() instanceof Player p)) return;
        Inventory bizim = mc.masasi(p.getUniqueId());
        if (bizim == null || !e.getInventory().equals(bizim)) return;
        // Giriş eşyası masada kalmasın: oyuncuya geri ver, yer yoksa yere at.
        ItemStack giris = bizim.getItem(BuyuBaglayici.GIRIS_YUVASI);
        if (giris != null && giris.getType() != Material.AIR) {
            p.getInventory().addItem(giris).values()
             .forEach(kalan -> p.getWorld().dropItemNaturally(p.getLocation(), kalan));
        }
        mc.masayiUnut(p.getUniqueId());
    }
}
