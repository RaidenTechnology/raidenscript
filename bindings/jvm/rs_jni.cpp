/* RaidenScript — JVM köprüsü (Faz 6).
 *
 * capi.h'daki C yüzeyini JNI üzerinden Java'ya açar. Buradaki tasarım
 * bindings/js/rs-host.js ile BİLEREK aynı: sayılar args[] dizisinden, dizeler
 * capi.h'daki ayrı kanaldan (rs_arg_str / rs_return_str) geçer. Java tarafında
 * bu görünmez — host fonksiyonu sıradan Object[] alır, Double ya da String
 * döndürür.
 *
 * Derleme (w64devkit + JDK 21):
 *     make jni
 *
 * NOT: Bu dosya src/ altındaki dil çekirdeğinin parçası DEĞİL, bir bağlayıcı.
 * Aynı capi.h'yi kullanır, dilde hiçbir şey değiştirmez.
 */

#include <jni.h>
#include <stdint.h>
#include <string.h>
#include "../../src/capi.h"

static JavaVM*   g_jvm  = 0;
static jclass    g_cls  = 0;   /* com.raiden.rs.RaidenScript */
static jmethodID g_host = 0;   /* static Object hostCall(long, String, String, Object[]) */
static jclass    g_dbl  = 0;
static jmethodID g_dblNew = 0;
static jmethodID g_dblVal = 0;

/* JNIEnv'i geçerli iş parçacığı için alır. Paper'ın ana iş parçacığından
 * çağrılıyoruz ama köprü başka bir iş parçacığından da çağrılabilsin diye
 * bağlanma yolu açık bırakıldı — bağlanamıyorsa sessiz sıfır yerine 0 dönüp
 * hatayı Java tarafına bırakmak yerine burada belli oluyor. */
static JNIEnv* env_al(int* baglandi) {
    JNIEnv* env = 0;
    *baglandi = 0;
    if (!g_jvm) return 0;
    jint rc = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_8);
    if (rc == JNI_EDETACHED) {
        if (g_jvm->AttachCurrentThread((void**)&env, 0) != JNI_OK) return 0;
        *baglandi = 1;
    } else if (rc != JNI_OK) {
        return 0;
    }
    return env;
}

/* Betikten host'a çağrı. 'include mc' + 'mc.mesaj(...)' buraya düşer. */
static double host_kopru(const char* modul, const char* fn,
                         const double* args, int argc, void* user) {
    rs_vm* vm = (rs_vm*)user;
    int baglandi = 0;
    JNIEnv* env = env_al(&baglandi);
    if (!env) return 0;

    jclass objCls = env->FindClass("java/lang/Object");
    jobjectArray dizi = env->NewObjectArray(argc, objCls, 0);

    for (int i = 0; i < argc; i++) {
        /* Dize mi sayı mı olduğunu C tarafı biliyor, biz sormuyoruz. */
        const char* s = rs_arg_str(vm, i);
        jobject deger;
        if (s) {
            deger = env->NewStringUTF(s);
        } else {
            deger = env->NewObject(g_dbl, g_dblNew, args[i]);
        }
        env->SetObjectArrayElement(dizi, i, deger);
        env->DeleteLocalRef(deger);
    }

    jstring jmodul = env->NewStringUTF(modul);
    jstring jfn = env->NewStringUTF(fn);

    jobject sonuc = env->CallStaticObjectMethod(g_cls, g_host, (jlong)(intptr_t)vm,
                                                jmodul, jfn, dizi);

    /* Java istisnası C++ karelerinin arasından GEÇMEMELİ. Web köprüsünde
     * ölçüldü: kaçan istisna yığın işaretçisini bozuyor. Burada da aynı kural —
     * istisna temizlenir, betiğe sıfır döner, hata Java tarafında loglanır. */
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(jmodul);
        env->DeleteLocalRef(jfn);
        env->DeleteLocalRef(dizi);
        if (baglandi) g_jvm->DetachCurrentThread();
        return 0;
    }

    double cikti = 0;
    if (sonuc) {
        if (env->IsInstanceOf(sonuc, g_dbl)) {
            cikti = env->CallDoubleMethod(sonuc, g_dblVal);
        } else {
            jclass strCls = env->FindClass("java/lang/String");
            if (env->IsInstanceOf(sonuc, strCls)) {
                const char* c = env->GetStringUTFChars((jstring)sonuc, 0);
                rs_return_str(vm, c);   /* metin kopyalanır, sahiplik bizde kalır */
                env->ReleaseStringUTFChars((jstring)sonuc, c);
            }
            env->DeleteLocalRef(strCls);
        }
        env->DeleteLocalRef(sonuc);
    }

    env->DeleteLocalRef(jmodul);
    env->DeleteLocalRef(jfn);
    env->DeleteLocalRef(dizi);
    if (baglandi) g_jvm->DetachCurrentThread();
    return cikti;
}

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* jvm, void* /*ayrilmis*/) {
    g_jvm = jvm;
    JNIEnv* env = 0;
    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) return JNI_ERR;

    jclass yerel = env->FindClass("com/raiden/rs/RaidenScript");
    if (!yerel) return JNI_ERR;
    g_cls = (jclass)env->NewGlobalRef(yerel);
    g_host = env->GetStaticMethodID(g_cls, "hostCall",
        "(JLjava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
    if (!g_host) return JNI_ERR;

    jclass d = env->FindClass("java/lang/Double");
    g_dbl = (jclass)env->NewGlobalRef(d);
    g_dblNew = env->GetMethodID(g_dbl, "<init>", "(D)V");
    g_dblVal = env->GetMethodID(g_dbl, "doubleValue", "()D");
    if (!g_dblNew || !g_dblVal) return JNI_ERR;

    return JNI_VERSION_1_8;
}

