# Teknik Destek Görev Zamanlayıcısı

BLM209 Programlama Lab. I proje çalışması. Program C ile geliştirilecek; görevleri
metin dosyasından okuyup dört teknisyen üzerinde farklı zamanlama algoritmalarıyla
simüle edecektir.

## İlk adım: ortak veri modeli

`include/prolab.h`, görevleri, teknisyenleri ve simülasyonun ortak durumunu tanımlar.
`src/model.c` bu modelin ilk değerlerini ve yaşam döngüsü yardımcılarını sağlar.

Model kararları:

- Simülasyon zamanı ve tüm süre sayaçları tam sayı zaman birimi kullanır.
- Görev kayıtları `Simulation` tarafından sahip olunan dinamik bir dizide tutulur;
  başlangıç kapasitesi 1000'dir. Kapasite artırımı girdi okuyucu adımında yapılacaktır.
- Görevler teknisyenleri `0..3` indeksleriyle, atanmamış durumları `-1` ile gösterir.
- Henüz oluşmamış ilk başlama ve bitiş zamanları `-1` olarak tutulur.
- `worked_units` yalnızca gerçek görev çalışmasını sayar. Konum geçişi ayrı teknisyen
  durumu/sayacı olarak tutulur ve görev süresine ya da Round Robin quantum'una eklenmez.
- Teknisyenin son konumu, ilk atamada geçiş gerekip gerekmediğini ayırt etmek için
  `has_last_location` ile birlikte saklanır.
- Round Robin kuyruğu görev ID'si yerine dizi indekslerini tutar; böylece görev kayıtları
  arasında arama yapmadan durum güncellenebilir.
- Round Robin dilim sayacı teknisyende tutulur; görev toplam kalan süresinden ayrıdır.
- Tamamlanmış görev beklemesi `completion_time - arrival_time - total_duration`
  hesabıyla türetilebilir; bu, Round Robin'deki kuyruk beklemelerini de kapsar.

## İkinci adım: görev dosyası okuyucusu

`src/task_file.c`, görev dosyasını satır satır okur. Boşluk ve sekmeyi ayraç kabul eder;
boş ve yorum satırlarını atlar. Hatalı satırları satır numarası ve nedeni ile bildirir,
sonraki satırları işlemeye devam eder. Sayısal alanlar tam sayı sınırlarıyla doğrulanır;
konum, öncelik ve görev türü uzunluğu denetlenir. Yinelenen ID'de ilk geçerli kayıt
korunur. Başlangıçtaki 1000 kayıt kapasitesi dolunca görev dizisi büyütülür.

`task_file_load` dosya açma veya bellek ayırma gibi yapısal hatalarda `false` döndürür.
Başarıyla okunan dosyada `TaskLoadReport`, kabul edilen, reddedilen ve atlanan satırları
ayrı sayar. Dosya açılamazsa mevcut görev dizisi korunur; dosya açıldıktan sonra yeni
yükleme için görev listesi temizlenir.

## Üçüncü adım: simülasyon çekirdeği

`src/simulation.c`, simülasyonu başlatma/sıfırlama, zaman adımı ilerletme ve geçmiş bir
zamana yeniden oynatarak dönme işlevlerini içerir. Shortest Job, Priority, Round Robin ve
özgün algoritma seçimleri uygulanmıştır. Her adımda mevcut işler ilerletilir, tamamlanmalar ve quantum
sonları işlenir, yeni gelişler kuyruğa alınır ve boş teknisyenler T1-T4 sırasıyla doldurulur.
Farklı konuma geçiş, bir iş adımı harcamadan ayrı bir teknisyen durumu olarak yürütülür.
Round Robin kuyruğu dairesel dizi kullanır; aynı andaki yeni işler ID sırasıyla eklenir,
quantum'u dolan işler ise teknisyen sırasıyla bunların arkasına konur.

Özgün strateji her boş teknisyen için bekleyen görevin `toplam süre + gerekiyorsa 1
birim geçiş` puanını hesaplar. Puan eşitse geçiş gerektirmeyen, sonra daha kısa, daha
erken gelmiş ve daha küçük ID'li görev seçilir. Strateji kesintisiz çalışır; Priority
alanı seçimde kullanılmaz.

## Dördüncü adım: konsol menüsü ve raporlama

`src/main.c`, dosya yolu alma ve menü akışını sağlar. Algoritma değiştirme, görevleri
listeleme/filtreleme/sorgulama, teknisyen durumları, zaman komutları, sıfırlama ve
istatistikler menüye bağlanmıştır. Kullanıcı girdileri tam sayı ve sınır açısından
denetlenir. İstatistiklerde ortalamalar sadece tamamlanmış görevlerden, kullanım ise
görevlerin şimdiye kadar çalıştığı birimlerden hesaplanır.

## Kontroller

`tests/test_prolab.c`, görev okuyucusu ve simülasyon için tekrar çalıştırılabilir kontroller
içerir. Windows PowerShell'de GCC ile uygulamayı derleyip testleri çalıştırmak için:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I include src/main.c src/model.c src/task_file.c src/simulation.c -o prolab.exe
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I include tests/test_prolab.c src/model.c src/task_file.c src/simulation.c -o test_prolab.exe
./test_prolab.exe
```

## Konum maliyeti senaryosu

`examples/location_cost_scenario.txt` algoritma karşılaştırması için oluşturulmuş 12
görevlik bir veri setidir. Round Robin quantum'u 2 seçilmiştir. Aynı simülasyon
kurallarıyla ölçülen sonuçlar:

| Algoritma | Makespan | Ortalama bekleme | Ortalama sistemde kalma | Genel kullanım |
|---|---:|---:|---:|---:|
| Shortest Job | 21 | 3.917 | 8.417 | %64.286 |
| Priority | 20 | 4.000 | 8.500 | %67.500 |
| Round Robin (q=2) | 22 | 8.917 | 13.417 | %61.364 |
| Özgün | 19 | 3.333 | 7.833 | %71.053 |

Bu senaryoda özgün yöntem makespan'i Shortest Job'a göre %9.524, Priority'ye göre
%5.000 ve Round Robin'e göre %13.636 düşürmüştür. Bu sonuç yalnızca bu veri seti ve
Round Robin quantum=2 için geçerlidir; yöntemin her iş yükünde daha iyi olacağını
göstermez. Kalıcı test paketi bu dört makespan sonucunu da doğrular.

## Kaynaklar

- `include/prolab.h` - türler, sabitler ve model arayüzü
- `src/model.c` - başlangıç ve bellek temizleme işlevleri
- `src/task_file.c` - görev dosyası ayrıştırma ve doğrulama
- `src/simulation.c` - dört algoritma ve simülasyon zaman akışı
- `src/main.c` - kullanıcı menüsü, ekranlar ve istatistikler
- `tests/test_prolab.c` - yükleme ve simülasyon kontrolleri

Kalan proje işleri: özgün algoritmanın standartlara karşı kendi senaryolarınızda sayısal
karşılaştırması, IEEE raporu ve sunum materyalleri.
