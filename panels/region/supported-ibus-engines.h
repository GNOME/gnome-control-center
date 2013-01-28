static const gchar *supported_ibus_engines[] = {
  /* Simplified Chinese */
  "pinyin",
  "bopomofo",
  "wubi",
  "erbi",
  /* Default in Fedora, where ibus-libpinyin replaces ibus-pinyin */
  "libpinyin",
  "libbopomofo",

  /* Traditional Chinese */
  /* https://bugzilla.gnome.org/show_bug.cgi?id=680840 */
  "chewing",
  "cangjie5",
  "cangjie3",
  "quick5",
  "quick3",
  "stroke5",

  /* Japanese */
  "anthy",
  "mozc-jp",
  "skk",

  /* Korean */
  "hangul",

  /* Thai */
  "m17n:th:kesmanee",
  "m17n:th:pattachote",
  "m17n:th:tis820",

  /* Vietnamese */
  "m17n:vi:tcvn",
  "m17n:vi:telex",
  "m17n:vi:viqr",
  "m17n:vi:vni",
  "Unikey",

  /* Sinhala */
  "m17n:si:wijesekera",
  "m17n:si:phonetic-dynamic",
  "m17n:si:trans",
  "sayura",

  /* Indic */
  /* https://fedoraproject.org/wiki/I18N/Indic#Keyboard_Layouts */

  /* Assamese */
  "m17n:as:phonetic",
  "m17n:as:inscript",
  "m17n:as:itrans",

  /* Bengali */
  "m17n:bn:inscript",
  "m17n:bn:itrans",
  "m17n:bn:probhat",

  /* Gujarati */
  "m17n:gu:inscript",
  "m17n:gu:itrans",
  "m17n:gu:phonetic",

  /* Hindi */
  "m17n:hi:inscript",
  "m17n:hi:itrans",
  "m17n:hi:phonetic",
  "m17n:hi:remington",
  "m17n:hi:typewriter",
  "m17n:hi:vedmata",

  /* Kannada */
  "m17n:kn:kgp",
  "m17n:kn:inscript",
  "m17n:kn:itrans",

  /* Kashmiri */
  "m17n:ks:inscript",

  /* Maithili */
  "m17n:mai:inscript",

  /* Malayalam */
  "m17n:ml:inscript",
  "m17n:ml:itrans",
  "m17n:ml:mozhi",
  "m17n:ml:swanalekha",

  /* Marathi */
  "m17n:mr:inscript",
  "m17n:mr:itrans",
  "m17n:mr:phonetic",

  /* Nepali */
  "m17n:ne:rom",
  "m17n:ne:trad",

  /* Oriya */
  "m17n:or:inscript",
  "m17n:or:itrans",
  "m17n:or:phonetic",

  /* Punjabi */
  "m17n:pa:inscript",
  "m17n:pa:itrans",
  "m17n:pa:phonetic",
  "m17n:pa:jhelum",

  /* Sanskrit */
  "m17n:sa:harvard-kyoto",

  /* Sindhi */
  "m17n:sd:inscript",

  /* Tamil */
  "m17n:ta:tamil99",
  "m17n:ta:inscript",
  "m17n:ta:itrans",
  "m17n:ta:phonetic",
  "m17n:ta:lk-renganathan",
  "m17n:ta:vutam",
  "m17n:ta:typewriter",

  /* Telugu */
  "m17n:te:inscript",
  "m17n:te:apple",
  "m17n:te:pothana",
  "m17n:te:rts",

  /* Urdu */
  "m17n:ur:phonetic",

  /* Inscript2 - https://bugzilla.gnome.org/show_bug.cgi?id=684854 */
  "m17n:as:inscript2",
  "m17n:bn:inscript2",
  "m17n:brx:inscript2-deva",
  "m17n:doi:inscript2-deva",
  "m17n:gu:inscript2",
  "m17n:hi:inscript2",
  "m17n:kn:inscript2",
  "m17n:kok:inscript2-deva",
  "m17n:mai:inscript2",
  "m17n:ml:inscript2",
  "m17n:mni:inscript2-beng",
  "m17n:mni:inscript2-mtei",
  "m17n:mr:inscript2",
  "m17n:ne:inscript2-deva",
  "m17n:or:inscript2",
  "m17n:pa:inscript2-guru",
  "m17n:sa:inscript2",
  "m17n:sat:inscript2-deva",
  "m17n:sat:inscript2-olck",
  "m17n:sd:inscript2-deva",
  "m17n:ta:inscript2",
  "m17n:te:inscript2",

  /* No corresponding XKB map available for the languages */

  /* Chinese Yi */
  "m17n:ii:phonetic",

  /* Tai-Viet */
  "m17n:tai:sonla",

  /* Kazakh in Arabic script */
  "m17n:kk:arabic",

  /* Yiddish */
  "m17n:yi:yivo",

  /* Canadian Aboriginal languages */
  "m17n:ath:phonetic",
  "m17n:bla:phonetic",
  "m17n:cr:western",
  "m17n:iu:phonetic",
  "m17n:nsk:phonetic",
  "m17n:oj:phonetic",

  /* Non-trivial engines, like transliteration-based instead of
     keymap-based.  Confirmation needed that the engines below are
     actually used by local language users. */

  /* Tibetan */
  "m17n:bo:ewts",
  "m17n:bo:tcrc",
  "m17n:bo:wylie",

  /* Esperanto */
  "m17n:eo:h-f",
  "m17n:eo:h",
  "m17n:eo:plena",
  "m17n:eo:q",
  "m17n:eo:vi",
  "m17n:eo:x",

  /* Amharic */
  "m17n:am:sera",

  /* Russian */
  "m17n:ru:translit",

  /* Classical Greek */
  "m17n:grc:mizuochi",

  /* Lao */
  "m17n:lo:lrt",

  /* Postfix modifier input methods */
  "m17n:da:post",
  "m17n:sv:post",
  NULL
};