JNIEXPORT jlong JNICALL
Java_com_raiden_rs_RaidenScript_nOpen(JNIEnv* /*env*/, jclass /*cls*/) {
    rs_vm* vm = rs_new();
    if (!vm) return 0;
    rs_set_host(vm, host_kopru, vm);
    /* JVM'in varsayılan iş parçacığı yığını 1 MB ve betik karesi başına ~1,7 KB
     * yerel yığın gidiyor: ~500 karede taşıyor ve taşma Java istisnası DEĞİL,
     * sunucunun sessiz ölümü (hs_err bile yok). 400, o duvarın güvenli tarafı.
     * -Xss ile büyük yığın veren sunucular bunu yükseltebilir. */
    rs_set_max_depth(vm, 400);
    return (jlong)(intptr_t)vm;
}

JNIEXPORT jint JNICALL
Java_com_raiden_rs_RaidenScript_nRegister(JNIEnv* env, jclass, jlong p,
                                          jstring modul, jstring fn) {
    const char* m = env->GetStringUTFChars(modul, 0);
    const char* f = env->GetStringUTFChars(fn, 0);
    jint rc = rs_register((rs_vm*)(intptr_t)p, m, f);
    env->ReleaseStringUTFChars(modul, m);
    env->ReleaseStringUTFChars(fn, f);
    return rc;
}

JNIEXPORT jint JNICALL
Java_com_raiden_rs_RaidenScript_nEval(JNIEnv* env, jclass, jlong p,
                                      jstring kaynak, jstring ad) {
    const char* k = env->GetStringUTFChars(kaynak, 0);
    const char* a = env->GetStringUTFChars(ad, 0);
    jint rc = rs_eval((rs_vm*)(intptr_t)p, k, a);
    env->ReleaseStringUTFChars(kaynak, k);
    env->ReleaseStringUTFChars(ad, a);
    return rc;
}

/* Sonuç 'cikti[0]'a yazılır, dönüş değeri SADECE hata kodudur. Tek bir double
 * hem sonucu hem hatayı taşısaydı, "-1 gerçekten sonuç muydu yoksa hata mıydı"
 * sorusu doğardı ve ilk yanlış okumada sessizce yanlış davranış üretirdi. */
JNIEXPORT jint JNICALL
Java_com_raiden_rs_RaidenScript_nCall(JNIEnv* env, jclass, jlong p,
                                      jstring fn, jdoubleArray args,
                                      jdoubleArray cikti) {
    const char* f = env->GetStringUTFChars(fn, 0);
    jsize n = args ? env->GetArrayLength(args) : 0;
    double* buf = 0;
    if (n > 0) {
        buf = new double[n];
        env->GetDoubleArrayRegion(args, 0, n, buf);
    }
    double out = 0;
    int rc = rs_call((rs_vm*)(intptr_t)p, f, buf, (int)n, &out);
    delete[] buf;
    env->ReleaseStringUTFChars(fn, f);
    if (rc == 0 && cikti && env->GetArrayLength(cikti) > 0) {
        env->SetDoubleArrayRegion(cikti, 0, 1, &out);
    }
    return rc;
}

JNIEXPORT jstring JNICALL
Java_com_raiden_rs_RaidenScript_nLastError(JNIEnv* env, jclass, jlong p) {
    const char* e = rs_last_error((rs_vm*)(intptr_t)p);
    return env->NewStringUTF(e ? e : "");
}

JNIEXPORT void JNICALL
Java_com_raiden_rs_RaidenScript_nClose(JNIEnv*, jclass, jlong p) {
    rs_free((rs_vm*)(intptr_t)p);
}

}  /* extern "C" */
