/* Mechanical forwarding to the platform IMFAttributes store. */
static HRESULT WINAPI activate_GetItem(IMFActivate *iface, REFGUID guidKey, PROPVARIANT *pValue){return IMFAttributes_GetItem(((struct activation*)iface)->attributes, guidKey, pValue);}
static HRESULT WINAPI activate_GetItemType(IMFActivate *iface, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType){return IMFAttributes_GetItemType(((struct activation*)iface)->attributes, guidKey, pType);}
static HRESULT WINAPI activate_CompareItem(IMFActivate *iface, REFGUID guidKey, REFPROPVARIANT Value, WINBOOL *pbResult){return IMFAttributes_CompareItem(((struct activation*)iface)->attributes, guidKey, Value, pbResult);}
static HRESULT WINAPI activate_Compare(IMFActivate *iface, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, WINBOOL *pbResult){return IMFAttributes_Compare(((struct activation*)iface)->attributes, pTheirs, MatchType, pbResult);}
static HRESULT WINAPI activate_GetUINT32(IMFActivate *iface, REFGUID guidKey, UINT32 *punValue){return IMFAttributes_GetUINT32(((struct activation*)iface)->attributes, guidKey, punValue);}
static HRESULT WINAPI activate_GetUINT64(IMFActivate *iface, REFGUID guidKey, UINT64 *punValue){return IMFAttributes_GetUINT64(((struct activation*)iface)->attributes, guidKey, punValue);}
static HRESULT WINAPI activate_GetDouble(IMFActivate *iface, REFGUID guidKey, double *pfValue){return IMFAttributes_GetDouble(((struct activation*)iface)->attributes, guidKey, pfValue);}
static HRESULT WINAPI activate_GetGUID(IMFActivate *iface, REFGUID guidKey, GUID *pguidValue){return IMFAttributes_GetGUID(((struct activation*)iface)->attributes, guidKey, pguidValue);}
static HRESULT WINAPI activate_GetStringLength(IMFActivate *iface, REFGUID guidKey, UINT32 *pcchLength){return IMFAttributes_GetStringLength(((struct activation*)iface)->attributes, guidKey, pcchLength);}
static HRESULT WINAPI activate_GetString(IMFActivate *iface, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength){return IMFAttributes_GetString(((struct activation*)iface)->attributes, guidKey, pwszValue, cchBufSize, pcchLength);}
static HRESULT WINAPI activate_GetAllocatedString(IMFActivate *iface, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength){return IMFAttributes_GetAllocatedString(((struct activation*)iface)->attributes, guidKey, ppwszValue, pcchLength);}
static HRESULT WINAPI activate_GetBlobSize(IMFActivate *iface, REFGUID guidKey, UINT32 *pcbBlobSize){return IMFAttributes_GetBlobSize(((struct activation*)iface)->attributes, guidKey, pcbBlobSize);}
static HRESULT WINAPI activate_GetBlob(IMFActivate *iface, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize){return IMFAttributes_GetBlob(((struct activation*)iface)->attributes, guidKey, pBuf, cbBufSize, pcbBlobSize);}
static HRESULT WINAPI activate_GetAllocatedBlob(IMFActivate *iface, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize){return IMFAttributes_GetAllocatedBlob(((struct activation*)iface)->attributes, guidKey, ppBuf, pcbSize);}
static HRESULT WINAPI activate_GetUnknown(IMFActivate *iface, REFGUID guidKey, REFIID riid, LPVOID *ppv){return IMFAttributes_GetUnknown(((struct activation*)iface)->attributes, guidKey, riid, ppv);}
static HRESULT WINAPI activate_SetItem(IMFActivate *iface, REFGUID guidKey, REFPROPVARIANT Value){return IMFAttributes_SetItem(((struct activation*)iface)->attributes, guidKey, Value);}
static HRESULT WINAPI activate_DeleteItem(IMFActivate *iface, REFGUID guidKey){return IMFAttributes_DeleteItem(((struct activation*)iface)->attributes, guidKey);}
static HRESULT WINAPI activate_DeleteAllItems(IMFActivate *iface){return IMFAttributes_DeleteAllItems(((struct activation*)iface)->attributes);}
static HRESULT WINAPI activate_SetUINT32(IMFActivate *iface, REFGUID guidKey, UINT32 unValue){return IMFAttributes_SetUINT32(((struct activation*)iface)->attributes, guidKey, unValue);}
static HRESULT WINAPI activate_SetUINT64(IMFActivate *iface, REFGUID guidKey, UINT64 unValue){return IMFAttributes_SetUINT64(((struct activation*)iface)->attributes, guidKey, unValue);}
static HRESULT WINAPI activate_SetDouble(IMFActivate *iface, REFGUID guidKey, double fValue){return IMFAttributes_SetDouble(((struct activation*)iface)->attributes, guidKey, fValue);}
static HRESULT WINAPI activate_SetGUID(IMFActivate *iface, REFGUID guidKey, REFGUID guidValue){return IMFAttributes_SetGUID(((struct activation*)iface)->attributes, guidKey, guidValue);}
static HRESULT WINAPI activate_SetString(IMFActivate *iface, REFGUID guidKey, LPCWSTR wszValue){return IMFAttributes_SetString(((struct activation*)iface)->attributes, guidKey, wszValue);}
static HRESULT WINAPI activate_SetBlob(IMFActivate *iface, REFGUID guidKey, const UINT8 *pBuf, UINT32 cbBufSize){return IMFAttributes_SetBlob(((struct activation*)iface)->attributes, guidKey, pBuf, cbBufSize);}
static HRESULT WINAPI activate_SetUnknown(IMFActivate *iface, REFGUID guidKey, IUnknown *pUnknown){return IMFAttributes_SetUnknown(((struct activation*)iface)->attributes, guidKey, pUnknown);}
static HRESULT WINAPI activate_LockStore(IMFActivate *iface){return IMFAttributes_LockStore(((struct activation*)iface)->attributes);}
static HRESULT WINAPI activate_UnlockStore(IMFActivate *iface){return IMFAttributes_UnlockStore(((struct activation*)iface)->attributes);}
static HRESULT WINAPI activate_GetCount(IMFActivate *iface, UINT32 *pcItems){return IMFAttributes_GetCount(((struct activation*)iface)->attributes, pcItems);}
static HRESULT WINAPI activate_GetItemByIndex(IMFActivate *iface, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue){return IMFAttributes_GetItemByIndex(((struct activation*)iface)->attributes, unIndex, pguidKey, pValue);}
static HRESULT WINAPI activate_CopyAllItems(IMFActivate *iface, IMFAttributes *pDest){return IMFAttributes_CopyAllItems(((struct activation*)iface)->attributes, pDest);}
