"""Generate the platform-independent Android UI dictionary; validate every language."""
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parent
def generate():
 data=json.loads((ROOT/'translations.json').read_text(encoding='utf-8'));rows=[]
 for key,values in data['strings'].items():
  if len(values)!=5 or any(not value.strip() for value in values):raise ValueError('Missing translation: '+key)
  # Keep printf arguments intact in translated UI messages.
  formats=lambda text:re.findall(r'%(?:\d+\$)?[.\d]*[sdf]',text)
  for value in values:
   if formats(value)!=formats(key):raise ValueError('Format arguments differ: '+key)
  rows.append('        entries.put('+json.dumps(key,ensure_ascii=False)+',new String[]{'+','.join(json.dumps(v,ensure_ascii=False) for v in values)+'});')
 source='''package ru.codex.obslancamera;
import java.util.*;
/** Generated from android/translations.json. No Android dependency: used by regression probes too. */
public final class Language {
    private static volatile int selected=languageIndex(Locale.getDefault().getLanguage());
    private static final Map<String,String[]> messages;
    static {Map<String,String[]> entries=new HashMap<>();
'''+"\n".join(rows)+'''
        messages=Collections.unmodifiableMap(entries);
    }
    private static int languageIndex(String code){if(code==null)return 0;code=code.toLowerCase(Locale.ROOT).split("[-_]")[0];return code.equals("ru")?1:code.equals("ja")?2:code.equals("es")?3:code.equals("zh")?4:0;}
    public static void set(String code){selected=languageIndex(code);}
    public static String code(){return new String[]{"en","ru","ja","es","zh"}[selected];}
    public static String[] names(){return new String[]{"English","Русский","日本語","Español","简体中文"};}
    public static String text(String key){String[] translated=messages.get(key);return translated==null?key:translated[selected];}
}
'''
 (ROOT/'src/ru/codex/obslancamera/Language.java').write_text(source,encoding='utf-8')
if __name__=='__main__':generate()
