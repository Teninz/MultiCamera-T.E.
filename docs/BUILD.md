# Сборка из исходников / Build from source

Команды выполняются из корня репозитория. / Run commands from the repository root.

Исходники: `hub/src` — приёмник Windows и виртуальные камеры; `android/src` — Android; `plugin/src` — OBS; `relay` — свой сервер. Лицензия GPL-2.0-or-later; см. `COPYING` и `THIRD-PARTY-*`.

Нужны Python, Zig 0.14.1, JDK 17, Android SDK Platform 35 / Build Tools 35.0.1 и ZXing Core 3.5.3. Для плагина дополнительно нужны исходники OBS 33.0.1 и заголовки FFmpeg 8; установленные библиотеки OBS не копируются в дистрибутив. Общий приёмник использует системный Windows Media Foundation и работает без установленного OBS. Интерфейсу нужен Microsoft Edge WebView2 Runtime; для сборки — .NET Framework csc и официальный NuGet Microsoft.Web.WebView2 1.0.4314.35. Светлая и тёмная темы переключаются в приложении.

```powershell
python build.py --work build-work --zig C:\tools\zig\zig.exe --obs-source C:\src\obs-studio --ffmpeg C:\src\ffmpeg-8.0 --sdk C:\Android\Sdk --jdk C:\Java\jdk-17 --zxing C:\libs\core-3.5.3.jar
python hub/build.py --work hub-work --zig C:\tools\zig\zig.exe --webview2 C:\deps\webview2 --installer
```

Рабочие каталоги сборки должны находиться вне публикуемых файлов. При первой сборке APK создаётся ключ подписи: храните его и пароль в резервной копии и не публикуйте. Обновления установленного APK должны подписываться тем же ключом.

[Протокол](../PROTOCOL.md), [проверки и их границы](../VERIFICATION.md), [проверки общего приёмника](../hub/DEVELOPMENT.md), [создание фонового изображения](../hub/assets/README.md).
