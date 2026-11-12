#include "usb.h"
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#include <string.h>
bool mc_usb_description(const wchar_t *description){
 if(!description)return false;wchar_t lower[512];wcsncpy(lower,description,511);lower[511]=0;CharLowerBuffW(lower,(DWORD)wcslen(lower));
 return wcsstr(lower,L"rndis")||wcsstr(lower,L"remote ndis")||wcsstr(lower,L"usb")||wcsstr(lower,L"ncm")||wcsstr(lower,L"tether");
}
int mc_usb_adapters(struct mc_usb_adapter *adapters,int capacity){
 ULONG size=16384;IP_ADAPTER_ADDRESSES *buffer=malloc(size);if(!buffer)return 0;
 ULONG result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST,NULL,buffer,&size);
 if(result==ERROR_BUFFER_OVERFLOW){free(buffer);buffer=malloc(size);if(!buffer)return 0;result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST,NULL,buffer,&size);}
 int count=0;if(result==NO_ERROR)for(IP_ADAPTER_ADDRESSES *adapter=buffer;adapter&&count<capacity;adapter=adapter->Next){
  if(adapter->OperStatus!=IfOperStatusUp||adapter->IfType!=IF_TYPE_ETHERNET_CSMACD||!mc_usb_description(adapter->Description))continue;
  for(IP_ADAPTER_UNICAST_ADDRESS *entry=adapter->FirstUnicastAddress;entry;entry=entry->Next){
   if(entry->Address.lpSockaddr->sa_family!=AF_INET||entry->DadState!=IpDadStatePreferred)continue;
   struct in_addr address=((struct sockaddr_in*)entry->Address.lpSockaddr)->sin_addr;uint32_t host=ntohl(address.s_addr);unsigned prefix=entry->OnLinkPrefixLength;
   if(!((host>>24)==10||(host>>16)==0xc0a8||(host>>20)==0xac1)||prefix<8||prefix>30)continue;
   struct mc_usb_adapter *item=&adapters[count++];memset(item,0,sizeof(*item));item->index=adapter->IfIndex;InetNtopA(AF_INET,&address,item->address,sizeof(item->address));wcsncpy(item->name,adapter->FriendlyName?adapter->FriendlyName:L"USB",255);
   struct in_addr network={.s_addr=htonl(host&(0xffffffffu<<(32-prefix)))};wchar_t subnet[64];InetNtopW(AF_INET,&network,subnet,64);swprintf(item->subnet,80,L"%ls/%u",subnet,prefix);break;
  }
 }
 free(buffer);return count;
}
