# VFR-H264: H.264 Codec için Değişken Kare Hızı (Varriable Frame Rate) Entegrasyonu

## 📌 Amaç ve Vizyon
Standart video kameraları genellikle Sabit Kare Hızı (Constant Frame Rate - CFR) ile kayıt yapar. Bu durum, videoda hiçbir hareketin olmadığı (örneğin boş bir oda, slayt sunumu veya güvenlik kamerası kayıtları) sahnelerde bile saniyede onlarca birbirinin aynısı karenin (frame) işlenmesine ve depolanmasına neden olur. 

**VFR-H264** projesinin vizyonu, bu gereksiz veri yığınını ortadan kaldırmaktır. Proje, ham video (YUV) verisini analiz ederek birbirini tekrar eden statik kareleri tespit eder ve kodlama (encoding) sürecinden dışlar. Bu sayede görsel kaliteden hiçbir ödün vermeden **ciddi oranda depolama alanı ve bant genişliği tasarrufu** sağlanır. Senkronizasyon kayıplarını önlemek için ise her karenin sunum zaman damgası (PTS) özel bir timecode dosyasına yazılarak video kapsayıcısına (MKV) otomatik olarak gömülür.



## ⚙️ Teknik Detaylar ve Mimari

Yazılım C dili ile geliştirilmiş olup, H.264 kodlaması için `libx264` kütüphanesini kullanmaktadır. Süreç şu adımlarla işler:

1. **Ham Veri Okuma:** `.yuv` formatındaki ham video dosyası (YUV420p renk uzayında) okunur.
2. **Kare Farkı Hesaplama (Luma Analizi):** Gelen her yeni karenin yalnızca Y (Luminance - Parlaklık) düzlemi piksel piksel bir önceki kare ile karşılaştırılır. Mutlak farkların ortalaması alınarak bir eşik değeri (Threshold) ile kıyaslanır. (Varsayılan Eşik: `5`).
3. **Akıllı Çerçeve Düşürme (Frame Drop):** Eğer iki kare arasındaki fark eşik değerinin altındaysa, sahnede hareket olmadığı varsayılır ve kare H.264 kodlayıcısına gönderilmez (ATILDI).
4. **Zaman Damgası (Timecode) Üretimi:** Alınan her orijinal karenin orijinal sırası (PTS) milisaniye cinsinden hesaplanır ve *v4 timecode* formatında bir metin dosyasına yazılır. Bu, ses ve görüntü senkronizasyonunun kaybolmasını engeller.
5. **Otomatik Çoklama (Multiplexing):** Kodlama bittikten sonra yazılım, işletim sistemi seviyesinde (`fork/execvp`) `mkvmerge` aracını çağırarak elde edilen `.h264` video akışını ve zaman damgası dosyasını tek bir `.mkv` dosyasında birleştirir.

## 🛠️ Sistem Gereksinimleri
Bu projeyi derlemek ve çalıştırmak için sisteminizde aşağıdaki paketlerin kurulu olması gerekmektedir:
* `gcc` (C Derleyicisi)
* `libx264-dev` (H.264 encoding kütüphanesi)
* `ffmpeg` (Ön işleme ve YUV dönüşümleri için)
* `mkvtoolnix` (İçerisindeki `mkvmerge` aracının MKV paketlemesi yapabilmesi için)

## 🚀 Kurulum ve Kullanım

### 1. Kodu Derleme
Proje dizininde terminali açın ve aşağıdaki komutla C kodunu derleyin:
```bash
gcc main.c -o vfr -lx264 -Wall
```

### 2. Ham Video (YUV) Hazırlığı
Program ham YUV420p verisi işlediği için, elinizdeki standart bir videoyu (mp4, mov vb.) öncelikle ffmpeg ile dönüştürmelisiniz.

* **Standart Videolar İçin:**
  ```bash
  ffmpeg -i input.mp4 -c:v rawvideo -s 1920x1080 yuv/input.yuv
  ```
* **iPhone Kamera Kayıtları İçin** (Renk uzayını zorlamak gerekebilir):
  ```bash
  ffmpeg -i input.mov -c:v rawvideo -pix_fmt yuv420p -s 1920x1080 yuv/input.yuv
  ```

### 3. Programı Çalıştırma
Derlediğiniz `vfr` aracını; input dosyası, genişlik, yükseklik ve orijinal FPS değerlerini argüman olarak vererek çalıştırın. Genişlik ve yükseklik değerleri 2'nin (genişlik 32'nin) katı olmalıdır.
```bash
./vfr yuv/input.yuv 1920 1080 30
```

*Not: Program çalıştığında `h264`, `timecode` ve `mkv` klasörlerini otomatik olarak oluşturacak ve çıktıları buralara kaydedecektir.*

### 4. Ses (Audio) Ekleme (İsteğe Bağlı)
Program şu an sadece video akışını işlemektedir. Eğer orijinal videonuzdaki sesi MKV dosyanıza dahil etmek isterseniz, sesi ayırıp MKVToolNix ile sonradan birleştirebilirsiniz:
```bash
# Sesi aac olarak ayırma
ffmpeg -i input.mp4 -vn -c:a aac aac/audio.aac

# Manuel birleştirme (Opsiyonel)
mkvmerge -o mkv/final.mkv mkv/input.mkv aac/audio.aac
```

## 📊 Örnek Çıktı

Konsolda işleme sırasında aşağıdaki gibi bir log ekranı göreceksiniz. Süreç sonunda elde edilen "Kare Tasarruf Oranı", videonun ne kadar durağan olduğuna bağlı olarak dosya boyutundaki kazancınızı temsil eder.

```text
Kare: 1    Fark: 0      PTS: 0 ALINDI
Kare: 2    Fark: 2 ATILDI
Kare: 3    Fark: 1 ATILDI
Kare: 4    Fark: 15     PTS: 3 ALINDI
...
Toplam kare: 1500
Encode olan kare: 450
Kare tasarruf orani: % 70.00
```

***

Bu dokümantasyon kodunun yeteneklerini oldukça profesyonel bir çerçevede özetliyor. Geliştirme sürecini daha da otomatize etmek için projeye bir `Makefile` eklemek istersen veya C kodundaki bellek yönetimi (örneğin YUV okuma kısımlarındaki `malloc`/`free` döngüleri) üzerine çalışmak istersen bana söylemen yeterli. Nasıl buldun bu yapıyı?