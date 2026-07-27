package com.raiden.ec;

import com.raiden.rs.RaidenScript;
import org.bukkit.command.Command;
import org.bukkit.command.CommandSender;
import org.bukkit.entity.Player;
import org.bukkit.plugin.java.JavaPlugin;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/**
 * RaidenEnderChest — kuralları RaidenScript'te olan bir Paper plugin'i.
 *
 * Bu sınıfın işi: yerel kütüphaneyi kur, VM'i aç, mc.* ilkellerini kaydet,
 * enderchest.rai'yi yükle, komutu betiğe ilet. Bekleme süresi, izin adları,
 * mesaj metinleri, ses ve parçacık seçimi — hiçbiri burada değil.
 *
 * '/ec yenile' betiği yeniden yüklüyor: sunucu kapatmadan kural değiştirmek
 * dilin buradaki asıl faydası.
 */
public final class EnderChestPlugin extends JavaPlugin {

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
        saveResource("enderchest.rai", false);
        betikYolu = new File(getDataFolder(), "enderchest.rai").getAbsolutePath();
        mc = new McBaglayici(this);

        if (!betigiYukle()) {
            getServer().getPluginManager().disablePlugin(this);
            return;
        }
        getLogger().info("RaidenScript kuralları yüklendi -> " + betikYolu);
    }

    @Override
    public void onDisable() {
        if (vm != null) { vm.close(); vm = null; }
    }

    /** Jar'ın içindeki DLL'i veri klasörüne çıkarır ve yükler. */
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
     * sonra kapatılıyor — bozuk bir düzenleme sunucuyu komutsuz bırakmasın.
     */
    private boolean betigiYukle() {
        RaidenScript yeni = null;
        try {
            String kaynak = Files.readString(new File(betikYolu).toPath(), StandardCharsets.UTF_8);
            yeni = RaidenScript.ac();
            // SIRA ÖNEMLİ: kayıtlar eval'den önce (capi.h).
            yeni.kaydetHepsi("mc", mc.ilkeller());
            yeni.eval(kaynak, "enderchest.rai");
            yeni.cagir("kur");
            if (vm != null) vm.close();
            vm = yeni;
            return true;
        } catch (Throwable t) {
            if (yeni != null) yeni.close();
            // Tanı metnini olduğu gibi bas: RaidenScript satır/sütun ve ok işareti
            // veriyor, kırpmak o bilgiyi çöpe atmak olur.
            getLogger().severe("enderchest.rai yüklenemedi:\n" + t.getMessage());
            return false;
        }
    }

    @Override
    public boolean onCommand(CommandSender gonderen, Command komut, String etiket, String[] args) {
        if (args.length == 1 && args[0].equalsIgnoreCase("yenile")) {
            if (!gonderen.hasPermission("raiden.ec.yonet")) {
                gonderen.sendMessage("§cBu komut için yetkin yok.");
                return true;
            }
            gonderen.sendMessage(betigiYukle()
                ? "§aenderchest.rai yeniden yüklendi."
                : "§cYükleme başarısız — sunucu günlüğüne bak. Eski kurallar duruyor.");
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
        // o yüzden bağlam kurulup betiğin ÇEKMESİ sağlanıyor — capi.h'daki desen.
        mc.baglamKur(oyuncu.getUniqueId(), args);
        try {
            vm.cagir("komut");
        } catch (RaidenScript.RaidenHatasi e) {
            oyuncu.sendMessage("§cKural hatası — sunucu günlüğüne bak.");
            getLogger().severe("enderchest.rai komut hatası:\n" + e.getMessage());
        }
        return true;
    }
}
