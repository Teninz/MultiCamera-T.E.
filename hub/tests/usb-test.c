#include "../src/usb.h"
#include <stdio.h>
int main(void){
 if(!mc_usb_description(L"Remote NDIS based Internet Sharing Device")||!mc_usb_description(L"USB NCM Host Device")||!mc_usb_description(L"SAMSUNG Mobile USB Remote NDIS Network Device")||mc_usb_description(L"Hyper-V Virtual Ethernet Adapter")||mc_usb_description(L"Realtek PCIe GbE Family Controller"))return 2;
 struct mc_usb_adapter adapters[32];int count=mc_usb_adapters(adapters,32);printf("USB_CLASSIFICATION_PASS; connected adapters=%d\n",count);
 for(int i=0;i<count;i++){if(!*adapters[i].address||!*adapters[i].subnet||!*adapters[i].name)return 3;printf("USB_ADAPTER %lu %s\n",adapters[i].index,adapters[i].address);}return 0;
}
