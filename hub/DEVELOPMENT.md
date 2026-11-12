# MultiCamera T.E. 0.4.1

Native core: Media Foundation H.264 decoder, five encrypted receiver slots, current-user named pipes, five session-lifetime Windows virtual cameras. COM cameras are created, kept alive and destroyed in the same MTA thread. OBS and virtual camera clients consume independent streams.

Desktop.cs is a .NET Framework WinForms/WebView2 host. Local UI has CSP, no external scripts, denied permissions/downloads/navigation, origin-checked messages. No additional web listener. Pairing secrets are not logged or stored by the UI; native settings use DPAPI. Only generated thumbnails cross the UI boundary, capped at 10fps.

Build with Zig0.14.1 and the official extracted Microsoft.Web.WebView21.0.4314.35 NuGet package: `python hub/build.py --zig C:/tools/zig/zig.exe --work hub-work --webview2 C:/deps/webview2 --installer`. Requires Windows .NET Framework csc compiler and the WebView2 Runtime on the user's PC.

UI sources in ui/, five-language translations combined during build. Generated bitmap provenance/prompts in ui/assets/README.md. Dark/light preference persists in local WebView storage; language, animation and server settings use DPAPI.

Validation and explicit remaining limits are recorded in ../VERIFICATION.md. Core tests in tests/run-core.py; real libobs probe tests/obs-hub-test.c; FrameServer capture tests/windows-camera-test.c. Positive relay tests do not prove public VPS deployment.
