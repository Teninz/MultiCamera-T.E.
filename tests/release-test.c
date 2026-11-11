#include <assert.h>
#include <stdio.h>
#include "../plugin/src/release.h"
int main(void)
{
 assert(olc_newer("v0.2.1","0.2.0"));assert(olc_newer("1.0.0","0.99.99"));assert(olc_newer("0.10.0","0.2.99"));
 assert(!olc_newer("0.2.0","0.2.0"));assert(!olc_newer("0.1.99","0.2.0"));
 assert(!olc_newer("0.3.0-beta","0.2.0"));assert(!olc_newer("99999999999.0.0","0.2.0"));assert(!olc_newer("v0.3","0.2.0"));
 assert(olc_asset_url("https://github.com/Teninz/obs-lan-camera/releases/download/v0.3.0/OBS-LAN-Camera-Setup.exe","v0.3.0"));
 assert(!olc_asset_url("http://github.com/Teninz/obs-lan-camera/releases/download/v0.3.0/OBS-LAN-Camera-Setup.exe","v0.3.0"));
 assert(!olc_asset_url("https://evil.example/OBS-LAN-Camera-Setup.exe","v0.3.0"));
 assert(!olc_asset_url("https://github.com/Other/obs-lan-camera/releases/download/v0.3.0/OBS-LAN-Camera-Setup.exe","v0.3.0"));
 assert(olc_sha256("sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));assert(!olc_sha256("sha256:abc"));assert(!olc_sha256("sha256:gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg"));
 puts("PASS: release version ordering, malformed versions, repository/HTTPS/asset checks, SHA-256 metadata checks");return 0;
}
